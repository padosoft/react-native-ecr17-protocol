# ECR17 example for React Native Windows

A small ECR17 console on **React Native Windows 0.84** (New Architecture,
`cpp-app` / WinAppSDK). It connects to a terminal on the LAN and runs `status`,
`totals`, `sendLastResult` (`G`) and a single payment.

The iOS/Android debug console is the Expo app in [`../example`](../example). The
two can't share one `package.json`: RNW 0.84 pins React Native **0.84.1**, while
the Expo app is on SDK 57 / RN 0.86. So this app is **not** a bun workspace: it
uses npm and its own `package-lock.json`, and links the library with
`file:../package`.

How the native side works (Nitro install shim, Winsock transport, header map):
[`../package/windows/README.md`](../package/windows/README.md).

## Requirements

Windows 10 22H2+ or 11, Node.js 22.11+, Visual Studio 2022 or 2026 with the
"Desktop development with C++" and "Windows application development" workloads,
and Windows SDK 10.0.26100 (pinned in `windows/ExperimentalFeatures.props`).
RNW's `rnw-dependencies.ps1` installs anything missing.

## Run

The library's nitrogen output must exist first. From the repo root:

```bash
bun install
cd package && bunx nitrogen
```

Then:

```bash
cd example-windows
npm install
npm run windows
```

`npm run windows` builds the app and starts Metro. After JS-only changes,
reload. After native changes (C++ in `package/`), rebuild.

Metro loads the library straight from `../package/src`, and resolves its imports
from this app's `node_modules`. That keeps the repo root's React Native (the Expo
app's version) out of the bundle.

## Regenerating the Windows project

```bash
npx react-native init-windows --template cpp-app --name Ecr17Example --namespace Ecr17Example --overwrite
npx react-native autolink-windows
```

`init-windows` overwrites `metro.config.js` and `windows/ExperimentalFeatures.props`;
re-apply the custom resolver and the SDK pin afterwards.

## Verifying a native change

There is no Windows CI job. After changing `package/windows/**` or `package/cpp/**`,
build this app and run the Winsock transport tests locally. Building only the app
project compiles and links `Ecr17.dll` without the MSIX packaging tools:

```bash
msbuild windows/Ecr17Example.sln /t:Ecr17Example /restore /m /p:Configuration=Debug /p:Platform=x64
```

Running the app needs a deployed MSIX package (`npm run windows`), which needs the
Visual Studio MSIX packaging component. The built exe can't be launched on its own.

```bash
cmake -S ../package/windows/tests -B ../build-win
cmake --build ../build-win --config Release
ctest --test-dir ../build-win -C Release --output-on-failure
```
