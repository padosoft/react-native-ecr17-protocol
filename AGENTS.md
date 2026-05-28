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

## Mandatory per-phase workflow (Definition of Done)
For multi-phase work, a phase is done ONLY after this loop; in auto mode proceed
to the next phase only once complete:
1. **Local tests green** — C++: `cmake -S package/cpp/tests -B build && cmake --build build && ctest --test-dir build`. TS: `cd package && bunx tsc --noEmit -p tsconfig.ci.json`.
2. **Local Copilot review** — `copilot --autopilot --yolo -p "/review …"`. Use a
   **focused** prompt ("read ONLY file X, check N things, answer in <=K lines") —
   feeding a whole diff times out. Copilot **edits in --yolo mode** and its
   nitro/C++ suggestions are sometimes wrong: VERIFY against the generated
   headers, don't trust blindly. Record takeaways in `docs/LESSON.md`.
3. **Zero actionable comments** → continue; else fix and go to 1.
4. **Push**, then **CI green**; else fix and go to 1.
5. Update `PROGRESS.md`; continue to the next phase.

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
- **No local C++/native toolchain** here; CI is the compiler. The unit target
  compiles only `Lcr/PacketCodec/Ecr17Protocol/Ecr17Response/Session` + tests —
  NOT `Ecr17Client`/adapter/native. Verify those via the Android build.
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
