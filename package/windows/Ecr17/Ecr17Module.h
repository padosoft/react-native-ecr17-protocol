#pragma once

#include "pch.h"
#include "resource.h"

#include "NativeModules.h"

#include <optional>
#include <string>

namespace winrt::Ecr17
{

// react-native-nitro-modules has no Windows project
// (https://github.com/mrousavy/nitro/issues/168), so this DLL provides the
// `NitroModules` TurboModule its JS expects
// (`TurboModuleRegistry.getEnforcing('NitroModules').install()`). install()
// installs Nitro into the JS runtime and registers this package's
// HybridObjects (`Ecr17Client`, `Ecr17Transport`).
//
// Only one native module per app may provide `NitroModules`. Another Nitro
// library that ships the same shim would conflict with this one.
REACT_MODULE(NitroModules, L"NitroModules")
struct NitroModules
{
  REACT_INIT(Initialize)
  void Initialize(React::ReactContext const &reactContext) noexcept;

  REACT_SYNC_METHOD(install)
  std::optional<std::string> install() noexcept;

private:
  React::ReactContext m_context;
};

} // namespace winrt::Ecr17
