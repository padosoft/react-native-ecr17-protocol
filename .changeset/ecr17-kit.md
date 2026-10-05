---
"@padosoft/react-native-ecr17": major
---

The protocol engine moved into a new React-free C++ package, `@padosoft/ecr17-kit`, usable from native apps too. The binding now maps the JS API onto the Kit's `Ecr17Client`, which owns auto-connect, the pre-send liveness probe and the money-safe retry policy (now unit-tested for every command).

Breaking: apps install `@padosoft/ecr17-kit` and `@padosoft/native-modules` (the podspec and `android/build.gradle` now come from its `nitro_module` / `nitro-module.gradle` helpers), and on iOS the `Ecr17Kit` pod must be in the Podfile: add the Kit's Expo config plugin (`"plugins": ["@padosoft/ecr17-kit"]`) or list the Kit as a direct dependency. Both `@padosoft` packages are not on npm yet.
