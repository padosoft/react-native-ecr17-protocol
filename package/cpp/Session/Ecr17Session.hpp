#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Lcr/Lcr.hpp"  // brings LrcMode
#include "PacketCodec/PacketCodec.hpp"
#include "Transport/Transport.hpp"

namespace margelo::nitro::ecr17 {

struct SessionConfig {
    LrcMode lrcMode = LrcMode::STD;
    int ackTimeoutMs = 2000;       // wait for the physical ACK/NAK
    int responseTimeoutMs = 60000; // wait for the application response
    int retryCount = 3;            // retransmissions on NAK/timeout (spec: up to 3)
    int retryDelayMs = 200;        // delay between retransmissions
    int receiptDrainMs = 0;        // after the result, keep forwarding 'S' receipt
                                   // lines until this many ms of silence (0 = off)
};

// Drives one ECR17 request/response exchange over a Transport: frames the
// request, handles the physical ACK/NAK handshake with retransmission, waits for
// the application response while forwarding progress (SOH) and receipt ('S')
// messages, and ACK/NAKs incoming frames per their LRC validity.
//
// Pure C++ and transport-agnostic: unit-tested against FakeTransport. A single
// exchange runs at a time (the protocol is one transaction per terminal).
class Ecr17Session {
   public:
    Ecr17Session(Transport& transport, const SessionConfig& config);

    void setOnProgress(std::function<void(const std::string&)> cb) { onProgress_ = std::move(cb); }
    void setOnReceiptLine(std::function<void(const std::string&)> cb) {
        onReceiptLine_ = std::move(cb);
    }

    // Sends `requestPayload` (the application message, without STX/ETX) and
    // returns the decoded application result. Throws std::runtime_error on
    // retransmission exhaustion, ACK/response timeout, or transport disconnect.
    DecodedPacket exchange(const std::string& requestPayload);

    // Like exchange(), but sends an extra additional-data message (command 'U',
    // tokenization) after the main request is ACKed, before the result: the
    // documented flow is request(flag=1) -> ACK -> 'U' -> ACK -> result.
    DecodedPacket exchangeWithAdditionalData(const std::string& requestPayload,
                                             const std::string& additionalPayload);

    // For commands whose only reply is the physical ACK (e.g. enable/disable ECR
    // printing 'E'). Performs the ACK handshake with retransmission and returns
    // once ACK is received; does NOT wait for an application response. Throws on
    // retransmission exhaustion / timeout / disconnect.
    void sendAckOnly(const std::string& requestPayload);

   private:
    void onData(const std::vector<uint8_t>& data);
    void onDisconnect();
    // Clears stale RX bytes and the disconnected flag so the session is reusable
    // across reconnects (a new transaction starts from a clean state).
    void resetForNewTransaction();
    // Sends a request and completes the physical ACK handshake (with
    // retransmission). Does NOT reset state — callers reset once per transaction.
    void ackHandshake(const std::string& requestPayload);
    // Waits for the application result after the ACK handshake, forwarding
    // progress (SOH) and receipt ('S') frames, NAKing invalid-LRC frames.
    DecodedPacket waitForResult();
    void drainReceipts();
    std::optional<std::vector<uint8_t>> extractFrameLocked();
    std::optional<DecodedPacket> waitForFrame(int timeoutMs);
    void sendControl(uint8_t control);
    static bool isReceipt(const std::string& payload);

    Transport& transport_;
    SessionConfig config_;
    PacketCodec codec_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<uint8_t> rxBuffer_;
    bool disconnected_ = false;

    std::function<void(const std::string&)> onProgress_{};
    std::function<void(const std::string&)> onReceiptLine_{};
};

}  // namespace margelo::nitro::ecr17
