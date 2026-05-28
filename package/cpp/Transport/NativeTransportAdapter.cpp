#include "Transport/NativeTransportAdapter.hpp"

#include <NitroModules/ArrayBuffer.hpp>

#include <utility>

namespace margelo::nitro::ecr17 {

NativeTransportAdapter::NativeTransportAdapter(std::shared_ptr<HybridEcr17TransportSpec> transport)
    : transport_(std::move(transport)) {}

void NativeTransportAdapter::connect() {
    // Connection is initiated by HybridEcr17Client via the spec's async connect();
    // nothing to do here.
}

void NativeTransportAdapter::disconnect() { transport_->disconnect(); }

bool NativeTransportAdapter::isConnected() const { return transport_->isConnected(); }

void NativeTransportAdapter::send(const std::vector<uint8_t>& bytes) {
    transport_->send(ArrayBuffer::copy(bytes));
}

void NativeTransportAdapter::setDataCallback(DataCallback cb) {
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

void NativeTransportAdapter::setDisconnectCallback(DisconnectCallback cb) {
    transport_->setOnDisconnect(std::move(cb));
}

}  // namespace margelo::nitro::ecr17
