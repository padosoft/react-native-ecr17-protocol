# CLAUDE.md

This repository ships **first-class context for AI coding agents** ("vibe-coding
batteries included"). If you're an AI assistant working here, start with these:

- **[AGENTS.md](AGENTS.md)** — project guide, the mandatory per-phase workflow,
  CI strategy, and verified Nitro/build know-how. **Read it first.**
- **[docs/LESSON.md](docs/LESSON.md)** — accumulated engineering lessons
  (environment, Nitro APIs, build traps, payment safety). Re-read at the start of
  every session, and pass its content into every sub-agent prompt you spawn.
- **[PROGRESS.md](PROGRESS.md)** — current task / resume state for crash-safe
  continuation across sessions.

## Non-negotiables

- 💰 **Money-critical:** a **financial command is never blindly re-sent** after a
  reconnect (double-charge risk). The decision lives in
  `package/cpp/Session/RetryPolicy.hpp` and is locked by `test_retry_policy.cpp`;
  recovery from a lost response is via `sendLastResult()` (spec command `G`).
- **Keep CI green** — `cpp-tests` (the protocol core, fully unit-tested) and
  `ts-checks`. The native Android build is a manual-dispatch workflow.
- **Per-phase loop:** local tests → local Copilot review → push → CI green.
- The official protocol source is the public Nexi developer portal; do **not**
  re-publish the full vendor docs in the repo (kept local/private).
