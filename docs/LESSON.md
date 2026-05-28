# LESSON.md — accumulated learnings (ECR17 module)

> **Context rule:** the content of this file MUST be passed into the prompt of
> every parallel subagent, and re-read at the start of every new session, so
> hard-won knowledge is never lost. Update it continuously — especially after
> Copilot/CI feedback and after fixing any bug.

## Environment & tooling
- Host is **Windows**; the `Bash` tool runs **bash**, the `PowerShell` tool runs pwsh.
  ⚠️ Do **not** use PowerShell here-strings (`@'...'@`) inside the Bash tool —
  the `@` leaks into the arg. Use a bash heredoc (`<<'EOF' … EOF`) or `git commit -F -`.
- **Local C++ toolchain available**: WinLibs **g++ 16** (MinGW/UCRT) at
  `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64\bin\g++.exe`
  (installed via `winget install BrechtSanders.WinLibs.POSIX.UCRT`). Compile the
  unit-testable core into a throwaway harness for a real local RED→GREEN check:
  `g++ -std=c++20 -I package/cpp -I package/cpp/tests/stubs <harness>.cpp <core>.cpp`.
  ⚠️ The preinstalled MSVC (VS18) is broken — its STL `include/` dir is missing, so
  `cl` can't compile; use g++. ⚠️ Avira quarantines a freshly-built `.exe`
  (false positive) → add a one-time AV exclusion for the build dir. No cmake/gtest
  locally, so the full GoogleTest suite still runs in **CI** (`package/cpp/tests`,
  Ubuntu). Native Swift/Kotlin + Nitro-integrated C++ are **not** locally
  compilable → verified only by the Android build CI job.
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
  emitting `new Ecr17Package()`. This requires **`package/react-native.config.js`**
  (declares the android/ios platforms) — it was missing despite being listed in
  `package.json` `files`, which can leave the package unlinked and the `.so`
  unloaded at runtime. The reference Nitro module (corasan/image-compressor) ships
  the same file; runtime load is verified only by running the example app.
- Android `package/android/CMakeLists.txt` lists C++ sources **explicitly** — every
  new `package/cpp/**/*.cpp` MUST be added there or it won't link (undefined symbols).
- iOS `package/Ecr17.podspec` globs `cpp/**/*.{hpp,cpp}` — new C++ auto-included.
- C++20 on both; Android NDK provides POSIX sockets in libc (no extra link lib).
- Include convention: cross-unit includes are subdir-qualified from the `../cpp`
  root, e.g. `#include "Lcr/Lcr.hpp"`, `#include "Ecr17Client/HybridEcr17Client.hpp"`
  (the client impl file is named after its Nitro class — see Build wiring).

## ECR17 protocol facts (from docs/)
- Status command code is lowercase `'s'` (0x73). Payment `'P'` request = 167 bytes.
- App frame = `STX(0x02)` payload `ETX(0x03)` `LRC`. LRC = `0x7F` XOR-folded;
  which framing bytes are folded is selected by `LrcMode` (configurable).
- Progress update = `SOH(0x01)` + 20-char message + `EOT(0x04)`, **no LRC**.
- Receipts arrive as one or more `S` messages (concatenate). Reversal = `'S'`.
- `decode()` must treat the buffer as exactly one frame (LRC = final byte);
  stream→frame splitting belongs to the transport layer.

## Review/CI learnings
- `package/tsconfig.json` and `biome.json` extend `@padosoft/config/*`, a
  **private package not on npm** (404). So the repo's own `bun run typecheck`
  fails in any clean env with `File '@padosoft/config/typescript/base' not found`.
  Workaround/fix: a committed self-contained `package/tsconfig.ci.json` (no
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
  `./gradlew assembleDebug` (working-dir example/android). ~15-20 min. Manual dispatch only.
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
  `package/android/src/main/java/com/margelo/nitro/ecr17/`. Nitro auto-generates
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
