// No pch.h: plain Winsock + Nitro (no WinRT).
#include "HybridEcr17TransportWindows.hpp"

#include <NitroModules/ArrayBuffer.hpp>

#include <algorithm>
#include <climits>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace margelo::nitro::ecr17 {

std::shared_ptr<Promise<void>> HybridEcr17TransportWindows::connect(const std::string& host, double port,
                                                                    double timeoutMs) {
    auto transport = transport_;
    auto promise = Promise<void>::create();
    // A dedicated thread, not Promise::async: the client already blocks a Nitro
    // ThreadPool worker on this promise. Queued on the same pool while every worker
    // is busy (a long payment exchange), the connect would wait for a free worker
    // and connectionTimeoutMs would no longer bound it.
    std::thread([transport, promise, host, port, timeoutMs]() {
        try {
            // NaN or out of range: rejected here instead of an undefined cast.
            if (!(port >= 1 && port <= 65535)) {
                throw std::invalid_argument("ECR17 transport: port must be between 1 and 65535");
            }
            // <= 0 (or NaN) waits indefinitely, like java.net.Socket.connect(addr, 0).
            const int timeout =
                timeoutMs > 0 ? static_cast<int>(std::min(timeoutMs, static_cast<double>(INT_MAX))) : 0;
            transport->connect(padosoft::ecr17::Endpoint{host, static_cast<int>(port), timeout});
            promise->resolve();
        } catch (...) {
            promise->reject(std::current_exception());
        }
    }).detach();
    return promise;
}

void HybridEcr17TransportWindows::disconnect() { transport_->disconnect(); }

bool HybridEcr17TransportWindows::isConnected() { return transport_->isConnected(); }

void HybridEcr17TransportWindows::send(const std::shared_ptr<ArrayBuffer>& bytes) {
    if (bytes == nullptr) {
        throw std::invalid_argument("ECR17 transport: send() needs a buffer");
    }
    const uint8_t* data = bytes->data();
    transport_->send(std::vector<uint8_t>(data, data + bytes->size()));
}

void HybridEcr17TransportWindows::setOnData(const std::function<void(const std::shared_ptr<ArrayBuffer>&)>& callback) {
    if (!callback) {
        transport_->setDataCallback(nullptr);
        return;
    }
    transport_->setDataCallback(
        [callback](const std::vector<uint8_t>& bytes) { callback(ArrayBuffer::copy(bytes)); });
}

void HybridEcr17TransportWindows::setOnDisconnect(const std::function<void()>& callback) {
    transport_->setDisconnectCallback(callback);
}

}  // namespace margelo::nitro::ecr17
