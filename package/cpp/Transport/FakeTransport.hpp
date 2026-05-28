#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "Transport/Transport.hpp"

namespace margelo::nitro::ecr17 {

// In-memory Transport for unit tests. Deterministic and synchronous:
//
//  - `enqueueResponse(bytes)` queues a scripted terminal reply.
//  - Every time the session sends an APPLICATION request (a frame starting with
//    STX, i.e. an initial send or a retransmit) the next queued response is
//    delivered synchronously via the data callback. Control sends (ACK/NAK,
//    which start with 0x06/0x15) are just recorded and never trigger a reply.
//
// This lets a test script "ACK + result", "NAK then (on retransmit) ACK+result",
// progress/receipt streams, or no reply at all (to exercise timeouts), without
// any real sockets or threads.
class FakeTransport : public Transport {
   public:
    static constexpr uint8_t STX = 0x02;

    void connect() override { connected_ = true; }
    void disconnect() override { connected_ = false; }
    bool isConnected() const override { return connected_; }

    void send(const std::vector<uint8_t>& bytes) override {
        sent_.push_back(bytes);
        const bool isApplicationRequest = !bytes.empty() && bytes.front() == STX;
        if (isApplicationRequest && disconnectOnRequest_) {
            triggerDisconnect();
            return;
        }
        if (isApplicationRequest && !responses_.empty()) {
            std::vector<uint8_t> next = std::move(responses_.front());
            responses_.erase(responses_.begin());
            if (dataCallback_) {
                dataCallback_(next);
            }
        }
    }

    void setDataCallback(DataCallback cb) override { dataCallback_ = std::move(cb); }
    void setDisconnectCallback(DisconnectCallback cb) override { disconnectCallback_ = std::move(cb); }

    // --- Test helpers ---
    void enqueueResponse(std::vector<uint8_t> bytes) { responses_.push_back(std::move(bytes)); }

    // Make the next application-request send drop the connection instead of replying.
    void disconnectOnNextRequest() { disconnectOnRequest_ = true; }

    // Deliver bytes immediately as if received from the terminal.
    void pushIncoming(const std::vector<uint8_t>& bytes) {
        if (dataCallback_) {
            dataCallback_(bytes);
        }
    }

    void triggerDisconnect() {
        connected_ = false;
        if (disconnectCallback_) {
            disconnectCallback_();
        }
    }

    const std::vector<std::vector<uint8_t>>& sentFrames() const { return sent_; }
    size_t applicationRequestCount() const {
        size_t count = 0;
        for (const auto& f : sent_) {
            if (!f.empty() && f.front() == STX) {
                ++count;
            }
        }
        return count;
    }

   private:
    bool connected_ = false;
    DataCallback dataCallback_{};
    DisconnectCallback disconnectCallback_{};
    std::vector<std::vector<uint8_t>> sent_{};
    std::vector<std::vector<uint8_t>> responses_{};
    bool disconnectOnRequest_ = false;
};

}  // namespace margelo::nitro::ecr17
