#pragma once

#include <memory>
#include <string>
#include <vector>

#include "HybridEcr17TransportSpec.hpp"  // generated Nitro spec (Swift/Kotlin impl)
#include "Transport/Transport.hpp"

namespace margelo::nitro::ecr17 {

// Adapts the native Nitro transport (Ecr17Transport HybridObject, implemented in
// Swift/Kotlin) to the C++ Transport interface used by Ecr17Session. Converts
// between std::vector<uint8_t> and Nitro's ArrayBuffer. Connection lifecycle is
// driven by HybridEcr17Client directly via the spec (async Promise); the session
// only uses send + the data/disconnect callbacks.
class NativeTransportAdapter : public Transport {
   public:
    explicit NativeTransportAdapter(std::shared_ptr<HybridEcr17TransportSpec> transport);

    void connect() override;      // no-op: client drives connect() via the spec (async)
    void disconnect() override;
    bool isConnected() const override;
    void send(const std::vector<uint8_t>& bytes) override;
    void setDataCallback(DataCallback cb) override;
    void setDisconnectCallback(DisconnectCallback cb) override;

   private:
    std::shared_ptr<HybridEcr17TransportSpec> transport_;
};

}  // namespace margelo::nitro::ecr17
