---
title: Installation
description: Install the React Native package and native build prerequisites.
---

# Installation

The module is built with Nitro Modules and requires React Native new architecture support.

## Requirements

- React Native 0.76 or newer.
- `react-native-nitro-modules` installed as a peer dependency.
- `@padosoft/ecr17`, the C++ protocol library, and `@padosoft/native-modules`, the native build helpers, come with the package as dependencies. ⚠️ They are not on npm yet.
- iOS, Android or Windows native build environments for the target app (Windows: React Native Windows 0.84+, see [Windows](/windows)).
- A Nexi Group ECR17-compatible terminal configured for LAN integration.

## Package install

```bash
npm install @padosoft/react-native-ecr17 @padosoft/ecr17 react-native-nitro-modules
```

For Bun users:

```bash
bun add @padosoft/react-native-ecr17 @padosoft/ecr17 react-native-nitro-modules
```

## iOS

The protocol library is its own pod, `Ecr17`, which the app declares (it is not autolinked). With Expo, add its config plugin to `app.json`, then prebuild:

```json
{ "expo": { "plugins": ["@padosoft/ecr17"] } }
```

In a bare app, add it to the app target in `ios/Podfile`:

```ruby
pod 'Ecr17', :path => '../node_modules/@padosoft/ecr17'
```

```bash
cd ios
pod install
```

The iOS transport uses Swift and Network.framework. Rebuild the app after changing native dependencies.

### Swift Package Manager (optional, experimental)

The binding's pod, `ReactNativeEcr17`, links the core through `native_dependency` from `@padosoft/native-modules`. When the app switches SwiftPM on (`withSwiftPackageManager` from `@padosoft/expo`), it links the `Ecr17` Swift package instead of the pod. The package is `Package.swift` at the repository root, resolved by the `X.Y.Z` tag of `@padosoft/ecr17` (from 2.0.0). The Expo plugin then leaves the `Ecr17` pod out; in a bare app, remove the Podfile line above.

Native iOS and macOS apps can add the same package directly:

```swift
.package(url: "https://github.com/padosoft/react-native-ecr17-protocol.git", from: "2.0.0")
```

## Android

The Android transport uses Kotlin TCP sockets and the Nitro-generated JNI bridge. A full native rebuild is required after installing the package.

::: callout info "Autolinking"
The package ships `react-native.config.js` so React Native autolinking can register the Android package and load the native library before JavaScript creates the HybridObject.
:::

## Windows

React Native Windows (New Architecture) needs the app's Nitro host, `@padosoft/react-native-nitro-windows` (not published yet: link it from a react-native-support checkout), because `react-native-nitro-modules` has no Windows project. In the app's `react-native.config.js`:

```js
const { windowsAppDependencies } = require("@padosoft/react-native-nitro-windows");

module.exports = {
  dependencies: windowsAppDependencies(),
};
```

Requirements, the Winsock transport and how Nitro is installed are covered in [Windows](/windows).

## Example app

The repository includes an Expo-based debug console in `apps/example/` for exercising commands and live logs against a real terminal. `apps/example-windows/` is a smaller console for React Native Windows.
