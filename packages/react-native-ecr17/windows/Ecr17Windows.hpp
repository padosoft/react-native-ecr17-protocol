#pragma once

namespace margelo::nitro::ecr17 {

// Registers this package's HybridObjects for React Native Windows: "Ecr17Client"
// (the shared C++ client) and "Ecr17Transport" (the Kit's WinsockTransport). The
// app's Nitro host (@padosoft/react-native-nitro-windows) calls it once, before
// Nitro is installed; it does the job of nitrogen's OnLoad/Autolinking on Android/iOS.
void registerEcr17HybridObjects();

}  // namespace margelo::nitro::ecr17
