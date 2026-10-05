// Minimal Nitro platform hooks for the standalone transport test. The real
// DLL uses NitroWindowsPlatform.cpp, which needs React Native Windows (WinRT).
#include "Dispatcher.hpp"
#include "NitroDefines.hpp"
#include "NitroLogger.hpp"
#include "ThreadUtils.hpp"

#include <windows.h>

#include <memory>
#include <string>
#include <utility>

namespace margelo::nitro {

namespace {

class InlineDispatcher final : public Dispatcher {
public:
  void runSync(std::function<void()>&& function) override { function(); }
  void runAsync(std::function<void()>&& function) override { function(); }
};

} // namespace

void Logger::nativeLog(LogLevel, const char* NON_NULL, const std::string&) {}

std::string ThreadUtils::getThreadName() {
  return "test-thread";
}

void ThreadUtils::setThreadName(const std::string&) {}

bool ThreadUtils::isUIThread() {
  return false;
}

std::shared_ptr<Dispatcher> ThreadUtils::createUIThreadDispatcher() {
  return std::make_shared<InlineDispatcher>();
}

} // namespace margelo::nitro
