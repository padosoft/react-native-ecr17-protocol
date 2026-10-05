---
title: Installation
description: Install the React Native package and native build prerequisites.
---

# Installation

The module is built with Nitro Modules and requires React Native new architecture support.

## Requirements

- React Native 0.76 or newer.
- `react-native-nitro-modules` installed as a peer dependency.
- `@padosoft/ecr17-kit`, the C++ protocol library, and `@padosoft/native-modules`, the native build helpers, come with the package as dependencies. ⚠️ They are not on npm yet.
- iOS, Android or Windows native build environments for the target app (Windows: React Native Windows 0.84+, see [Windows](/windows)).
- A Nexi Group ECR17-compatible terminal configured for LAN integration.

## Package install

```bash
npm install @padosoft/react-native-ecr17 @padosoft/ecr17-kit react-native-nitro-modules
```

For Bun users:

```bash
bun add @padosoft/react-native-ecr17 @padosoft/ecr17-kit react-native-nitro-modules
```

## iOS

The protocol library is its own pod, `Ecr17Kit`. With Expo, add its config plugin to `app.json` (it uses `@padosoft/expo`, so install that too), then prebuild:

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
