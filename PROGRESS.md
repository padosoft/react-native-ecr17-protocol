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
- [x] Phase 4 — HybridEcr17Client wiring + NativeTransportAdapter + sendAckOnly  ✅ written (cpp-tests 77/77; client/adapter verified only by Android build)
- [x] Phase 5 — native Kotlin (HybridEcr17Transport.kt) + Swift (best-effort, no iOS CI)  ✅ written
- [x] Phase 8b — native Android build CI  ✅ GREEN on first real attempt (run 26562706306); client/adapter/Kotlin/Nitro all compiled+linked into APK
- [x] Phase 7 — README (Roadmap/Feature status ✅, async API, events, hook)  ✅ DONE
- [x] Phase 6 — tests done inline per phase (cpp-tests 77/77)  ✅
- [x] Phase 9 — root AGENTS.md distilled from LESSON.md (workflow + nitro/build know-how)  ✅
- Final confirming Android build dispatched after the Kotlin reconnect/disconnect fix (run 26564115692).

### Roadmap COMPLETED (this session)
- [x] Auto-connect/keepAlive (ensureConnected via transport await; socket kept open)
- [x] Tokenization (U) flow wired (exchangeWithAdditionalData; pay/preAuth/verify)
- [x] Receipt streaming after result (SessionConfig.receiptDrainMs + Ecr17Config field)
- [x] Opt-in real-terminal test (PosixTcpTransport + test_integration_terminal, env-gated)
- [x] PR #4 retargeted to main (#3 merged)
- cpp-tests 80/80 green; client/native verified by Android build.

- [x] Auto-reconnect on mid-session drop — SAFE policy (financial never replayed;
      RetryPolicy.hpp unit-tested; recover via sendLastResult 'G'). cpp 82/82.
- [x] Money-safety reviewed by Copilot (no double-charge path) + README enterprise section.

### Genuinely remaining (need hardware/macOS — documented in README roadmap)
- iOS Swift transport verification: NO macOS CI runner. Swift written best-effort.
- (everything else is implemented + verified: Android build green, cpp 82/82, ts-checks green)

### Native build iteration (Phase 8b)
Trigger: `gh workflow run "Android build" --ref feat/ecr17-transport-and-commands`. ~15-20 min.
Likely first-run errors to fix (unverifiable locally): nitro include paths
(<NitroModules/ArrayBuffer.hpp>, <NitroModules/HybridObjectRegistry.hpp>),
HybridObjectRegistry::createHybridObject usage, Kotlin ArrayBuffer/Promise API,
Swift ArrayBuffer bridging (iOS has NO CI — best-effort). Batch fixes, re-dispatch.

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

## Native impl spec (turnkey — all APIs confirmed from generated headers)
Transport C++ spec (generated): `connect(string host,double port,double timeoutMs)->Promise<void>`,
`disconnect()`, `isConnected()->bool`, `send(shared_ptr<ArrayBuffer>)`,
`setOnData((shared_ptr<ArrayBuffer>)->void)`, `setOnDisconnect(()->void)`.
- ArrayBuffer: `ArrayBuffer::copy(const std::vector<uint8_t>&)` to send; read via
  `buf->data()` (uint8_t*) + `buf->size()` (owning native buffers are thread-safe).
- Registry: `#include <NitroModules/HybridObjectRegistry.hpp>`;
  `auto obj = HybridObjectRegistry::createHybridObject("Ecr17Transport");`
  `auto t = std::static_pointer_cast<HybridEcr17TransportSpec>(obj);`
- NativeTransportAdapter : Transport — store host/port/timeout (from config);
  connect() -> t->connect(host,port,timeout); send(vector)-> t->send(ArrayBuffer::copy(v));
  setDataCallback -> t->setOnData([cb](buf){ cb(vector(buf->data(),buf->data()+buf->size())); }).
- Native impls (auto-globbed iOS / android src): HybridEcr17Transport.kt extends
  HybridEcr17TransportSpec() (java.net.Socket + reader thread -> onData(ArrayBuffer));
  HybridEcr17Transport.swift extends HybridEcr17TransportSpec (Network.framework).
  Nitro generates the JNI bridge for the Kotlin HybridObject (no manual JNI).
- Client (Phase 4): own NativeTransportAdapter + Ecr17Session(adapter, SessionConfig
  from config_). Commands: Promise<T>::async([this,req]{ ensureConnected();
  auto pkt = session.exchange(Ecr17Protocol::buildX(...)); auto p = Ecr17Response::parseX(pkt.payload);
  return mapToNitro(p); }). Map: outcome->TransactionOutcome (OK/KO/CARDNOTPRESENT/
  UNKNOWNTAG/UNKNOWN), cardType "1"/"2"/"3"->DEBIT/CREDIT/OTHER else UNKNOWN,
  entryMode ICC/MAG/MAN/CLM/CLI->ICC/MAG/MANUAL/CLESSMAG/CLESSICC. Optional strings
  -> std::optional (empty => nullopt). amountCents is double in requests (cast int).
- Verify ONLY via Android build job (workflow_dispatch). Batch native edits; one build per batch.

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
