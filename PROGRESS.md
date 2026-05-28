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
- [x] Phase 0 — TS spec async + types + events + transport spec + nitrogen  ✅ DONE (PR #4 stacked on #3, CI green)
- [x] Phase 1 — all command builders (Ecr17Protocol) + builder tests  ✅ DONE (CI 55/55)
- [x] Phase 2 — response parsers (Ecr17Response) + parser tests  ✅ DONE (CI 66/66)
- [x] Phase 3 — Ecr17Session orchestration + FakeTransport + session tests  ✅ DONE (CI 74/74)
- [x] Phase 8a — ts-checks CI (typecheck + nitrogen), checkout@v5  ✅ GREEN
- [~] Phase 8b — native Android build CI (baseline current code, then verifies Phase 4/5)  ⏳ IN PROGRESS
- [~] Phase 4 — HybridEcr17Client wiring (build->session->parse->map) + NativeTransportAdapter  ⏳ NEXT (after Android baseline green)

### Blocker RESOLVED
CI `bun install` 403 on private @padosoft/config is fixed by STRIPPING that
tooling-only devDep before install in CI (jq del + rm bun.lock + bun install).
No token / package-visibility change needed. Use the SAME trick in the Android
build job. Org disallows making the package public anyway.

### CI note
- `cpp-tests.yml` uses actions/checkout@v4 (Node20 deprecation warning) → bump to @v5 in Phase 8.
- [ ] Phase 3 — Ecr17Session orchestration + FakeTransport
- [ ] Phase 4 — HybridEcr17Client async + events
- [ ] Phase 5 — native transport (Swift/Kotlin + JNI; ref corasan/image-compressor#11)
- [ ] Phase 6 — C++ tests expanded
- [ ] Phase 7 — README (Roadmap/Feature status ✅) + example
- [ ] Phase 8 — CI: typecheck + nitrogen check + Android/iOS build jobs
- [ ] Phase 9 — distill LESSON.md into AGENTS.md / rules / skills

## Current task / resume notes
Phase 4: wire HybridEcr17Client to real logic:
- Hold a Transport (NativeTransportAdapter wrapping a generated
  HybridEcr17TransportSpec obtained via HybridObjectRegistry::createHybridObject
  ("Ecr17Transport")) + an Ecr17Session built from SessionConfig(config_).
- configure(): store config + (re)build SessionConfig (lrcMode/timeouts/retry).
- connect()/disconnect()/isConnected(): delegate to transport (host/port/
  connectionTimeoutMs); fire onConnectionStateChange_.
- Each command (Promise<T>::async): ensureConnected -> Ecr17Protocol::buildX(...)
  -> session.exchange(payload) -> Ecr17Response::parseX(pkt.payload) -> map to the
  generated Nitro result struct (field-copy; CHECK exact generated field names in
  nitrogen/generated/shared/c++/<Type>.hpp). Map outcome enum->TS union string,
  cardType '1'/'2'/'3'->debit/credit/other, entryMode ICC/MAG/...->union.
- Wire session.setOnProgress/onReceiptLine -> onProgress_/onReceiptLine_ (ProgressEvent{message}/ReceiptLine{text}).
NOTE: Phase 4/5 are NOT compilable by the C++ unit CI (nitro-coupled). Verified
only by the Phase-8 Android/iOS build jobs. Keep mapping mechanical; the risky
logic (build/parse/session) is already CI-tested. Match generated struct field
names EXACTLY. Promise<T>::async([](){ return T; }) — see Ecr17Client.cpp stubs.

## Notes to resume
- Cannot compile natively locally (no toolchain). C++ unit target (GoogleTest) is the local/CI gate; native verified only via CI build jobs (Phase 8).
- `copilot` CLI present (v1.0.54).
- Generated `nitrogen/generated/**` is gitignored (regenerated via `./node_modules/.bin/nitrogen`).
