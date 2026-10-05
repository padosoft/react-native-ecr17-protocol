#include "Transport/NativeTransportAdapter.hpp"

#include <NitroModules/ArrayBuffer.hpp>

#include <stdexcept>
#include <utility>

namespace margelo::nitro::ecr17 {

NativeTransportAdapter::NativeTransportAdapter(std::shared_ptr<HybridEcr17TransportSpec> transport)
    : transport_(std::move(transport)) {
    if (!transport_) {
        throw std::invalid_argument("ECR17: NativeTransportAdapter needs a transport");
    }
}

void NativeTransportAdapter::connect(const padosoft::ecr17::Endpoint& endpoint) {
    // Blocks the calling (worker) thread until the native transport connects;
    // the transport's own thread resolves the promise, so this never waits on itself.
    transport_->connect(endpoint.host, endpoint.port, endpoint.timeoutMs)->await().get();
}

void NativeTransportAdapter::disconnect() { transport_->disconnect(); }

bool NativeTransportAdapter::isConnected() { return transport_->isConnected(); }

void NativeTransportAdapter::send(const std::vector<uint8_t>& bytes) {
    transport_->send(ArrayBuffer::copy(bytes));
}

void NativeTransportAdapter::setDataCallback(padosoft::ecr17::DataCallback cb) {
    // Always hand Nitro a callable: a cleared callback becomes a no-op, never an
    // empty std::function crossing into Swift/Kotlin.
    transport_->setOnData([cb = std::move(cb)](const std::shared_ptr<ArrayBuffer>& buffer) {
        if (!cb || buffer == nullptr) {
            return;
        }
        const uint8_t* data = buffer->data();
        if (data == nullptr) {
            return;
        }
        cb(std::vector<uint8_t>(data, data + buffer->size()));
    });
}

void NativeTransportAdapter::setDisconnectCallback(padosoft::ecr17::DisconnectCallback cb) {
    transport_->setOnDisconnect([cb = std::move(cb)]() {
        if (cb) {
            cb();
        }
    });
}

}  // namespace margelo::nitro::ecr17
