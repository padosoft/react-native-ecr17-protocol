# LESSON.md — accumulated learnings (ECR17 module)

> **Context rule:** the content of this file MUST be passed into the prompt of
> every parallel subagent, and re-read at the start of every new session, so
> hard-won knowledge is never lost. Update it continuously — especially after
> Copilot/CI feedback and after fixing any bug.

## Environment & tooling
- Host is **Windows**; the `Bash` tool runs **bash**, the `PowerShell` tool runs pwsh.
  ⚠️ Do **not** use PowerShell here-strings (`@'...'@`) inside the Bash tool —
  the `@` leaks into the arg. Use a bash heredoc (`<<'EOF' … EOF`) or `git commit -F -`.
- **No local C++/native toolchain** (`g++`/`clang++`/`cl` absent). Local + CI
  verification of protocol logic goes through the **standalone GoogleTest target**
  (`package/cpp/tests`, GoogleTest via CMake FetchContent, Ubuntu CI). Native
  Swift/Kotlin + Nitro-integrated C++ are **not** locally compilable.
- `copilot` CLI present (v1.0.54) for the local review loop.
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

## Build wiring
- Android `package/android/CMakeLists.txt` lists C++ sources **explicitly** — every
  new `package/cpp/**/*.cpp` MUST be added there or it won't link (undefined symbols).
- iOS `package/Ecr17.podspec` globs `cpp/**/*.{hpp,cpp}` — new C++ auto-included.
- C++20 on both; Android NDK provides POSIX sockets in libc (no extra link lib).
- Include convention: cross-unit includes are subdir-qualified from the `../cpp`
  root, e.g. `#include "Lcr/Lcr.hpp"`, `#include "Ecr17Client/Ecr17Client.hpp"`.

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
- (add entries as Copilot/CI feedback arrives)
