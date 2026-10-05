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

Install the package, its C++ Kit, the Nitro host and Nitro. The host, `@padosoft/react-native-nitro-windows`, is not published yet: link it from a [react-native-support](https://github.com/padosoft/react-native-support) checkout (`file:…/packages/react-native-nitro-windows`), as `example-windows` does.

```bash
npm install @padosoft/react-native-ecr17 @padosoft/ecr17-kit @padosoft/react-native-nitro-windows react-native-nitro-modules
```

Add the host's autolink helper to the app's `react-native.config.js`:

```js
const { windowsAppDependencies } = require("@padosoft/react-native-nitro-windows");

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
`react-native-nitro-modules` does not ship a Windows project yet, and Nitro must exist once per app. The app's Nitro host (`@padosoft/react-native-nitro-windows`) provides the `NitroModules` TurboModule that Nitro's JS calls, installs Nitro into the JS runtime, and registers `Ecr17Client` and `Ecr17Transport`. `windowsAppDependencies()` tells the React Native CLI to skip Nitro's missing Windows project.
:::

## How it works

- The app's Nitro host, `@padosoft/react-native-nitro-windows` (not published yet), implements `NitroModules.install()`. It registers this package's HybridObjects, then runs Nitro's `install()` with RNW's JSI runtime and `CallInvoker`. This package ships no Windows project: it declares its C++ in `package.json` (`nitroWindows`), and the host compiles it together with the Kit's.
- `Ecr17Client` is the shared C++ `HybridEcr17Client` over the Kit's `Ecr17Client`, the same code as on iOS and Android.
- `Ecr17Transport` is `HybridEcr17TransportWindows`, the Nitro face of the Kit's C++ `WinsockTransport` (iOS uses Swift, Android uses Kotlin).
- The host compiles Nitro's C++ runtime from the app's `node_modules/react-native-nitro-modules`, once for every Nitro package in the app.
- The host also generates the `<NitroModules/…>` header shims MSVC needs, because MSVC has no header map.

Every Nitro package that supports Windows goes through the same host, so several of them can live in one app.

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

The Kit's tests (`cmake -S kit`) run the Winsock transport against a loopback server on Windows, with no Nitro or React Native Windows needed: drop detected before send, no bytes written by the probe, none consumed, one disconnect signal per drop, and a fast probe. Windows has no CI job, so run it and the `example-windows` build locally after native changes.

## Status

The Windows build, the transport tests and a runtime smoke test are verified locally. The smoke test ran in the deployed example app against a fake terminal: Nitro installed, the client connected, a status frame went out, and the drop was detected after the terminal closed the socket. The integration has **not** been verified against a physical terminal yet. Test with small amounts on a test terminal before production use.
