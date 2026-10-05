#include "pch.h"

#include "Ecr17Module.h"

#include "CallInvokerDispatcher.hpp"
#include "HybridEcr17Client.hpp"
#include "HybridEcr17TransportWindows.hpp"
#include "HybridObjectRegistry.hpp"
#include "InstallNitro.hpp"
#include "NitroWindowsPlatform.hpp"

#include <JSI/JsiApiContext.h>

#include <exception>
#include <memory>
#include <mutex>

using namespace winrt::Microsoft::ReactNative;

namespace {

// Registers the same HybridObjects that nitrogen's generated Ecr17OnLoad.cpp
// (Android) / Ecr17Autolinking.mm (iOS) register on the other platforms. On
// Windows the transport is the C++ Winsock implementation instead of Kotlin/Swift.
void registerEcr17HybridObjects() noexcept
{
  using namespace margelo::nitro;
  using namespace margelo::nitro::ecr17;

  static std::once_flag once;
  std::call_once(once, []() {
    HybridObjectRegistry::registerHybridObjectConstructor(
        "Ecr17Client", []() -> std::shared_ptr<HybridObject> { return std::make_shared<HybridEcr17Client>(); });
    HybridObjectRegistry::registerHybridObjectConstructor(
        "Ecr17Transport",
        []() -> std::shared_ptr<HybridObject> { return std::make_shared<HybridEcr17TransportWindows>(); });
  });
}

} // namespace

namespace winrt::Ecr17
{

void NitroModules::Initialize(React::ReactContext const &reactContext) noexcept
{
  m_context = reactContext;
  // The WinRT IReactDispatcher (ReactContext::UIDispatcher() returns the C++ wrapper).
  margelo::nitro::windows::setUIDispatcher(reactContext.Handle().UIDispatcher());
  registerEcr17HybridObjects();
}

std::optional<std::string> NitroModules::install() noexcept
{
  try {
    registerEcr17HybridObjects();

    facebook::jsi::Runtime *runtime = TryGetOrCreateContextRuntime(m_context);
    if (runtime == nullptr) {
      return std::string("No JSI runtime");
    }

    auto callInvoker = m_context.CallInvoker();
    if (callInvoker == nullptr) {
      return std::string("CallInvoker was null");
    }

    auto dispatcher = std::make_shared<margelo::nitro::CallInvokerDispatcher>(callInvoker);
    margelo::nitro::install(*runtime, dispatcher);
    return std::nullopt;
  } catch (const std::exception &exc) {
    return std::string(exc.what());
  } catch (...) {
    return std::string("Failed to install Nitro");
  }
}

} // namespace winrt::Ecr17
