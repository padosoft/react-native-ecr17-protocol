#pragma once

#include <Ecr17Kit/WinsockTransport.hpp>

#include <memory>
#include <string>

#include "HybridEcr17TransportSpec.hpp"

namespace margelo::nitro::ecr17 {

// The `Ecr17Transport` HybridObject on Windows: the Kit's WinsockTransport behind
// the Nitro spec (iOS/Android implement the spec in Swift/Kotlin). All the socket
// behaviour — the write-free pre-send probe, one drop signal per connection — lives
// and is tested in the Kit (kit/windows).
class HybridEcr17TransportWindows : public HybridEcr17TransportSpec {
   public:
    HybridEcr17TransportWindows() : HybridObject(TAG) {}

    std::shared_ptr<Promise<void>> connect(const std::string& host, double port, double timeoutMs) override;
    void disconnect() override;
    bool isConnected() override;
    void send(const std::shared_ptr<ArrayBuffer>& bytes) override;
    void setOnData(const std::function<void(const std::shared_ptr<ArrayBuffer>&)>& callback) override;
    void setOnDisconnect(const std::function<void()>& callback) override;

   private:
    // shared: the connect thread may outlive this object.
    std::shared_ptr<padosoft::ecr17::WinsockTransport> transport_ =
        std::make_shared<padosoft::ecr17::WinsockTransport>();
};

}  // namespace margelo::nitro::ecr17
