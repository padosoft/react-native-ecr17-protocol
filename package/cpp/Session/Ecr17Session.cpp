#include "Session/Ecr17Session.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace margelo::nitro::ecr17 {

using clock = std::chrono::steady_clock;

Ecr17Session::Ecr17Session(Transport& transport, const SessionConfig& config)
    : transport_(transport), config_(config), codec_(config.lrcMode) {
    transport_.setDataCallback([this](const std::vector<uint8_t>& data) { onData(data); });
    transport_.setDisconnectCallback([this]() { onDisconnect(); });
}

void Ecr17Session::onData(const std::vector<uint8_t>& data) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rxBuffer_.insert(rxBuffer_.end(), data.begin(), data.end());
    }
    cv_.notify_all();
}

void Ecr17Session::onDisconnect() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        disconnected_ = true;
    }
    cv_.notify_all();
}

// Extracts one complete frame from the front of rxBuffer_, dropping leading junk
// bytes to resynchronise. Returns nullopt if no complete frame is available yet.
// Caller must hold mutex_.
std::optional<std::vector<uint8_t>> Ecr17Session::extractFrameLocked() {
    while (!rxBuffer_.empty()) {
        const uint8_t first = rxBuffer_.front();

        if (first == PacketCodec::ACK || first == PacketCodec::NAK) {
            if (rxBuffer_.size() < 3) {
                return std::nullopt;  // wait for ETX + LRC
            }
            std::vector<uint8_t> frame(rxBuffer_.begin(), rxBuffer_.begin() + 3);
            rxBuffer_.erase(rxBuffer_.begin(), rxBuffer_.begin() + 3);
            return frame;
        }

        if (first == PacketCodec::STX) {
            auto etx = std::find(rxBuffer_.begin(), rxBuffer_.end(), PacketCodec::ETX);
            if (etx == rxBuffer_.end() || etx + 1 == rxBuffer_.end()) {
                return std::nullopt;  // wait for ETX and the trailing LRC
            }
            auto lastByte = etx + 1;  // LRC
            std::vector<uint8_t> frame(rxBuffer_.begin(), lastByte + 1);
            rxBuffer_.erase(rxBuffer_.begin(), lastByte + 1);
            return frame;
        }

        if (first == PacketCodec::SOH) {
            auto eot = std::find(rxBuffer_.begin(), rxBuffer_.end(), PacketCodec::EOT);
            if (eot == rxBuffer_.end()) {
                return std::nullopt;  // wait for EOT
            }
            std::vector<uint8_t> frame(rxBuffer_.begin(), eot + 1);
            rxBuffer_.erase(rxBuffer_.begin(), eot + 1);
            return frame;
        }

        // Unrecognised lead byte: drop it and resynchronise.
        rxBuffer_.erase(rxBuffer_.begin());
    }
    return std::nullopt;
}

std::optional<DecodedPacket> Ecr17Session::waitForFrame(int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs);
    while (true) {
        if (auto frame = extractFrameLocked()) {
            return codec_.decode(*frame);
        }
        if (disconnected_) {
            throw std::runtime_error("ECR17: transport disconnected during exchange");
        }
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            if (auto frame = extractFrameLocked()) {
                return codec_.decode(*frame);
            }
            return std::nullopt;
        }
    }
}

void Ecr17Session::sendControl(uint8_t control) {
    transport_.send(codec_.encodeControl(control));
}

bool Ecr17Session::isReceipt(const std::string& payload) {
    // Send-ticket message from the terminal uses message code 'S' at position 10.
    return payload.size() >= 10 && payload[9] == 'S';
}

void Ecr17Session::resetForNewTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    disconnected_ = false;
    rxBuffer_.clear();
    pendingResult_.reset();
}

void Ecr17Session::sendAckOnly(const std::string& requestPayload) {
    resetForNewTransaction();
    ackHandshake(requestPayload);
}

