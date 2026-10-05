# @padosoft/react-native-ecr17 for React Native Windows

Native module for **React Native Windows New Architecture** (Fabric / WinAppSDK, RNW 0.84+).
Same JS API and the same C++ protocol core (`@padosoft/ecr17-kit`) as iOS and Android.
Only the TCP transport is Windows-specific: the Kit's `WinsockTransport`.

## How it works

`react-native-nitro-modules` has no Windows project yet
([mrousavy/nitro#168](https://github.com/mrousavy/nitro/issues/168)), so this
package's DLL installs Nitro itself. The approach follows
[NitromelonDB's Windows support](https://github.com/StasDoskalenko/NitromelonDB/pull/65).

```
JS  react-native-nitro-modules
      TurboModuleRegistry.getEnforcing('NitroModules').install()
        │
        ▼
Ecr17.dll  REACT_MODULE(NitroModules)::install()        Ecr17/Ecr17Module.cpp
        │  CallInvokerDispatcher(ReactContext.CallInvoker())
        ▼
margelo::nitro::install(runtime, dispatcher)
        │
        ▼
HybridObjectRegistry
  "Ecr17Client"    → HybridEcr17Client          ../cpp → Ecr17Kit::Ecr17Client (shared with iOS/Android)
  "Ecr17Transport" → HybridEcr17TransportWindows Ecr17/ → Ecr17Kit::WinsockTransport (kit/windows)
```

1. **Autolinking.** The package `react-native.config.js` points RNW at `windows/Ecr17.sln`.
2. **Skip Nitro's missing Windows project.** Apps spread `windowsAppDependencies()`
   from `@padosoft/react-native-ecr17/windows-autolink`, which sets
   `react-native-nitro-modules` `platforms.windows` to `null`.
3. **Nitro C++ from node_modules.** `Ecr17.vcxproj` compiles the app's
   `react-native-nitro-modules/cpp` (minus `views/`), the binding in `../cpp`, the
   Kit's `cpp/src` and `windows/src` (`@padosoft/ecr17-kit`, found in `node_modules`
   or, in this repo, `../kit`; override with `/p:Ecr17KitDir=…`) and the nitrogen
   specs in `../nitrogen/generated/shared/c++`.
4. **MSVC header map.** Nitro and nitrogen include `<NitroModules/Foo.hpp>`.
   iOS/Android get that prefix from a header map; MSVC has none. Before compiling,
   MSBuild runs `../scripts/windows-nitro-shims.mjs`, which writes one-line shims to
   `windows/include/NitroModules/` (gitignored).
5. **Platform hooks.** `NitroWindowsPlatform.cpp` implements Nitro's per-platform
   `ThreadUtils` / `Logger` (thread names, `OutputDebugString`, RNW UI dispatcher).

Only one native module per app can provide the `NitroModules` TurboModule. If
another Nitro library in the same app ships the same shim, the two conflict.

## Transport and payment safety

`HybridEcr17TransportWindows` is the Nitro face of the Kit's `WinsockTransport`, which
mirrors the Kotlin transport:

- TCP with `TCP_NODELAY` and a background reader thread.
- `isConnected()` is a **non-destructive, write-free** liveness probe:
  an instant `select` poll plus `recv(MSG_PEEK)` of one byte. ECR17/Nexi terminals close
  the socket between transactions, so a dead socket is found **before** a
  financial command is sent, and a reconnect happens first. The probe never
  writes to the terminal and never consumes a protocol byte.
- `onDisconnect` fires exactly once per unexpected drop, never for `disconnect()`.

The money-safety rules are unchanged: a financial command is never re-sent after
a drop (`kit/cpp/include/Ecr17Kit/RetryPolicy.hpp`). Recover a lost result with
`sendLastResult()` (command `G`).

The Kit's tests run the protocol core, the client and, on Windows, the Winsock
transport against a loopback server. No Nitro, no RNW, no nitrogen needed:

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
npm install @padosoft/react-native-ecr17 react-native-nitro-modules
```

In the app's `react-native.config.js`:

```js
const { windowsAppDependencies } = require("@padosoft/react-native-ecr17/windows-autolink");

module.exports = {
  dependencies: windowsAppDependencies(),
};
```

Then:

```bash
npx react-native autolink-windows
npx react-native run-windows
```

If the package is linked from a local checkout instead of `node_modules`, pass
`windowsAppDependencies({ root: "<path to the package folder>" })`.

See [`example-windows/`](https://github.com/padosoft/react-native-ecr17-protocol/tree/main/example-windows)
for a working app.
