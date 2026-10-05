// Windows implementations of the per-platform hooks that
// react-native-nitro-modules declares in cpp/platform (NitroLogger.hpp,
// ThreadUtils.hpp). iOS and Android ship their own; Nitro has no Windows port.
#include "pch.h"

#include "NitroWindowsPlatform.hpp"

#include "Dispatcher.hpp"
#include "NitroDefines.hpp"
#include "NitroLogger.hpp"
#include "ThreadUtils.hpp"

#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace margelo::nitro::windows {

namespace {

std::mutex gDispatcherMutex;

// Intentionally leaked: a static WinRT reference would be released during DLL
// unload / static destruction, possibly after the apartment is gone.
// (WinRT projected types delete operator new, hence the wrapper struct.)
struct DispatcherSlot
{
  winrt::Microsoft::ReactNative::IReactDispatcher value{nullptr};
};

winrt::Microsoft::ReactNative::IReactDispatcher &uiDispatcherSlot() noexcept
{
  static auto *slot = new DispatcherSlot();
  return slot->value;
}

} // namespace

winrt::Microsoft::ReactNative::IReactDispatcher uiDispatcher() noexcept
{
  std::lock_guard<std::mutex> lock(gDispatcherMutex);
  return uiDispatcherSlot();
}

void setUIDispatcher(winrt::Microsoft::ReactNative::IReactDispatcher const &dispatcher) noexcept
{
  std::lock_guard<std::mutex> lock(gDispatcherMutex);
  uiDispatcherSlot() = dispatcher;
}

} // namespace margelo::nitro::windows

namespace margelo::nitro {

namespace {

// Posts work to RNW's UI thread (the IReactDispatcher from the ReactContext).
class UIThreadDispatcher final : public Dispatcher {
public:
  void runSync(std::function<void()> &&) override
  {
    throw std::runtime_error("UIThreadDispatcher::runSync() is not implemented on Windows");
  }

  void runAsync(std::function<void()> &&function) override
  {
    auto dispatcher = windows::uiDispatcher();
    if (!dispatcher) {
      throw std::runtime_error("UIThreadDispatcher: the React Native UI dispatcher is not available yet");
    }
    dispatcher.Post([function = std::move(function)]() { function(); });
  }
};

} // namespace

void Logger::nativeLog([[maybe_unused]] LogLevel level, [[maybe_unused]] const char *NON_NULL tag,
                       [[maybe_unused]] const std::string &message)
{
#ifdef NITRO_DEBUG
  std::string line = std::string("[Nitro.") + tag + "] " + message + "\n";
  OutputDebugStringA(line.c_str());
#endif
}

std::string ThreadUtils::getThreadName()
{
  PWSTR description = nullptr;
  if (SUCCEEDED(GetThreadDescription(GetCurrentThread(), &description)) && description != nullptr &&
      description[0] != L'\0') {
    const int size = WideCharToMultiByte(CP_UTF8, 0, description, -1, nullptr, 0, nullptr, nullptr);
    std::string name(static_cast<size_t>(size > 0 ? size - 1 : 0), '\0');
    if (size > 1) {
      WideCharToMultiByte(CP_UTF8, 0, description, -1, name.data(), size, nullptr, nullptr);
    }
    LocalFree(description);
    return name;
  }
  if (description != nullptr) {
    LocalFree(description);
  }

  std::stringstream stream;
  stream << std::this_thread::get_id();
  return std::string("Thread #") + stream.str();
}

void ThreadUtils::setThreadName(const std::string &name)
{
  const int size = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
  if (size <= 0) {
    return;
  }
  std::wstring wide(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wide.data(), size);
  SetThreadDescription(GetCurrentThread(), wide.c_str());
}

bool ThreadUtils::isUIThread()
{
  auto dispatcher = windows::uiDispatcher();
  return dispatcher && dispatcher.HasThreadAccess();
}

std::shared_ptr<Dispatcher> ThreadUtils::createUIThreadDispatcher()
{
  return std::make_shared<UIThreadDispatcher>();
}

} // namespace margelo::nitro