void Ecr17Session::ackHandshake(const std::string& requestPayload) {
    const std::vector<uint8_t> requestFrame = codec_.encodeApplication(requestPayload);

    transport_.send(requestFrame);
    int attempts = 1;
    auto deadline = clock::now() + std::chrono::milliseconds(config_.ackTimeoutMs);

    while (true) {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()).count();
        if (remaining <= 0) {
            if (attempts > config_.retryCount) {
                throw std::runtime_error("ECR17: no ACK after " + std::to_string(attempts) +
                                         " attempts");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.retryDelayMs));
            transport_.send(requestFrame);
            ++attempts;
            deadline = clock::now() + std::chrono::milliseconds(config_.ackTimeoutMs);
            continue;
        }

        std::optional<DecodedPacket> pkt = waitForFrame(static_cast<int>(remaining));
        if (!pkt) {
            continue;
        }
        if (pkt->type == PacketType::ACK) {
            return;
        }
        if (pkt->type == PacketType::NAK) {
            if (attempts > config_.retryCount) {
                throw std::runtime_error("ECR17: NAK after " + std::to_string(attempts) +
                                         " attempts");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.retryDelayMs));
            transport_.send(requestFrame);
            ++attempts;
            deadline = clock::now() + std::chrono::milliseconds(config_.ackTimeoutMs);
            continue;
        }
        if (pkt->type == PacketType::APPLICATION) {
            // The terminal sent the application response without (or before) a
            // physical ACK. The request was clearly received, so treat the
            // handshake as satisfied and stash the frame for waitForResult() to
            // validate/ACK — dropping it would lose a completed transaction.
            pendingResult_ = pkt;
            return;
        }
        // Ignore any progress frames that may precede the ACK.
    }
}

DecodedPacket Ecr17Session::exchange(const std::string& requestPayload) {
    resetForNewTransaction();     // start clean (reusable across reconnects)
    ackHandshake(requestPayload);  // send + physical ACK handshake (with retransmission)
    return waitForResult();
}

DecodedPacket Ecr17Session::exchangeWithAdditionalData(const std::string& requestPayload,
                                                       const std::string& additionalPayload) {
    resetForNewTransaction();
    ackHandshake(requestPayload);     // main request -> ACK
    ackHandshake(additionalPayload);  // 'U' additional-data message -> ACK
    return waitForResult();
}

DecodedPacket Ecr17Session::waitForResult() {
    auto deadline = clock::now() + std::chrono::milliseconds(config_.responseTimeoutMs);
    while (true) {
        std::optional<DecodedPacket> pkt;
        if (pendingResult_) {
            // A frame the ACK handshake received early — process it first.
            pkt = std::move(pendingResult_);
            pendingResult_.reset();
        } else {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now())
                    .count();
            if (remaining <= 0) {
                throw std::runtime_error("ECR17: no application response before timeout");
            }
            pkt = waitForFrame(static_cast<int>(remaining));
            if (!pkt) {
                continue;
            }
        }
        switch (pkt->type) {
            case PacketType::PROGRESS:
                if (onProgress_) onProgress_(pkt->payload);
                break;
            case PacketType::APPLICATION:
                if (!pkt->validLrc) {
                    sendControl(PacketCodec::NAK);
                    break;
                }
                sendControl(PacketCodec::ACK);
                if (isReceipt(pkt->payload)) {
                    if (onReceiptLine_) onReceiptLine_(pkt->payload);
                    break;
                }
                drainReceipts();  // forward receipts that follow the result (if enabled)
                return *pkt;
            case PacketType::ACK:
            case PacketType::NAK:
                break;  // stray confirmation; ignore
            case PacketType::UNKNOWN:
                sendControl(PacketCodec::NAK);
                break;
        }
    }
}

void Ecr17Session::drainReceipts() {
    if (config_.receiptDrainMs <= 0) {
        return;
    }
    // Keep forwarding 'S' receipt lines that arrive after the result until the
    // terminal goes quiet for receiptDrainMs.
    while (true) {
        std::optional<DecodedPacket> pkt = waitForFrame(config_.receiptDrainMs);
        if (!pkt) {
            return;  // idle: no more receipts
        }
        switch (pkt->type) {
            case PacketType::APPLICATION:
                if (pkt->validLrc) {
                    sendControl(PacketCodec::ACK);
                    if (isReceipt(pkt->payload) && onReceiptLine_) {
                        onReceiptLine_(pkt->payload);
                    }
                } else {
                    sendControl(PacketCodec::NAK);  // request retransmit, like waitForResult
                }
                break;
            case PacketType::PROGRESS:
                if (onProgress_) onProgress_(pkt->payload);
                break;
            default:
                break;
        }
    }
}

}  // namespace margelo::nitro::ecr17
