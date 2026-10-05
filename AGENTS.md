# AGENTS.md — react-native-ecr17-protocol

Guidance for AI agents (and humans) working in this repo. Read this first, then
`PROGRESS.md` (current task/resume state) and `docs/LESSON.md` (accumulated,
hard-won engineering lessons). **Always pass `docs/LESSON.md` into the prompt of
any sub-agent you spawn, and re-read it when starting a new session.**

## What this is
A React Native **Nitro** module implementing the Italian **ECR17** payment
protocol (Nexi Group POS terminals) over LAN. Protocol engine in C++ (shared),
native TCP transport in Kotlin/Swift (C++/Winsock on React Native Windows),
async Promise API to JS.

A **bun monorepo** (`workspaces: ["packages/*", "apps/example"]`):

```
packages/ecr17/              @padosoft/ecr17 — the native library (no React)
packages/react-native-ecr17/ @padosoft/react-native-ecr17 — the React Native binding
apps/example/                Expo debug console (iOS / Android), a workspace
apps/example-windows/        RNW console: npm, NOT a workspace (RNW pins an older RN)
apps/docs/                   documentation site (docmd): npm, NOT a workspace
scripts/                     repo scripts, TypeScript run with bun (`bun scripts/x.ts`)
Package.swift                the `Ecr17` Swift package (the core); SwiftPM needs it at the repo root
```

Scripts are `.ts`, never `.mjs`/`.js` (except config files a tool requires as JS, e.g.
`app.plugin.js`, `react-native.config.js`, `babel.config.js`). Root scripts run with `bun`;
`apps/docs/scripts` run with `node` (≥ 22.18 strips types; `apps/docs/.node-version` is 24).

A new standalone native package (e.g. a future Swift or Kotlin API) goes in `packages/`, a new
example or tool app in `apps/`. Roadmap: README "Roadmap" section.

- **`packages/ecr17/` — `@padosoft/ecr17`**, the React-free C++ core (namespace
  `padosoft::ecr17`, headers `<ecr17/…>`): `Lcr` (LRC) → `PacketCodec` (framing) →
  `Ecr17Protocol` (builders) → `Ecr17Response` (parsers) → `Ecr17Session` (ACK/NAK +
  retransmit + timeout) → `Ecr17Client` (auto-connect, pre-send probe, money-safe retry).
  `Transport` is the byte-stream interface; `PosixTransport` (macOS/Linux, `posix/`),
  `WinsockTransport` (Windows, `windows/`) and `FakeTransport` (tests) implement it.
  **Node.js API**: `src/` (TypeScript `Ecr17Client`, same API as the RN binding; built with
  tsdown) over `node/addon.cpp` (Node-API: config/requests in, raw core structs out; one
  worker thread per client, results/events back via thread-safe functions). `src/mappers.ts`
  maps raw structs to public types exactly like `HybridEcr17Client.cpp` — keep the two in sync,
  and `src/types.ts` in sync with the RN package's types. Usable from native apps: CMake (`ecr17::ecr17`), the `Ecr17` pod, the `Ecr17`
  Swift package (root `Package.swift`, tags `X.Y.Z` from 2.0.0).
- **`packages/react-native-ecr17/` — `@padosoft/react-native-ecr17`**, the Nitro binding (namespace
  `margelo::nitro::ecr17`, pod `ReactNativeEcr17` = nitro.json `iosModuleName`; Android lib
  stays `libEcr17.so`): `HybridEcr17Client` maps the Nitro types onto `@padosoft/ecr17`'s
  `Ecr17Client`; `NativeTransportAdapter` exposes the `Ecr17Transport` HybridObject
  (Kotlin/Swift; the core's Winsock on Windows) as an `ecr17::Transport`. No protocol logic.
Spec reference vendored in `docs/`.

## Mandatory workflow (Definition of Done)
A task/phase is done ONLY after BOTH loops below pass. In auto mode, proceed to
the next phase only once complete.

### Local loop (per phase, before pushing)
1. **Local tests green** — C++: the core's GoogleTest suite (core, session, client,
   protocol flows; on Windows also the Winsock transport):
   `cmake -S packages/ecr17 -B build-kit && cmake --build build-kit && ctest --test-dir build-kit --output-on-failure`
   (GoogleTest from `find_package` or FetchContent). Without cmake, a throwaway **g++ harness**
   still works: `g++ -std=c++20 -I packages/ecr17/cpp/include <harness>.cpp packages/ecr17/cpp/src/*.cpp`.
   TS: `cd packages/react-native-ecr17 && bunx tsc --noEmit -p tsconfig.ci.json`.
