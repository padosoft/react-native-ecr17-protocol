#pragma once

#include <Ecr17Kit/Transport.hpp>

#include <memory>
#include <vector>

#include "HybridEcr17TransportSpec.hpp"  // generated Nitro spec (Swift/Kotlin/Winsock impl)

namespace margelo::nitro::ecr17 {

// Adapts the native Nitro transport (the Ecr17Transport HybridObject: Swift on iOS,
// Kotlin on Android, the Kit's WinsockTransport on Windows) to the Kit's Transport
// interface. Converts between std::vector<uint8_t> and Nitro's ArrayBuffer, and
// turns the spec's async connect() into the blocking connect the Kit client expects.
class NativeTransportAdapter final : public padosoft::ecr17::Transport {
   public:
    explicit NativeTransportAdapter(std::shared_ptr<HybridEcr17TransportSpec> transport);

    void connect(const padosoft::ecr17::Endpoint& endpoint) override;
    void disconnect() override;
    bool isConnected() override;
    void send(const std::vector<uint8_t>& bytes) override;
    void setDataCallback(padosoft::ecr17::DataCallback cb) override;
    void setDisconnectCallback(padosoft::ecr17::DisconnectCallback cb) override;

   private:
    std::shared_ptr<HybridEcr17TransportSpec> transport_;
};

}  // namespace margelo::nitro::ecr17
