#include "Ecr17Windows.hpp"

#include <NitroModules/HybridObjectRegistry.hpp>

#include <memory>
#include <mutex>

#include "HybridEcr17Client.hpp"
#include "HybridEcr17TransportWindows.hpp"

namespace margelo::nitro::ecr17 {

void registerEcr17HybridObjects() {
    static std::once_flag once;
    std::call_once(once, []() {
        HybridObjectRegistry::registerHybridObjectConstructor(
            "Ecr17Client", []() -> std::shared_ptr<HybridObject> { return std::make_shared<HybridEcr17Client>(); });
        HybridObjectRegistry::registerHybridObjectConstructor("Ecr17Transport", []() -> std::shared_ptr<HybridObject> {
            return std::make_shared<HybridEcr17TransportWindows>();
        });
    });
}

}  // namespace margelo::nitro::ecr17
