---
"@padosoft/react-native-ecr17": minor
---

The protocol engine moved into a new React-free C++ package, `@padosoft/ecr17-kit`, usable from native apps too. The binding now maps the JS API onto the Kit's `Ecr17Client`, which owns auto-connect, the pre-send liveness probe and the money-safe retry policy (now unit-tested for every command). The JS API is unchanged.

The podspec and `android/build.gradle` now come from `@padosoft/native-modules` (`nitro_module`, `nitro-module.gradle`), installed with the package as a dependency. On iOS the `Ecr17Kit` pod must be in the Podfile: Expo apps add the Kit's config plugin (`"plugins": ["@padosoft/ecr17-kit"]`, which needs `@padosoft/expo`); bare apps list `@padosoft/ecr17-kit` as a direct dependency so autolinking finds it.
