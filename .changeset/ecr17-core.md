---
"@padosoft/react-native-ecr17": minor
---

The protocol engine moved into a new React-free C++ package, `@padosoft/ecr17` (2.0.0), usable from native apps too: CMake (`ecr17::ecr17`), the `Ecr17` pod, and the `Ecr17` Swift package (`Package.swift` at the repository root, tagged `X.Y.Z`). The binding now maps the JS API onto its `Ecr17Client`, which owns auto-connect, the pre-send liveness probe and the money-safe retry policy (now unit-tested for every command). The JS API is unchanged.

The podspec and `android/build.gradle` now come from `@padosoft/native-modules` (`nitro_module`, `nitro-module.gradle`), installed with the package as a dependency. The iOS pod is renamed `ReactNativeEcr17` (it was `Ecr17`, now the core's name). It links the core through `native_dependency`: the `Ecr17` Swift package when the app switches SwiftPM on (`withSwiftPackageManager` from `@padosoft/expo`), else the `Ecr17` pod, which the app declares: Expo apps add the config plugin (`"plugins": ["@padosoft/ecr17"]`), bare apps add `pod 'Ecr17', :path => '../node_modules/@padosoft/ecr17'` to the Podfile.
