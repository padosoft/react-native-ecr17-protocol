---
title: Installation
description: Install the React Native package and native build prerequisites.
---

# Installation

The module is built with Nitro Modules and requires React Native new architecture support.

## Requirements

- React Native 0.76 or newer.
- `react-native-nitro-modules` installed as a peer dependency.
- `@padosoft/ecr17-kit`, the C++ protocol library (a dependency), and `@padosoft/native-modules`, the native build helpers (a peer dependency for iOS and Android). ⚠️ They are not on npm yet.
- iOS, Android or Windows native build environments for the target app (Windows: React Native Windows 0.84+, see [Windows](/windows)).
- A Nexi Group ECR17-compatible terminal configured for LAN integration.

## Package install

```bash
npm install @padosoft/react-native-ecr17 @padosoft/ecr17-kit @padosoft/native-modules react-native-nitro-modules
```

For Bun users:

```bash
bun add @padosoft/react-native-ecr17 @padosoft/ecr17-kit @padosoft/native-modules react-native-nitro-modules
```

## iOS

The protocol library is its own pod, `Ecr17Kit`. With Expo, add its config plugin to `app.json`, then prebuild:

```json
{ "expo": { "plugins": ["@padosoft/ecr17-kit"] } }
```

In a bare app, keeping `@padosoft/ecr17-kit` among the direct dependencies is enough: autolinking finds `Ecr17Kit.podspec`.

```bash
cd ios
pod install
```

The iOS transport uses Swift and Network.framework. Rebuild the app after changing native dependencies.

## Android

The Android transport uses Kotlin TCP sockets and the Nitro-generated JNI bridge. A full native rebuild is required after installing the package.

::: callout info "Autolinking"
The package ships `react-native.config.js` so React Native autolinking can register the Android package and load the native library before JavaScript creates the HybridObject.
:::

## Windows

React Native Windows (New Architecture) needs one extra line in the app's `react-native.config.js`, because `react-native-nitro-modules` has no Windows project and this package's DLL installs Nitro instead:

```js
const { windowsAppDependencies } = require("@padosoft/react-native-ecr17/windows-autolink");

module.exports = {
  dependencies: windowsAppDependencies(),
};
```

Requirements, the Winsock transport and how Nitro is installed are covered in [Windows](/windows).

## Example app

The repository includes an Expo-based debug console in `example/` for exercising commands and live logs against a real terminal. `example-windows/` is a smaller console for React Native Windows.
