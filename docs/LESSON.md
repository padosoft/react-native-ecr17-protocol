# LESSON.md — accumulated learnings (ECR17 module)

> **Context rule:** the content of this file MUST be passed into the prompt of
> every parallel subagent, and re-read at the start of every new session, so
> hard-won knowledge is never lost. Update it continuously — especially after
> Copilot/CI feedback and after fixing any bug.

## Environment & tooling
- Host is **Windows**; the `Bash` tool runs **bash**, the `PowerShell` tool runs pwsh.
  ⚠️ Do **not** use PowerShell here-strings (`@'...'@`) inside the Bash tool —
  the `@` leaks into the arg. Use a bash heredoc (`<<'EOF' … EOF`) or `git commit -F -`.
- ⚠️ (2026-10-05) The WinLibs g++ below was **no longer installed** — reinstall with
  `winget install BrechtSanders.WinLibs.POSIX.UCRT` before using the local harness.
  The Windows SDK IS installed, but on **`D:\Windows Kits\10`** (10.0.26100 + 10.0.28000;
  registry `HKLM\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\Windows\v10.0`), not under
  Program Files — don't conclude "no SDK" from the default path. CMake picks VS 2022
  Build Tools (MSVC 14.44); `C:\CMake\bin\cmake.exe` works. Windows code is verified LOCALLY
  (no Windows CI job, by decision) — see AGENTS.md.
- ⚠️ Backslashes in a Bash-tool heredoc can get collapsed (`"Ecr17\Ecr17.vcxproj"` became
  `"Ecr17Ecr17.vcxproj"` in a generated .sln). Write files containing Windows paths with
  the Write tool, then check them.
- **Local C++ toolchain available**: WinLibs **g++ 16** (MinGW/UCRT) at
  `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64\bin\g++.exe`
  (installed via `winget install BrechtSanders.WinLibs.POSIX.UCRT`). Compile the
  unit-testable core into a throwaway harness for a real local RED→GREEN check:
  `g++ -std=c++20 -I packages/ecr17/cpp/include <harness>.cpp packages/ecr17/cpp/src/*.cpp`.
  ⚠️ The preinstalled MSVC (VS18) is broken — its STL `include/` dir is missing, so
  `cl` can't compile; use g++. ⚠️ Avira quarantines a freshly-built `.exe`
  (false positive) → add a one-time AV exclusion for the build dir. The full GoogleTest
  suite is `cmake -S packages/ecr17` (CI: `cpp-tests`, Ubuntu). Native Swift/Kotlin + the
  Nitro-integrated C++ are not in it: Android via the Android build CI job, iOS by
  building the Expo example on a Mac.
- `copilot` CLI present for the local review loop.
- `nitrogen` runs via `./node_modules/.bin/nitrogen` (needs only Node/Bun).
  `nitrogen/generated/**` is **gitignored** (regenerated, not committed).

## Nitro facts
- TS `Promise<T>` → C++ `std::shared_ptr<margelo::nitro::Promise<T>>`.
  Use `#include <NitroModules/Promise.hpp>`; `Promise<T>::async(lambda)` runs the
  lambda on Nitro's `ThreadPool` and resolves/rejects from a worker thread
  (dispatch back to JS is automatic). Exceptions thrown in the lambda → reject.
- Generated enum for a TS string union keeps UPPER member names: e.g.
  `type LrcMode = "stx"|"std"|"noext"|"stx_noext"` → `enum class LrcMode { STX, STD, NOEXT, STX_NOEXT }`.
- Spec virtuals are pure (`= 0`) in `HybridXxxSpec`; the impl class overrides them.
  Methods registered in `loadHybridMethods()` via `registerHybridMethod`.

## Runtime (Android)
- **Calling a Kotlin HybridObject from a C++ `Promise::async` worker thread fails
  with "Unable to retrieve jni environment. Is the thread attached?".** Nitro's
  C++ thread-pool worker threads are NOT attached to the JVM, so the generated
  C++→Kotlin JNI bridge can't get a `JNIEnv`. Fix: attach with fbjni
  `facebook::jni::ThreadScope` (RAII) for the scope of the transport calls —
  guarded by `#ifdef __ANDROID__` (no-op on iOS). We attach in `ensureConnected`,
  `runTransaction`, `runAckOnly` (the worker-thread paths that hit the transport).
  Only reproducible by RUNNING the app — not by the build.
