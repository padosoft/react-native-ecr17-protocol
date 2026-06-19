---
title: Installation
description: Install the React Native package and native build prerequisites.
---

# Installation

The module is built with Nitro Modules and requires React Native new architecture support.

## Requirements

- React Native 0.76 or newer.
- `react-native-nitro-modules` installed as a peer dependency.
- iOS and Android native build environments for the target app.
- A Nexi Group ECR17-compatible terminal configured for LAN integration.

## Package install

```bash
npm install react-native-ecr17 react-native-nitro-modules
```

For Bun users:

```bash
bun add react-native-ecr17 react-native-nitro-modules
```

## iOS

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

## Example app

The repository includes an Expo-based debug console in `example/` for exercising commands and live logs against a real terminal.
