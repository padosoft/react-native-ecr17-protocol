#pragma once

#include <winrt/Microsoft.ReactNative.h>

namespace margelo::nitro::windows {

// Stores RNW's UI dispatcher so Nitro's ThreadUtils (createUIThreadDispatcher /
// isUIThread) can use it. Set once from the NitroModules TurboModule.
void setUIDispatcher(winrt::Microsoft::ReactNative::IReactDispatcher const &dispatcher) noexcept;

// The stored UI dispatcher, or null before the TurboModule initialized.
winrt::Microsoft::ReactNative::IReactDispatcher uiDispatcher() noexcept;

} // namespace margelo::nitro::windows