2. **Local Copilot review** — `copilot --autopilot --yolo -p "/review …"`. Use a
   **focused** prompt ("read ONLY file X, check N things, answer in <=K lines") —
   feeding a whole diff times out. Copilot **edits in --yolo mode** and its
   nitro/C++ suggestions are sometimes wrong: VERIFY against the generated
   headers, don't trust blindly. Record takeaways in `docs/LESSON.md`.
3. **Zero actionable comments** → continue; else fix and go to 1.

### Remote loop (REQUIRED before a task/PR is considered done)
4. **Push**, then **CI green** (`cpp-tests` + `ts-checks` + `Node.js addon`); else fix → local loop.
   **If the PR touches native code** (`packages/react-native-ecr17/android/**`, `packages/react-native-ecr17/ios/**`, the
   Nitro-integrated C++ in `Ecr17Client`/adapter, `CMakeLists.txt`, `*.podspec`,
   `nitro.json`, autolinking/`react-native.config.js`), also dispatch the manual
   **`android-build`** job (`gh workflow run "Android build" --ref <branch>`) and
   require it green — it's the ONLY CI that compiles/links the native + Nitro code.
5. **Remote PR review** — ensure the PR is reviewed by the remote bots
   (`copilot-pull-request-reviewer[bot]` + `chatgpt-codex-connector[bot]`);
   re-request review after each push (e.g. `gh pr edit <n> --add-reviewer copilot`).
   **WAIT** for the reviews to land.
6. **Fix every valid comment** (validate each against the code/spec; reject only
   with a clear reason), push, re-request review. **Repeat 4–6 until the reviewers
   report ZERO actionable comments.**
7. Only then is the task done — merge the PR. Update `PROGRESS.md`.

Rationale: local verification can miss things; the remote CI + AI-review loop is a
second independent gate, so a merged PR is "super robust" — never merge a PR that
still has open, valid reviewer comments.

## CI
- `cpp-tests` (fast, ~1 min): the real correctness gate — builds/runs the core's
  GoogleTest suite (`cmake -S packages/ecr17`). Keep it green.
- `ts-checks` (fast): typecheck + nitrogen codegen.
- `Node.js addon` (`node.yml`, ~3 min): builds `@padosoft/ecr17`'s Node-API addon on
  linux-x64, linux-arm64, darwin-arm64 and win32-x64, runs `node --test` against the fake
  terminal (so Winsock IS exercised on Windows CI through Node), uploads each `ecr17.node`;
  plus typecheck + tsdown (needs `GESCAT_NPM_TOKEN` for `@padosoft/config`). The release
  workflow calls it and ships the binaries in `packages/ecr17/prebuilds/`.