- **`ClassNotFoundException` for the Kotlin HybridObject when created from a
  worker thread.** After attaching a Nitro worker thread with `ThreadScope`,
  `createHybridObject` does a JNI `FindClass` that resolves against the *thread's*
  class loader — an attached worker thread gets the SYSTEM class loader (the error
  shows `DexPathList[... /system/lib64 ...]`), which can't see app classes. Fix:
  perform `ensureInit()` (the `createHybridObject`) on the **JS thread** (in
  `configure()`), which has the app class loader; fbjni caches the resolved
  `jclass` globally, so later method calls from worker threads work (they only need
  a `JNIEnv`, supplied by the `ThreadScope` guards). Only reproducible by RUNNING
  the app. (Alternative: `ThreadScope::WithClassLoader`.)
- **`ClassNotFoundException` for `com.margelo.nitro.core.ArrayBuffer` when a command
  runs.** This is the SAME root cause as above but for a NitroModules *core* class,
  not our transport. The generated transport bridge resolves `ArrayBuffer` LAZILY on
  the worker thread inside `send()` (`JHybridEcr17TransportSpec::send` →
  `JArrayBuffer::wrap` → `FindClass("com/margelo/nitro/core/ArrayBuffer")`). A plain
  `facebook::jni::ThreadScope` only ATTACHES a JNIEnv — it does NOT install the app
  class loader, so that `FindClass` still hits the SYSTEM loader
  (`DexPathList[... /system/lib64 ...]`) → ClassNotFoundException. The PR#8 fix
  (creating the transport on the JS thread) only cached OUR transport's jclass; it
  doesn't help core classes looked up later on a worker thread. Fix: run ALL
  worker-thread C++→Kotlin JNI work under **`facebook::jni::ThreadScope::WithClassLoader(std::function<void()>)`**,
  which attaches the thread AND installs fbjni's cached app class loader for the
  duration, so every `FindClass` inside (incl. NitroModules' ArrayBuffer) resolves
  app classes. We wrap the bodies of `ensureConnected`/`runTransaction`/`runAckOnly`
  by passing a lambda to the `runOnJvmThread(fn)` template helper: on Android it runs
  `fn` inside `WithClassLoader`; on iOS (no JVM) it just calls `fn()`. Since
  `WithClassLoader` takes a `std::function<void()>`, on Android the helper captures
  `fn`'s return value in a `std::optional` and any thrown exception via
  `std::exception_ptr` (rethrown after the scope), so the caller's return-value and
  money-safety try/catch semantics are unchanged. `fn` being a lambda means a
  `return` inside it exits the lambda (not the caller) on both platforms — safe in
  the value-returning `runTransaction`. Replaces the old plain-`ThreadScope`
  `ECR17_JNI_THREAD_GUARD` (which only attached a JNIEnv, not the class loader).
  Only reproducible by RUNNING the app — no build/CI catches it.
- **Emit `DISCONNECTED` on a failed connect**, else listeners stay stuck on
  `CONNECTING`. `connect()` delegates to `ensureConnected()` which emits
  CONNECTING→(CONNECTED | DISCONNECTED on throw).
- **ECR17/Nexi terminals close the TCP socket BETWEEN transactions → detect the
  drop PROACTIVELY (before sending), not reactively (after).** Observed on a real
  device: financial commands (verifyCard/pay) INTERMITTENTLY failed with
  "transport disconnected during exchange" while safe commands (status/totals)
  succeeded — apparent "works once, fails next". Root cause: the terminal closes
  the socket after a transaction (and the Kotlin reader thread may not have
  observed the EOF `read()<0` yet — a race), so `isConnected()` returned `true`
  for a half-open socket. The command was then SENT on a dead socket; the read
  failed MID-exchange; `runTransaction`'s catch reconnected and applied the
  money-safety RetryPolicy → safe/idempotent ops were replayed (→ ok) but
  financial ops were (correctly) NOT replayed → a FALSE error surfaced. The
  reactive reconnect left a fresh socket, so the user's NEXT manual attempt landed
  on a good socket → the alternation. The money-safety behavior was correct; the
  bug was discovering the drop AFTER the send instead of BEFORE. Fix: make Kotlin
  `isConnected()` a synchronous, NON-DESTRUCTIVE, WRITE-FREE liveness probe — a
  1-byte peek on a `PushbackInputStream` (short `soTimeout`): `read()==-1` ⇒ peer
  closed (dead); `SocketTimeoutException` ⇒ idle but alive; any read byte is
  `unread()` so a protocol byte is NEVER consumed. ⚠️ Do NOT use
  `socket.sendUrgentData(0xFF)` for this (the first version did): it WRITES a TCP
  out-of-band byte, and on a terminal with `SO_OOBINLINE` that 0xFF lands INLINE
  right before the next `STX` frame — corrupting a financial command. The probe
  must never put bytes on the peer's protocol stream. The reader thread and the
  probe share the input stream under one `ioLock` (reader uses a short read timeout
  so it releases the lock between reads); a single-shot `AtomicBoolean` makes a drop
  fire onDisconnect exactly once across both paths, and the reader/probe close the
  socket on a drop so `isConnected()`'s `isClosed` check is an immediate signal.
  When the probe trips it marks the socket dead, so the existing `ensureConnected()`
  (called at the start of every command) reconnects BEFORE the send and the
  financial command starts on a verified-live socket. Money-safety is UNCHANGED —
  RetryPolicy and sendLastResult ('G') recovery are untouched; we only removed the
  FALSE drop from a stale pre-send socket; a genuine mid-exchange drop still
  surfaces and is recovered via 'G'. iOS (`NWConnection.state == .ready`) reflects
  peer-close too but state updates are async (small residual race; no iOS CI →
  best-effort). Only reproducible by RUNNING against a real terminal — no build/unit
  CI catches it.

## Windows (React Native Windows, New Architecture)
- **Since the Kit split (2026-10-05) this package ships no Windows project.** The app's Nitro
  host, `@padosoft/react-native-nitro-windows` (react-native-support, NOT published yet:
  `apps/example-windows` links it from a react-native-support checkout next to this repo), does what the old Ecr17 DLL
  did (below): it compiles the `nitroWindows` sources of every Nitro package + their Kits'
  `nativeKit.windows` sources, and calls `registerEcr17HybridObjects()` before installing
  Nitro. Its own vcxproj links no `ws2_32`: the Kit's WinsockTransport.cpp links it with
  `#pragma comment(lib, ...)`. The notes below describe the original DLL; the mechanics
  (install, shims, registry) moved into the host unchanged.
- Approach copied from NitromelonDB PR #65. **Nitro has no Windows project**
  (mrousavy/nitro#168), so `packages/react-native-ecr17/windows/Ecr17` (a WinAppSDK DLL) provided the
  `NitroModules` TurboModule (`REACT_MODULE(NitroModules)` + sync `install()`):
  `TryGetOrCreateContextRuntime(ctx)` + `CallInvokerDispatcher(ctx.CallInvoker())`
  → `margelo::nitro::install`, then `HybridObjectRegistry::registerHybridObjectConstructor`
  for `Ecr17Client` and `Ecr17Transport` (the jobs nitrogen's OnLoad/Autolinking do
  on Android/iOS). Only ONE module per app may provide `NitroModules`.
- Apps must spread `windowsAppDependencies()` (`packages/react-native-ecr17/windows-autolink.js`) into
  their `react-native.config.js`: it sets `react-native-nitro-modules`
  `platforms.windows = null` so the RNW CLI doesn't look for Nitro's missing vcxproj.
- The transport spec is `{ios: swift, android: kotlin}`, but nitrogen still emits the
  shared C++ `HybridEcr17TransportSpec` → Windows implements it directly in C++
  (`HybridEcr17TransportWindows`, a thin wrapper over the Kit's `WinsockTransport`). Its probe is `recv(MSG_PEEK)` after an instant (0 ms)
  `select` — no PushbackInputStream needed, still write-free/non-consuming. Per-connection
  state object so a stale reader from an old socket can't flip a new connection.
- MSVC has no `<NitroModules/…>` header map → (historical: the package's `windows-nitro-shims.mjs`, deleted with its own Windows project in a825f73)
  writes one-line shims (gitignored `packages/react-native-ecr17/windows/include/`), run by MSBuild.
- Nitro 0.37 added `cpp/views/RawPropsCompat.cpp` (needs Fabric renderer headers):
  excluded from the vcxproj and the test build (no views here).
- **RNW pins RN exactly** (`react-native-windows@0.84.0` → `react-native@0.84.1`), while
  the Expo example is RN 0.86 → `apps/example-windows/` is a separate **npm** project
  (`file:../../packages/react-native-ecr17` symlink), not a bun workspace. Its Metro `resolveRequest` maps the
  library to `packages/react-native-ecr17/src` and re-roots bare imports from the library to the app,
  otherwise Metro walks up to the repo-root node_modules (RN 0.86 + 2nd Nitro copy).
- `init-windows` files are CRLF; RNW New Arch defaults to SDK 10.0.22621 → pin
  10.0.26100 in `apps/example-windows/windows/ExperimentalFeatures.props` (installed locally).
- Local verification done for this: example `tsc`, a full `react-native bundle
  --platform windows` (checked no `../node_modules` modules leaked in), shim script.
  The transport + DLL are built and tested locally (see AGENTS.md).
- **MSVC `std::mutex` (SRWLOCK) is NOT fair.** First transport version held the I/O
  lock across the reader's 100 ms `select`; the probe (`isConnected()`, run before
  every command) was starved: ~340 ms per call, potentially unbounded. Removing the
  lock entirely was WRONG too: probe `select` says readable → reader drains the bytes →
  probe's blocking `recv(MSG_PEEK)` waits for the next frame (a test hung 682 s).
  Fix: reader waits WITHOUT the lock and takes it only around `recv`; probe holds it for
  an instant `select(0)` + peek. Both regressions are locked by tests + a ctest TIMEOUT.
- The RNW `cpp-app` template only RUNS as a deployed MSIX package: launching the built
  `Ecr17Example.exe` directly aborts (0xC0000409 in ucrtbase) because nothing registers
  the WinRT classes (Microsoft.ReactNative, Ecr17) for an unpackaged process. Packaging
  (`run-windows` / the .wapproj) needs VS's MSIX packaging (DesktopBridge) component.
- Installing that component: `rnw-dependencies.ps1 -Install` did NOT add it (it updated
  VS Build Tools 2026 instead, and the VS installer runs one operation at a time). What
  worked: `setup.exe modify --installPath <VS 2026 dir> --add Microsoft.VisualStudio.Workload.Universal
  --add Microsoft.VisualStudio.ComponentGroup.UWP.Support --add Microsoft.VisualStudio.ComponentGroup.UWP.VC
  --includeRecommended --passive` (needs UAC; done by the user). Check for
  `MSBuild/Microsoft/DesktopBridge/Microsoft.DesktopBridge.props` in the VS dir.
- `run-windows` builds + deploys fine, but its final launch step runs `Get-AppxPackage`
  via PowerShell 7 (Store build), where the Appx module fails (0x80131539) → exit 127.
  The package is registered anyway; launch via `shell:AppsFolder\<PackageFamilyName>!App`
  (query it with Windows PowerShell 5.1: `powershell.exe -NoProfile -Command "Get-AppxPackage Ecr17Example"`).
- RN 0.84 Metro does not show the app's `console.log` (logs moved to DevTools). For a
  scripted smoke test, POST log lines from JS to a local HTTP server.
- Runtime smoke test PASSED (2026-10-05) in the deployed app against a Node fake terminal:
  Nitro install + `createEcr17Client`, state events connecting → connected → disconnected,
  Winsock connect, a correct `s` frame on the wire with 3 retransmits ("no ACK after 4
  attempts"), and `isConnected() == false` after the terminal's FIN. Not yet run against
  a physical Nexi terminal.
- The generated app vcxproj sets `<WindowsTargetPlatformVersion>10.0</…>` UNCONDITIONALLY
  after importing ExperimentalFeatures.props, which defeats the SDK pin (RNW then bumps
  "10.0" to 10.0.22621 → MSB8036). Make it conditional (`'$(WindowsTargetPlatformVersion)' == ''`).
- WinRT projected types delete `operator new` (C2280): to leak one on purpose, wrap it
  in a plain struct and `new` the struct.
- Transport `connect()` must NOT use `Promise::async`: the client's `ensureConnected()`
  already blocks a Nitro ThreadPool worker on it, so with busy workers the connect
  queues and `connectionTimeoutMs` stops bounding it. Use a dedicated thread.

## Build wiring
- **Nitro C++ HybridObject impl header MUST be named after `implementationClassName`
  and be on the include path.** nitrogen's generated `Ecr17OnLoad.cpp` does a flat
  `#include "HybridEcr17Client.hpp"` (the impl class name from `nitro.json`). So the
  impl file must be `HybridEcr17Client.{hpp,cpp}` (not `Ecr17Client.*`) AND its dir
  must be in `CMakeLists.txt` `include_directories` (we added `../cpp/Ecr17Client`).
  This only surfaces when the example app actually depends on the package — the
  `android-build` CI compiles the package's C++ ONLY when a consumer autolinks it;
  before the example took the dependency, `HybridEcr17Client`/adapter were never
  truly compiled, hiding the bug. cpp-tests don't cover the client either.
- **Downcasting a `createHybridObject` result needs `dynamic_pointer_cast`**, not
  `static_pointer_cast`: `margelo::nitro::HybridObject` is a *virtual* base, so a
  static downcast is ill-formed. Null-check the result.
- **Android `.so` loading + autolinking**: a Nitro module still needs an
  autolinked `ReactPackage` so its native lib loads at runtime. `Ecr17Package`
  (`com.ecr17`, a `BaseReactPackage` returning no modules) loads `libEcr17.so`
  from its `companion init` (`Ecr17OnLoad.initializeNative()` → `System.loadLibrary`),
  which runs `JNI_OnLoad → registerAllNatives()` and registers the HybridObjects
  BEFORE JS creates them. RN CLI autolinking discovers it by globbing
  `*Package.{kt,java}` under `android/src/main/java`, inferring the FQN and
  emitting `new Ecr17Package()`. This requires **`packages/react-native-ecr17/react-native.config.js`**
  (declares the android/ios platforms) — it was missing despite being listed in
  `package.json` `files`, which can leave the package unlinked and the `.so`
  unloaded at runtime. The reference Nitro module (corasan/image-compressor) ships
  the same file; runtime load is verified only by running the example app.
- Android `packages/react-native-ecr17/android/CMakeLists.txt` lists the binding's C++ sources **explicitly**
  (every new `packages/react-native-ecr17/cpp/**/*.cpp` MUST be added there) and globs the core's
  `cpp/src/*.cpp`, found with `node --print require.resolve('@padosoft/ecr17/package.json')`.
- iOS: `nitro_module` globs `cpp/**/*.{hpp,cpp}` of the binding; the core is its own pod (`Ecr17`)
  or Swift package (`Ecr17`), linked by `native_dependency`.
- C++20 on both; Android NDK provides POSIX sockets in libc (no extra link lib).
- Include convention: the core's headers are `<ecr17/Name.hpp>`; the binding's are
  subdir-qualified from `packages/react-native-ecr17/cpp` (`"Ecr17Client/HybridEcr17Client.hpp"`).

## Kit split (`packages/ecr17/` = @padosoft/ecr17) — 2026-10-05
- The protocol core + `Ecr17Client` (auto-connect, pre-send probe, money-safe retry) moved
  out of Nitro into `packages/ecr17/`, namespace `padosoft::ecr17`. It needs its own `LrcMode`: in the
  binding's namespace the name is taken by nitrogen's generated enum, so the binding uses
  `namespace kit = padosoft::ecr17;` and never `using namespace` both (PaymentRequest,
  TokenizationRequest, ConnectionState… exist on both sides).
- The connect/retry orchestration used to be in `HybridEcr17Client`, untested except for
  `RetryPolicy`. In the Kit it is unit-tested against FakeTransport, including a test that
  replays EVERY command against a drop. A deliberate mutation of `shouldRetryAfterReconnect`
  (dropping `safeToRetry`) fails two tests: keep it that way.
- JS numbers → Kit ints go through `toInt` (throws on NaN/±inf/out-of-range instead of the
  undefined behaviour of a plain cast). Fractions are still truncated, as before: an amount
  computed with float math (`0.29 * 100 = 28.999…`) is the CALLER's problem to round.
- **CocoaPods + a C++-only Kit pod**: a Swift pod (the Nitro module) depending on a pod that
  doesn't define a module fails `pod install` ("does not define modules"). `DEFINES_MODULE=YES`
  with the generated module map works. A custom `s.module_map` with a relative `umbrella`
  dir does NOT: CocoaPods copies the map to `Target Support Files/`, where the path no longer
  resolves (48× "umbrella directory not found").
- `header_mappings_dir = "cpp/include"` puts the headers at
  `Pods/Headers/Public/Ecr17/ecr17/*.hpp`, so consumers include `<ecr17/…>` exactly
  as with CMake.
- (Superseded 2026-10-05, see "SwiftPM opt-in" below.) RN autolinking used to link the core's
  podspec by itself when the app listed it as a direct dependency.
- `nitro-module.gradle` (from @padosoft/native-modules) keeps the namespace in
  `ext.nitroModule`, which autolinking can't read: `react-native.config.js` must declare
  `android.packageName`.
- `@padosoft/native-modules` / `@padosoft/expo` are on the private GitHub Packages registry
  only (not npm yet). CI strips them (`scripts/strip-private-deps.ts`) except where the
  native build needs them (android-build, which then needs `GESCAT_NPM_TOKEN`).
  `@padosoft/native-modules` is a regular DEPENDENCY of the binding and the Kit (it has no
  deps or peers of its own). `@padosoft/expo` stays an OPTIONAL peer of the Kit: it peers on
  `expo`, so as a dependency it would drag Expo into bare and Windows apps.
- npm does not install the dependencies of a `file:` linked package into the app: they
  resolve from the linked package's own folder upward (here: the root `bun install`). So
  `apps/example-windows` needs no registry config for the private @padosoft packages — and an
  `.npmrc` with `${GESCAT_NPM_TOKEN}` would make npm FAIL for anyone without the variable.
- npm writes `package-lock.json` with `package.json`'s indentation: after editing a tab-indented
  `package.json`, a lockfile that used 2 spaces is rewritten whole. Re-serialize it with 2
  spaces to keep the diff to the real change.
- The Bash tool on macOS is zsh: an unquoted `$INC` is ONE argument (no word splitting);
  use `${=INC}` or an array (or `xargs`). BSD `sed -E` has no `\b`: use `perl -pi -e`.
- The private-registry token (`GESCAT_NPM_TOKEN`) is only in the interactive zsh profile, not
  in the agent's shell: `bun install` gets 401 unless run as `zsh -ic '… bun install'`.

## Rename + SwiftPM opt-in (PR #26 follow-ups) — 2026-10-05
- Names: npm `@padosoft/ecr17` (was `@padosoft/ecr17-kit`), pod + Swift product `Ecr17`, headers
  `<ecr17/…>`, CMake `ecr17::ecr17`. The RN binding's pod became `ReactNativeEcr17`: a pod name
  must be unique, and nitro.json `iosModuleName` names the pod, nitrogen's
  `<name>+autolinking.rb` and the Swift module. Android keeps `androidCxxLibName: "Ecr17"`
  (`libEcr17.so`), which is independent. Regenerate nitrogen after changing `iosModuleName`.
- **SwiftPM resolves a remote package from the ROOT of its git repo, by plain semver tag** (no
  subdirectory support). So `Package.swift` is at this repo's root (`path:
  "packages/ecr17/cpp"`), and the release workflow tags each `@padosoft/ecr17` version
  `X.Y.Z`. The old `1.0.0`/`1.1.0` tags predate it (old package): `@padosoft/ecr17` starts at
  2.0.0 so `upToNextMajorVersion` never resolves them.
- A SwiftPM C++ target works with `publicHeadersPath: "include"` + `cxxLanguageStandard:
  .cxx20`; a dependent C++ target includes `<ecr17/…>` (verified with a throwaway consumer
  package and `swift build` on macOS).
- **`@padosoft/native-modules` 1.5.0's `native_dependency` SPM branch is broken**: it calls
  `spec.spm_dependency(...)`, which neither CocoaPods (1.17 has no SPM code at all) nor RN
  defines. RN's bridge is a TOP-LEVEL `spm_dependency(spec, url:, requirement:, products:)`
  (react_native_pods.rb → scripts/cocoapods/spm.rb), and it also accepts a local path. Hence
  the "CocoaPods < 1.16" log line on CocoaPods 1.17, and a NoMethodError once
  `$NativeKitForceSPM` is set. Fix belongs in react-native-support.
- `native_dependency` args win over `.padosoft/native-kit.json`, so the binding's podspec passes
  the SPM coordinates itself (it knows where the core lives); the requirement's minimum is
  read from its `@padosoft/ecr17` dependency range, so pod and SwiftPM resolve the same major.
- **Autolinking vs SwiftPM**: RN autolinking adds a dependency's pod unless the Podfile already
  declared it, so when the plugin skips `pod 'Ecr17'` (SwiftPM on) autolinking would add it
  back. `@padosoft/ecr17` therefore opts out of autolinking (`react-native.config.js`,
  `platforms.ios: null`, like native-core-kit). The pod comes from the Expo plugin (a
  Podfile block that evaluates the same flags as `native_dependency`) or one Podfile line in
  a bare app.
- Scripts are `.ts`: root ones run with `bun`, `apps/docs/scripts` with `node` ≥ 22.18
  (type stripping; `apps/docs/.node-version` is 24, `"type": "module"` for top-level await).

## ECR17 protocol facts (from docs/)
- Status command code is lowercase `'s'` (0x73). Payment `'P'` request = 167 bytes.
- App frame = `STX(0x02)` payload `ETX(0x03)` `LRC`. LRC = `0x7F` XOR-folded;
  which framing bytes are folded is selected by `LrcMode` (configurable).
- Progress update = `SOH(0x01)` + 20-char message + `EOT(0x04)`, **no LRC**.
- Receipts arrive as one or more `S` messages (concatenate). Reversal = `'S'`.
- `decode()` must treat the buffer as exactly one frame (LRC = final byte);
  stream→frame splitting belongs to the transport layer.

## Review/CI learnings
- `packages/react-native-ecr17/tsconfig.json` and `biome.json` extend `@padosoft/config/*`, a
  **private package not on npm** (404). So the repo's own `bun run typecheck`
  fails in any clean env with `File '@padosoft/config/typescript/base' not found`.
  Workaround/fix: a committed self-contained `packages/react-native-ecr17/tsconfig.ci.json` (no
  private `extends`) used for local + CI typecheck. `react-native-nitro-modules`
  and `@types/react` ARE installed, so a standalone tsconfig resolves fine.
- Nitrogen parsing of the `.nitro.ts` specs is itself strong type validation:
  if `nitrogen` succeeds, the spec types are nitro-compatible. Generated async
  signature confirmed: `Promise<T>` → `virtual std::shared_ptr<Promise<T>> m() = 0;`
  and callbacks → `const std::function<void(const T&)>&`.
- **`copilot --autopilot --yolo -p "/review …"` does NOT just report — it EDITS
  and COMMITS autonomously.** Treat its output as proposals to VERIFY, never trust
  blindly (receiving-code-review discipline). In Phase 0 it: (good) added an
  EOT-terminator check to the SOH decode branch + a regression test; (bad) wrote
  `Ecr17Client` stubs that would not compile.
- **Copilot's Nitro/C++ knowledge is unreliable** — it got two things wrong:
  1. `Promise<T>::async` takes `std::function<T()>` (lambda **no args, returns T**).
     Copilot wrote `[](auto& res){ … }` (resolver-style) → won't compile. Correct
     stub: `Promise<T>::async([]() -> T { throw …; })`; for void:
     `Promise<void>::async([]() { throw …; })`.
  2. Callback param types must EXACTLY match the generated spec. Enums are passed
     **by value**: spec is `std::function<void(ConnectionState)>` (NOT
     `const ConnectionState&`); structs are `const T&`
     (`const ProgressEvent&`, `const ReceiptLine&`). A mismatched `override`
     silently becomes a new method → "cannot instantiate abstract class".
- **EOT check (kept from Copilot):** SOH/progress frames must end in `EOT (0x04)`;
  `decode()` now rejects SOH frames whose last byte != EOT.
- The C++ unit-test target does NOT compile `Ecr17Client.cpp` (only Lcr,
  PacketCodec, Ecr17Protocol, Ecr17Response, Session + tests), so client-layer
  compile errors are NOT caught by the GoogleTest CI — they need the Android/iOS
  build jobs (Phase 8). Verify client/nitro C++ against the generated headers by
  hand until then.
- **Local Copilot review prompt efficiency:** feeding a big patch file makes the
  CLI read it in chunks and frequently TIME OUT (saw 124 at 540s twice). A
  focused prompt — "read ONLY file X and Y, check these N specific things, answer
  in <=K lines" — finishes in <1 min and is reliable. Prefer per-file/targeted
  reviews over dumping the whole branch diff.
- Test exe using std::thread/condition_variable needs `find_package(Threads)` +
  link `Threads::Threads` on Linux CI (Android gets pthread via libc).
- `LrcMode.hpp` is flat (nitrogen-generated / test stub at tests/stubs), NOT under
  `Lcr/`. Include `"Lcr/Lcr.hpp"` (which includes "LrcMode.hpp") — never
  `"Lcr/LrcMode.hpp"`.
- Phase-3 session orchestration with real threads/condvar compiled & passed in CI
  (74 tests) — FakeTransport delivers scripted replies synchronously on each STX
  send, so happy-path tests don't actually block; only timeout tests wait (use
  tiny timeouts, e.g. 40ms).
- **Private dep `@padosoft/config`** is a GitHub Packages package (root
  package.json devDep `^1.2.2`); `bunfig.toml` maps the `padosoft` scope to
  `https://npm.pkg.github.com/` via `$GESCAT_NPM_TOKEN`. ANY CI job running
  `bun install` (typecheck, nitrogen, native build) needs it → set
  `env: GESCAT_NPM_TOKEN: ${{ secrets.GESCAT_NPM_TOKEN || secrets.GITHUB_TOKEN }}`
  + `permissions: packages: read`. The TS source never imports it (only
  tsconfig/biome `extends` it).
- The example app is **Expo managed** (RN 0.85, expo ~56, no committed `android/`):
  a native CI build needs `expo prebuild` then gradle/xcode — heavy, best-effort.
- **Android native build pipeline WORKS** (verified green): checkout → setup-java 17 →
  setup-bun → install(strip @padosoft) → `bunx nitrogen` → `bunx expo prebuild -p android` →
  android-actions/setup-android → `sdkmanager "ndk;27.1.12297006" "cmake;3.22.1"` →
  `./gradlew assembleDebug` (working-dir apps/example/android). ~15-20 min. Manual dispatch only.
- **Verified Nitro C++ APIs** (compiled into the APK, use as-is):
  - `#include <NitroModules/HybridObjectRegistry.hpp>`;
    `auto o = HybridObjectRegistry::createHybridObject("Ecr17Transport");`
    `auto t = std::dynamic_pointer_cast<HybridEcr17TransportSpec>(o);` (NOT
    static_pointer_cast — HybridObject is a virtual base; null-check `t`). Call
    `createHybridObject` on the JS thread (app class loader) — see Runtime (Android).
  - `#include <NitroModules/ArrayBuffer.hpp>`; `ArrayBuffer::copy(const std::vector<uint8_t>&)`,
    `buf->data()` / `buf->size()`.
  - Transport spec uses `std::shared_ptr<ArrayBuffer>` (NOT std::vector) for send/onData.
  - `Promise<T>::async([]() -> T {...})` (returns T) — confirmed compiling.
- **Verified Nitro Kotlin APIs**: `class X : HybridEcr17TransportSpec()`;
  `Promise.parallel { ... }` (blocking on a thread) / `Promise.async { suspend }`;
  `ArrayBuffer.copy(ByteArray)`, `arrayBuffer.toByteArray()`. Impl goes in
  `packages/react-native-ecr17/android/src/main/java/com/margelo/nitro/ecr17/`. Nitro auto-generates
  the C++↔Kotlin JNI bridge — NO manual JNI needed (corasan ref was for non-nitro).
- **Name-clash trap**: parser structs in our namespace must NOT reuse a
  Nitro-generated struct name (had to rename our `CurrencyExchange` → `DccInfo`,
  since the generated `CurrencyExchange` is in the same `margelo::nitro::ecr17`).
- iOS: Swift transport verified on device by the dev (no macOS CI runner).
- **Bug found by a user question (auto-reconnect):** `Ecr17Session::disconnected_`
  was set on a drop and NEVER reset, so after the client reconnected the transport
  the next `exchange()` threw "disconnected" immediately → auto-reconnect didn't
  actually recover. Fix: `resetForNewTransaction()` (clear `disconnected_` +
  `rxBuffer_`) at the start of every exchange/sendAckOnly; the session is now
  reusable across reconnects. Regression test: `Session.RecoversAndSucceedsAfterReconnect`
  (FakeTransport `disconnectOnNextRequest()` then `rearm()`).
- **Legal:** public web docs are NOT free to republish; attribution ≠ license,
  quotation exceptions cover short excerpts only. The full Nexi doc was untracked
  (`git rm --cached` + .gitignore), kept local/private; README links the official
  public URL. NOTE: it's still in git history (PR #3 merged) — a history rewrite
  (git filter-repo) is needed if full removal is required.
