# @padosoft/react-native-ecr17 for React Native Windows

For **React Native Windows New Architecture** (Fabric / WinAppSDK, RNW 0.84+).
It has the same JS API and the same C++ protocol core (`@padosoft/ecr17-kit`) as
iOS and Android. Only the TCP transport is Windows-specific: the Kit's
`WinsockTransport`.

## How it works

`react-native-nitro-modules` has no Windows support yet
([mrousavy/nitro#168](https://github.com/mrousavy/nitro/issues/168)), and Nitro
must exist only **once** per app. So this package ships **no Windows project**.

The app adds one Nitro host,
[`@padosoft/react-native-nitro-windows`](https://github.com/padosoft/react-native-support/tree/main/packages/react-native-nitro-windows).
The host provides the `NitroModules` TurboModule, installs Nitro, and compiles
the Windows C++ that each Nitro package declares in its `package.json`:

```json
"nitroWindows": {
  "sources": ["windows/*.cpp", "cpp/Ecr17Client/*.cpp", "cpp/Transport/*.cpp", "nitrogen/generated/shared/c++/*.cpp"],
  "includeDirs": ["windows", "cpp", "cpp/Ecr17Client", "nitrogen/generated/shared/c++"],
  "headers": ["windows/Ecr17Windows.hpp"],
  "register": ["margelo::nitro::ecr17::registerEcr17HybridObjects"],
  "kits": ["@padosoft/ecr17-kit"]
}
```

The Kit declares its own sources (`nativeKit.windows`: the core and
`WinsockTransport`). The host compiles each Kit once, even when two packages
wrap it.

```
JS  react-native-nitro-modules
      TurboModuleRegistry.getEnforcing('NitroModules').install()
        │
        ▼
NitroWindows.dll  (the host)  install(): registerEcr17HybridObjects(), then Nitro
        │
        ▼
HybridObjectRegistry
  "Ecr17Client"    → HybridEcr17Client          ../cpp → Ecr17Kit::Ecr17Client (shared with iOS/Android)
  "Ecr17Transport" → HybridEcr17TransportWindows windows/ → Ecr17Kit::WinsockTransport (kit/windows)
```

## Transport and payment safety

`HybridEcr17TransportWindows` is the Nitro face of the Kit's `WinsockTransport`.
Its `connect` runs on a dedicated thread, not on Nitro's thread pool. The
transport mirrors the Kotlin one:

- TCP with `TCP_NODELAY` and a background reader thread.
- `isConnected()` is a **non-destructive, write-free** liveness probe: an
  instant `select` poll plus `recv(MSG_PEEK)` of one byte. ECR17/Nexi terminals
  close the socket between transactions, so a dead socket is found **before** a
  financial command is sent, and the client reconnects first. The probe never
  writes to the terminal and never consumes a protocol byte.
- `onDisconnect` fires exactly once per unexpected drop, never for `disconnect()`.

The money-safety rules are unchanged: a financial command is never re-sent after
a drop (`kit/cpp/include/Ecr17Kit/RetryPolicy.hpp`). Recover a lost result with
`sendLastResult()` (command `G`).

The Kit's tests run the protocol core, the client and the Winsock transport
against a loopback server. No Nitro or RNW is needed:

```powershell
cmake -S kit -B build-win
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
```

## Requirements

| Tool | Version / notes |
| --- | --- |
| Windows | 10 22H2+ or 11 |
| Node.js | 22.11+ |
| React Native Windows | 0.84+ (New Architecture, `cpp-app` template) |
| Visual Studio | 2022 (MSVC v143) or 2026 (MSVC v145) |
| Workloads | Desktop development with C++; Windows application development |
| Windows SDK | 10.0.26100 |

RNW's [dependency script](https://microsoft.github.io/react-native-windows/docs/rnw-dependencies)
installs anything missing (elevated PowerShell):

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\node_modules\react-native-windows\scripts\rnw-dependencies.ps1
```

## App setup

```bash
npm install @padosoft/react-native-ecr17 @padosoft/ecr17-kit @padosoft/react-native-nitro-windows react-native-nitro-modules
```

> ⚠️ `@padosoft/react-native-nitro-windows` is **not published yet**. Until it is,
> link it from a checkout of
> [react-native-support](https://github.com/padosoft/react-native-support), as
> `example-windows` does
> (`"file:../../react-native-support/packages/react-native-nitro-windows"`).

In the app's `react-native.config.js`:

```js
const { windowsAppDependencies } = require("@padosoft/react-native-nitro-windows");

module.exports = {
  dependencies: windowsAppDependencies(),
};
```

Then:

```bash
npx react-native autolink-windows
npx react-native run-windows
```

Autolinking adds the host's project (`NitroWindows.vcxproj`) to the app
solution. No project comes from this package.

See [`example-windows/`](https://github.com/padosoft/react-native-ecr17-protocol/tree/main/example-windows)
for a working app.