- **Windows has no C++/RNW CI job (by decision): verify it LOCALLY** on this Windows host
  (VS 2022 Build Tools / VS 2026 + Windows SDK in `D:\Windows Kits\10`):
  1. Core tests, Winsock transport included (no Nitro or RNW needed):
     `cmake -S packages/ecr17 -B build-win && cmake --build build-win --config Release && ctest --test-dir build-win -C Release --output-on-failure`
  2. RNW build + deploy of `apps/example-windows` (compiles the Nitro host DLL from
     `@padosoft/react-native-nitro-windows`, which collects this package's `nitroWindows`
     sources and the core's; builds and registers the MSIX package). The host is not
     published yet: `apps/example-windows` links it from a react-native-support checkout next to this repo, so clone
     react-native-support next to this repo first:
     `cd apps/example-windows && npm install && npx react-native run-windows --arch x64`.
     The auto-launch at the end fails here (RNW calls `Get-AppxPackage` through PowerShell 7,
     whose Appx module can't load); the app IS deployed — launch it with
     `explorer.exe "shell:AppsFolder\Ecr17Example_mcede4qbjepqr!App"` or from the Start menu.
  3. Runtime smoke test (for Nitro/transport changes): temporarily point `index.js` at a
     script that creates the client and connects to a local fake terminal (Node TCP server);
     RN 0.84's Metro does NOT print `console.log`, so POST results to a local HTTP logger.
  Required for any change under `packages/ecr17/**`, `packages/react-native-ecr17/windows/**` or `packages/react-native-ecr17/cpp/**`.
- `android-build` (~15-20 min, **manual dispatch only**:
  `gh workflow run "Android build" --ref <branch>`): compiles C++/Kotlin/Nitro
  via expo prebuild + gradle. The ONLY verifier of client/adapter/native code
  (not in the unit target). It installs `@padosoft/native-modules` from GitHub Packages,
  so the `GESCAT_NPM_TOKEN` secret must be able to read padosoft packages. Batch native edits; run sparingly. **No iOS CI** →
  Swift is best-effort.

## Hard-won rules (see docs/LESSON.md for the full list)
- 💰 **Money-critical — never blindly retry a financial command.** This terminal
  charges real cards. On a drop, reconnect the socket but do NOT re-send
  payments/reversals/pre-auths (double-charge); recover via `sendLastResult()`
  (command `G`). The decision is in `packages/ecr17/cpp/include/ecr17/RetryPolicy.hpp`, locked
  by `test_retry_policy.cpp` and end to end by `Client.AFinancialCommandIsNeverResentAfterADrop`
  (`packages/ecr17/cpp/tests/test_client.cpp`, which replays every command against a drop). `Ecr17Session` resets its connection state per
  transaction (`resetForNewTransaction`) so it's reusable across reconnects.
- **Local C++ toolchain**: the core builds with CMake on any host (Windows: VS Build
  Tools; macOS: Xcode clang + `brew install googletest`). A WinLibs **g++ 16** at
  `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64\bin\g++.exe`
  also compiles the core (`-std=c++20 -I packages/ecr17/cpp/include packages/ecr17/cpp/src/*.cpp`). Avira may
  quarantine a freshly-built `.exe` — a one-time AV exclusion fixes it. The Nitro binding
  (`HybridEcr17Client`, adapter, Kotlin/Swift) is NOT in the unit target: Android via the
  `android-build` job; iOS by building the Expo example on a Mac (`expo prebuild -p ios`,
  `pod install`, `xcodebuild`).
- The `Bash` tool is **bash** (use heredocs / `git commit -F -`, never PowerShell
  here-strings like `@'…'@`).
- `@padosoft/config` is a **private GitHub Packages** root devDep that blocks
  `bun install` in CI. It's tooling-only → CI strips it: `jq 'del(.devDependencies["@padosoft/config"])' package.json > t && mv t package.json && rm -f bun.lock && bun install`.
- `nitrogen/generated/**` is **gitignored** (regenerate with `cd packages/react-native-ecr17 && bunx nitrogen`).
- Nitro: TS `Promise<T>` → C++ `std::shared_ptr<Promise<T>>`,
  `Promise<T>::async([]() -> T {…})`; string-union enums → SCREAMING members
  (`cardNotPresent`→`CARDNOTPRESENT`); optional TS fields → `std::optional`;
  numbers → `double`. Transport spec uses `std::shared_ptr<ArrayBuffer>`
  (`ArrayBuffer::copy(vector)`, `buf->data()/size()`). Get other HybridObjects
  via `HybridObjectRegistry::createHybridObject(name)`. Kotlin HybridObjects get
  an auto-generated JNI bridge (no manual JNI); use `Promise.parallel{}` for
  blocking work, `ArrayBuffer.copy(ByteArray)/toByteArray()`.
- **Nitro Android native integration (JNI) — gotchas only reproducible by RUNNING
  the app, not by the build** (full detail in docs/LESSON.md):
  1. The C++ impl file MUST be named after `implementationClassName` from
     `nitro.json` (`HybridEcr17Client.{hpp,cpp}`) and its dir be in CMake
     `include_directories` — the generated `*OnLoad.cpp` does a flat
     `#include "HybridEcr17Client.hpp"`.
  2. Downcast `createHybridObject` results with `dynamic_pointer_cast` (HybridObject
     is a *virtual* base); null-check.
  3. Autolinking + `.so` load needs `packages/react-native-ecr17/react-native.config.js` (declares
     android/ios) so RN registers `Ecr17Package`, whose `companion init` runs
     `System.loadLibrary`.
  4. Commands run on Nitro worker threads → attach to the JVM with fbjni
     `ThreadScope` (`#ifdef __ANDROID__`) before any C++→Kotlin call, else "Unable
     to retrieve jni environment".
  5. `createHybridObject` (JNI `FindClass`) must run on the **JS thread** (do it in
     `configure()`), because attached worker threads use the system class loader →
     `ClassNotFoundException`. fbjni caches the jclass so later worker-thread method
     calls work.
- Don't reuse a Nitro-generated struct name in our namespace (clash) — e.g. our
  parser DCC struct is `DccInfo`, not `CurrencyExchange`.
- ECR17: status code is lowercase `'s'`; payment `'P'` = 167 bytes; progress
  `SOH`+20+`EOT` has no LRC; `decode()` treats the buffer as exactly one frame.

## Conventions
- C++20. Core headers are included as `<ecr17/Name.hpp>` (from `packages/ecr17/cpp/include`). A new
  `packages/ecr17/cpp/src/*.cpp` goes in `packages/ecr17/CMakeLists.txt`; the pod, the binding's Android CMake and
  the Windows vcxproj pick it up by glob. A new test goes in `packages/ecr17/cpp/tests/CMakeLists.txt`.
  New `packages/react-native-ecr17/cpp/**/*.cpp` (binding only) MUST be added to `packages/react-native-ecr17/android/CMakeLists.txt`;
  iOS globs via `nitro_module`, Windows via the `nitroWindows.sources` globs in package.json
  (a new folder needs a new glob).
- Build tooling from `@padosoft/native-modules` (not on npm yet: private GitHub Packages,
  installed with `GESCAT_NPM_TOKEN`): `ReactNativeEcr17.podspec` is `nitro_module(…)` plus
  `native_dependency(s, "Ecr17", spm_url: <this repo>, …)` (SwiftPM when the app opts in,
  else the `Ecr17` pod), `packages/ecr17/Ecr17.podspec` is `native_kit`,
  `android/build.gradle` applies `nitro-module.gradle`. Keep the native namespace in
  `react-native.config.js` (`android.packageName`): autolinking can't read it from
  `ext.nitroModule`.
- Windows: `packages/react-native-ecr17/windows/` (the Windows HybridObject + registration, compiled by the app's
  Nitro host) + `apps/example-windows/` (separate npm
  app: RNW pins an older RN than the Expo example, so it is NOT a bun workspace).
  See `packages/react-native-ecr17/windows/README.md` and the Windows section of docs/LESSON.md.
- Commit messages: gitmoji-free conventional style; end with the Co-Authored-By
  trailer. Branch + PR per feature; keep CI green per push.
