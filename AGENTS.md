# AGENTS.md — react-native-ecr17-protocol

Guidance for AI agents (and humans) working in this repo. Read this first, then
`PROGRESS.md` (current task/resume state) and `docs/LESSON.md` (accumulated,
hard-won engineering lessons). **Always pass `docs/LESSON.md` into the prompt of
any sub-agent you spawn, and re-read it when starting a new session.**

## What this is
A React Native **Nitro** module implementing the Italian **ECR17** payment
protocol (Nexi Group POS terminals) over LAN. Protocol engine in C++ (shared),
native TCP transport in Kotlin/Swift, async Promise API to JS.

Layered C++ core (`package/cpp/`, namespace `margelo::nitro::ecr17`):
`Lcr` (LRC) → `PacketCodec` (framing) → `Ecr17Protocol` (builders) →
`Ecr17Response` (parsers) → `Ecr17Session` (ACK/NAK + retransmit + timeout) →
`HybridEcr17Client` (Nitro async API). Transport: abstract `Transport` +
`NativeTransportAdapter` (wraps the Nitro `Ecr17Transport` HybridObject) +
`FakeTransport` (tests). Spec reference vendored in `docs/`.

## Mandatory workflow (Definition of Done)
A task/phase is done ONLY after BOTH loops below pass. In auto mode, proceed to
the next phase only once complete.

### Local loop (per phase, before pushing)
1. **Local tests green** — C++: no local cmake/gtest here, so the local check is a
   throwaway **g++ harness** that compiles the unit-testable core
   (`Lcr/PacketCodec/Ecr17Protocol/Ecr17Response/Session`) and asserts the changed
   behavior — `g++ -std=c++20 -I package/cpp -I package/cpp/tests/stubs <harness>.cpp <core>.cpp`
   (see "local toolchain" below). The full GoogleTest suite (`cmake -S package/cpp/tests …`)
   runs in **CI**, not locally. TS: `cd package && bunx tsc --noEmit -p tsconfig.ci.json`.
2. **Local Copilot review** — `copilot --autopilot --yolo -p "/review …"`. Use a
   **focused** prompt ("read ONLY file X, check N things, answer in <=K lines") —
   feeding a whole diff times out. Copilot **edits in --yolo mode** and its
   nitro/C++ suggestions are sometimes wrong: VERIFY against the generated
   headers, don't trust blindly. Record takeaways in `docs/LESSON.md`.
3. **Zero actionable comments** → continue; else fix and go to 1.

### Remote loop (REQUIRED before a task/PR is considered done)
4. **Push**, then **CI green** (`cpp-tests` + `ts-checks`); else fix → local loop.
   **If the PR touches native code** (`package/android/**`, `package/ios/**`, the
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
- `cpp-tests` (fast, ~1 min): the real correctness gate — builds/runs the
  standalone GoogleTest suite. Keep it green.
- `ts-checks` (fast): typecheck + nitrogen codegen.
- `android-build` (~15-20 min, **manual dispatch only**:
  `gh workflow run "Android build" --ref <branch>`): compiles C++/Kotlin/Nitro
  via expo prebuild + gradle. The ONLY verifier of client/adapter/native code
  (not in the unit target). Batch native edits; run sparingly. **No iOS CI** →
  Swift is best-effort.

## Hard-won rules (see docs/LESSON.md for the full list)
- 💰 **Money-critical — never blindly retry a financial command.** This terminal
  charges real cards. On a drop, reconnect the socket but do NOT re-send
  payments/reversals/pre-auths (double-charge); recover via `sendLastResult()`
  (command `G`). The decision is in `package/cpp/Session/RetryPolicy.hpp`, locked
  by `test_retry_policy.cpp`. `Ecr17Session` resets its connection state per
  transaction (`resetForNewTransaction`) so it's reusable across reconnects.
- **Local C++ toolchain**: a WinLibs **g++ 16** (MinGW, UCRT) is installed at
  `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64\bin\g++.exe`
  — enough to compile the unit-testable core (`Lcr/PacketCodec/Ecr17Protocol/Ecr17Response/Session`)
  with `-std=c++20 -I package/cpp -I package/cpp/tests/stubs` for a quick local
  RED→GREEN harness. (No cmake/gtest locally; the full GoogleTest suite still runs
  in CI.) Avira may quarantine a freshly-built `.exe` — a one-time AV exclusion
  fixes it. There is NO local Android/iOS native build: `Ecr17Client`/adapter/
  Kotlin/Swift are verified ONLY via the Android build CI job.
- The `Bash` tool is **bash** (use heredocs / `git commit -F -`, never PowerShell
  here-strings like `@'…'@`).
- `@padosoft/config` is a **private GitHub Packages** root devDep that blocks
  `bun install` in CI. It's tooling-only → CI strips it: `jq 'del(.devDependencies["@padosoft/config"])' package.json > t && mv t package.json && rm -f bun.lock && bun install`.
- `nitrogen/generated/**` is **gitignored** (regenerate with `cd package && bunx nitrogen`).
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
  3. Autolinking + `.so` load needs `package/react-native.config.js` (declares
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
- C++20. Cross-unit includes are subdir-qualified from `package/cpp` (e.g.
  `#include "Lcr/Lcr.hpp"`). New `package/cpp/**/*.cpp` MUST be added to
  `package/android/CMakeLists.txt` (iOS auto-globs via the podspec) and, if
  unit-testable, to `package/cpp/tests/CMakeLists.txt`.
- Commit messages: gitmoji-free conventional style; end with the Co-Authored-By
  trailer. Branch + PR per feature; keep CI green per push.
