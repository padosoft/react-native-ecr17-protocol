# PROGRESS — ECR17 transport & full command set

Branch: `feat/ecr17-transport-and-commands` (off PR #3 branch `fix/ecr17-spec-compliance`).
Plan: `~/.claude/plans/vivid-honking-bengio.md`.
Target PR: to be opened against `main` once Phase 0 lands (or reuse a draft).

## Per-phase Definition of Done (loop)
1. Local tests green (`ctest` + TS typecheck where applicable).
2. Local Copilot review: `copilot --autopilot --yolo -p "/review <full branch diff vs origin/main>"` (save diff to temp file if large). Record learnings in `docs/LESSON.md`.
3. Zero actionable review comments → continue; else fix and go to 1.
4. Push branch.
5. CI all green; else fix and go to 1.
6. Phase done → continue to next phase in automode.

## Status
- [~] Phase 0 — TS spec async + types + events + transport spec + nitrogen  → code done, typecheck green, in review/push loop
- [ ] Phase 1 — all command builders (Ecr17Protocol)
- [ ] Phase 2 — response parsers (Ecr17Response)
- [ ] Phase 3 — Ecr17Session orchestration + FakeTransport
- [ ] Phase 4 — HybridEcr17Client async + events
- [ ] Phase 5 — native transport (Swift/Kotlin + JNI; ref corasan/image-compressor#11)
- [ ] Phase 6 — C++ tests expanded
- [ ] Phase 7 — README (Roadmap/Feature status ✅) + example
- [ ] Phase 8 — CI: typecheck + nitrogen check + Android/iOS build jobs
- [ ] Phase 9 — distill LESSON.md into AGENTS.md / rules / skills

## Current task
Phase 0: redesign `package/src/specs/client.nitro.ts` (async commands + events),
add `transport.nitro.ts`, new result types, update `nitro.json`, run nitrogen.

## Notes to resume
- Cannot compile natively locally (no toolchain). C++ unit target (GoogleTest) is the local/CI gate; native verified only via CI build jobs (Phase 8).
- `copilot` CLI present (v1.0.54).
- Generated `nitrogen/generated/**` is gitignored (regenerated via `./node_modules/.bin/nitrogen`).
