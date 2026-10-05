---
title: Windows
description: Use the ECR17 client on React Native Windows (New Architecture), how Nitro is installed there, and the Winsock transport.
---

# Windows

The package supports **React Native Windows 0.84+** with the New Architecture
(Fabric / WinAppSDK, `cpp-app` template). The JS API and the C++ protocol core
are the same as on iOS and Android. Only the TCP transport is Windows-specific.

## Requirements

- Windows 10 22H2+ or Windows 11.
- Node.js 22.11 or newer.
- Visual Studio 2022 (MSVC v143) or 2026 (MSVC v145) with the "Desktop development with C++" and "Windows application development" workloads.
- Windows SDK 10.0.26100.
- `react-native-windows` 0.84+ and `react-native-nitro-modules` in the app.

RNW's `rnw-dependencies.ps1` script (in `node_modules/react-native-windows/scripts`) installs missing tools.

## Setup

Install the package and Nitro:

```bash
npm install @padosoft/react-native-ecr17 react-native-nitro-modules
```

Add the Windows autolink helper to the app's `react-native.config.js`:

```js
const { windowsAppDependencies } = require("@padosoft/react-native-ecr17/windows-autolink");

module.exports = {
  dependencies: windowsAppDependencies(),
};
```

Link and build:

```bash
npx react-native autolink-windows
npx react-native run-windows
```

Client code is unchanged:

```ts
import { createEcr17Client } from "@padosoft/react-native-ecr17";

const client = createEcr17Client({
  host: "192.168.1.100",
  port: 10000,
  terminalId: "00000000",
  cashRegisterId: "00000001",
});
```

::: callout info "Why the autolink helper"
`react-native-nitro-modules` does not ship a Windows project yet. The package's own DLL provides the `NitroModules` TurboModule that Nitro's JS calls, installs Nitro into the JS runtime, and registers `Ecr17Client` and `Ecr17Transport`. `windowsAppDependencies()` tells the React Native CLI to skip Nitro's missing Windows project.
:::

## How it works

- `Ecr17.dll` implements `NitroModules.install()`. It runs Nitro's `install()` with RNW's JSI runtime and `CallInvoker`.
- `Ecr17Client` is the shared C++ `HybridEcr17Client`, the same code as on iOS and Android.
- `Ecr17Transport` is `HybridEcr17TransportWindows`, a Winsock TCP transport written in C++ (iOS uses Swift, Android uses Kotlin).
- The DLL compiles Nitro's C++ runtime from the app's `node_modules/react-native-nitro-modules`.
- Before compiling, MSBuild runs `scripts/windows-nitro-shims.mjs`, which writes `<NitroModules/…>` header shims because MSVC has no header map.

Only one native module per app can provide the `NitroModules` TurboModule. Another Nitro library that ships the same shim would conflict.

## Payment safety on Windows

The Winsock transport keeps the same contract as the Kotlin one:

- `isConnected()` is a **write-free, non-destructive** probe (an instant `select` poll plus `recv` with `MSG_PEEK`). A socket the terminal closed between transactions is detected **before** a command is sent, so the client reconnects first.
- The probe never writes to the terminal and never consumes a protocol byte.
- `onDisconnect` fires once per unexpected drop and never for a caller's `disconnect()`.

::: callout danger "Never blindly retry a financial command"
Nothing changes in the retry policy: a payment, reversal or pre-auth is never re-sent after a drop. Recover a lost result with `sendLastResult()` (command `G`). See [Payment Safety](/best-practices/payment-safety).
:::

## Example app and tests

`example-windows/` in the repository is a small RNW console app (status, totals, last result, one payment). It is a separate npm project, because RNW 0.84 pins React Native 0.84.1 while the Expo example uses a newer React Native.

`package/windows/tests` is a standalone CMake + GoogleTest suite that runs the Winsock transport against a loopback server: drop detected before send, no bytes written by the probe, none consumed, one disconnect signal per drop, and a fast probe. Windows has no CI job, so run it and the `example-windows` build locally after native changes.

## Status

The Windows build, the transport tests and a runtime smoke test are verified locally. The smoke test ran in the deployed example app against a fake terminal: Nitro installed, the client connected, a status frame went out, and the drop was detected after the terminal closed the socket. The integration has **not** been verified against a physical terminal yet. Test with small amounts on a test terminal before production use.
