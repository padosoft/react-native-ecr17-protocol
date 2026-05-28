<div align="center">

# 💳 react-native-ecr17

**A React Native / Nitro module for the Italian ECR17 payment protocol — drive Nexi Group POS terminals over LAN, straight from your cash-register app.**

**The most complete open-source ECR17 toolkit for React Native & native mobile (iOS/Android).**

[![npm version](https://img.shields.io/npm/v/react-native-ecr17.svg?style=flat-square)](https://www.npmjs.com/package/react-native-ecr17)
[![npm downloads](https://img.shields.io/npm/dm/react-native-ecr17.svg?style=flat-square)](https://www.npmjs.com/package/react-native-ecr17)
[![License: MIT](https://img.shields.io/npm/l/react-native-ecr17.svg?style=flat-square)](https://github.com/padosoft/react-native-ecr17-protocol/blob/main/LICENSE)
[![C++ tests](https://github.com/padosoft/react-native-ecr17-protocol/actions/workflows/cpp-tests.yml/badge.svg)](https://github.com/padosoft/react-native-ecr17-protocol/actions/workflows/cpp-tests.yml)
[![Built with Nitro](https://img.shields.io/badge/built%20with-Nitro-8B5CF6?style=flat-square)](https://nitro.margelo.com)
[![Platforms](https://img.shields.io/badge/platforms-iOS%20%7C%20Android-555?style=flat-square)](#requirements)

</div>

> 🐘 **Using PHP / Laravel?** There's a sibling port:
> **[padosoft/laravel-ecr17](https://github.com/padosoft/laravel-ecr17)** —
> the same ECR17 protocol as a Laravel package + debug console.

---

## 📚 Table of contents

- [What is ECR17?](#-what-is-ecr17)
- [Why this exists](#-why-this-exists)
- [Highlights](#-highlights)
- [Feature status](#-feature-status)
- [Requirements](#requirements)
- [Installation](#-installation)
- [Quick start](#-quick-start)
- [React hook example](#-react-hook-example)
- [Configuration](#%EF%B8%8F-configuration)
- [API reference](#-api-reference)
- [Events](#-events)
- [Protocol cheat-sheet](#-protocol-cheat-sheet)
- [Architecture](#%EF%B8%8F-architecture)
- [Testing](#-testing)
- [Vibe-coding batteries included](#-vibe-coding-batteries-included)
- [License](#-license)

## 🧭 What is ECR17?

**ECR17** is the Italian standard protocol — supported by **Nexi Group** terminals —
that integrates an *Electronic Cash Register* (ECR) with an *EFT-POS* payment
terminal over a local LAN connection. The cash register sends a request
(payment, reversal, status…), the terminal talks to the acquiring host, and
replies synchronously.

This library speaks that protocol from React Native, with the protocol engine
written in C++ and bridged via [Nitro Modules](https://nitro.margelo.com).

> 📚 **Official protocol reference (public):**
> <https://developer.nexigroup.com/traditionalpos/en-EU/docs/> — the
> authoritative source. Field positions, message codes and `lrcMode` may vary by
> terminal/firmware; always check against the official docs.

## 🎯 Why this exists

Integrating Italian POS terminals has long been needlessly painful. The ECR17
protocol is **not publicly documented** — the specifications are shared under NDA,
mostly with established point-of-sale software vendors — so everyone else
reverse-engineers it by trial and error across terminals and firmware versions.
(The classic trap that blocks almost everyone: the LRC is computed over a base of
`0x7F`, not `0x00` — handled here, and configurable per terminal.)

A few community efforts exist for server-side languages, but there was **nothing
for React Native or native mobile (iOS/Android)**. To our knowledge this is the
**most complete open-source ECR17 toolkit for React Native and native mobile**:
the full command set, response parsing, the ACK/NAK + retransmit orchestration,
configurable LRC modes, and payment-safety — all tested.

The goal is simple: **low-level, Android and iOS developers should no longer
struggle to talk to Italian POS terminals.** No NDA hunting, no guesswork — just
`await client.pay({ amountCents })`. These protocols should be this approachable
for everyone, and now, for mobile, they are.

> 🤝 Compatibility notes (lrcMode, field quirks per terminal/firmware) are
> welcome as issues, so we can build, together, the reference the ecosystem
> never had.

## ✨ Highlights

- ⚡️ **C++ protocol core, Nitro-bridged** — framing/LRC/orchestration run natively on iOS & Android.
- 🔄 **Async, Promise-based API** — `await client.pay({ amountCents })`.
- 🧱 **Full command set** — payment, extended payment, reversal, pre-auth (request/incremental/closure), card verification, close session, totals, last result, ECR printing, reprint, VAS.
- 🛡️ **Robust by design** — fixed-width field validation, defensive response parsing, ACK/NAK handshake with **retransmit-up-to-3** and timeouts.
- 📡 **Live events** — progress messages, streamed receipt lines, connection state.
- 🧩 **Shared C++ ↔ native bridge** — one C++ protocol engine talks to the native
  TCP socket (Kotlin/Swift) through Nitro's auto-generated **C++↔Kotlin JNI**
  bridge — a notoriously fiddly piece on Android, here done cleanly with no
  hand-written JNI.
- ✅ **Heavily tested** — 83 C++ unit/flow/safety tests (LRC, codec, every builder, every parser, full session orchestration) run in CI.
- 🤖 **Vibe-coding batteries included** — ships first-class AI-agent context
  (`AGENTS.md`, `CLAUDE.md`, `docs/LESSON.md`, `PROGRESS.md`) so contributors
  using AI assistants get accurate, instant project context. See [below](#-vibe-coding-batteries-included).

## 🛡️ Enterprise robustness & payment safety

This module handles real money, so correctness and failure handling are
first-class:

- **Physical handshake** — every application frame is confirmed with ACK/NAK and
  **retransmitted up to 3 times** (per spec) on NAK or timeout, with separate
  ACK and response timeouts.
- **Integrity** — LRC validated on every received frame; invalid frames are
  NAKed to request retransmission. Outgoing fixed-width fields are validated, so
  a malformed frame is never sent to the terminal.
- **No double charge** — on a connection drop, `autoReconnect` restores the
  socket but a **financial command is never blindly re-sent** (a re-send could
  charge the cardholder twice). Read-only/idempotent commands (status, totals,
  `sendLastResult`, enable-printing) are retried; payments/reversals/pre-auths
  reconnect and surface the error so you recover the outcome via
  `sendLastResult()` (the spec's `G` command). This invariant is unit-tested.
- **Defensive parsing** — response parsers never read out of bounds on short or
  malformed payloads.
- **One transaction at a time** — matches the protocol's request/response model.
- **Tested** — 83 C++ unit/flow/safety tests in CI, plus an opt-in real-terminal
  integration test.

## 📊 Feature status

| Area | Status |
|------|:------:|
| Packet framing + LRC (4 modes) | ✅ |
| All request builders (`P X p i c H U C T G E R K s S`) | ✅ |
| Response parsing (`E/V/s/T/C/e/K`, incl. DCC) | ✅ |
| Session orchestration (ACK/NAK, retransmit, timeout, progress/receipt) | ✅ |
| Async client API + events | ✅ |
| Auto-connect, tokenization (`U`) flow, receipt streaming | ✅ |
| Android native transport (Kotlin TCP) | ✅ *(CI-built)* |
| iOS native transport (Swift / Network.framework) | ✅ *(verified on device)* |

## Requirements

- **React Native** 0.76+ (new architecture) — the example uses Expo SDK 56 / RN 0.85
- **react-native-nitro-modules** (peer dependency)
- A Nexi Group ECR17-compatible terminal configured for **LAN integration**

## 📦 Installation

```bash
bun add react-native-ecr17 react-native-nitro-modules
# or: npm install react-native-ecr17 react-native-nitro-modules
cd ios && pod install   # iOS
```

> Nitro module: requires the RN **new architecture** (default on 0.76+).

## 🚀 Quick start

```ts
import { createEcr17Client } from 'react-native-ecr17';

const client = createEcr17Client({
  host: '192.168.1.50',     // terminal IP on the LAN
  port: 1024,               // configured ECR port
  terminalId: '12345678',
  cashRegisterId: '00000001',
  lrcMode: 'std',
  responseTimeoutMs: 60000,
});

await client.connect();

const result = await client.pay({ amountCents: 650 });
if (result.outcome === 'ok') {
  console.log('Approved', result.authCode, 'PAN', result.pan);
} else {
  console.warn('Declined:', result.errorDescription);
}

// Reversal ("annullamento") of the last transaction:
await client.reverse({});

const status = await client.status();   // PosStatusResponse
await client.disconnect();
```

## ⚛️ React hook example

```tsx
import { useEffect, useMemo, useState } from 'react';
import { createEcr17Client, type Ecr17Config, type ProgressEvent } from 'react-native-ecr17';

export function useEcr17(config: Ecr17Config) {
  const client = useMemo(() => createEcr17Client(config), [config]);
  const [progress, setProgress] = useState<string>('');

  useEffect(() => {
    client.setOnProgress((e: ProgressEvent) => setProgress(e.message));
    client.connect();
    return () => client.disconnect();
  }, [client]);

  return {
    progress,
    pay: (amountCents: number) => client.pay({ amountCents }),
    reverse: () => client.reverse({}),
    status: () => client.status(),
  };
}
```

## ⚙️ Configuration

`Ecr17Config`: `host` (required), `port?`, `terminalId` (required), `cashRegisterId`
(required), `lrcMode?`, `keepAlive?`, `autoReconnect?`, `connectionTimeoutMs?`,
`responseTimeoutMs?`, `ackTimeoutMs?`, `retryCount?`, `retryDelayMs?`, `debug?`.

## 📖 API reference

All commands are **async** (`Promise`) and perform a full request/response
exchange. `configure`/`configuration` are synchronous.

| Method | Command | Returns |
|--------|:------:|---------|
| `connect()` / `disconnect()` / `isConnected()` | — | `Promise<void>` / `void` / `bool` |
| `status()` | `s` | `PosStatusResponse` |
| `pay(req)` / `payExtended(req)` | `P` / `X` | `PaymentResult` |
| `reverse(req)` | `S` | `ReversalResult` |
| `preAuth(req)` / `incrementalAuth(req)` / `preAuthClosure(req)` | `p` / `i` / `c` | `PreAuthResult` / `PaymentResult` |
| `verifyCard(req)` | `H` | `CardVerificationResult` |
| `closeSession()` / `totals()` | `C` / `T` | `CloseSessionResult` / `TotalsResult` |
| `sendLastResult()` | `G` | `PaymentResult` |
| `enableEcrPrinting(bool)` / `reprint(bool)` | `E` / `R` | `Promise<void>` |
| `vas(xml)` | `K` | `VasResult` |

Commands require an open connection (`connect()` first) and reject on
timeout / retransmission exhaustion / disconnect.

## 📡 Events

```ts
client.setOnProgress((e) => {/* e.message — display text during a procedure */});
client.setOnReceiptLine((l) => {/* l.text — a receipt line when ECR printing is on */});
client.setOnConnectionStateChange((s) => {/* 'disconnected' | 'connecting' | 'connected' */});
```

## 🔐 Protocol cheat-sheet

App frame: `STX(0x02)` · payload · `ETX(0x03)` · `LRC`. Progress: `SOH(0x01)` ·
20 chars · `EOT(0x04)`. Confirmation: `ACK(0x06)` / `NAK(0x15)` · `ETX` · `LRC`.
LRC = `0x7F` XOR-folded; framing bytes folded in are selectable via `lrcMode`
(`stx` / `std` / `noext` / `stx_noext`).

## 🏗️ Architecture

```
package/cpp/
├── Lcr/            # LRC (4 modes, base 0x7F)
├── PacketCodec/    # framing: STX·ETX·SOH·EOT·ACK·NAK + LRC
├── Ecr17Protocol/  # request builders (all commands), fixed-width + validated
├── Ecr17Response/  # response field parsers -> plain structs
├── Session/        # ACK/NAK + retransmit + timeout orchestration
├── Transport/      # abstract Transport + NativeTransportAdapter + FakeTransport (tests)
└── Ecr17Client/    # HybridEcr17Client (Nitro async API)
package/android/.../HybridEcr17Transport.kt   # Kotlin TCP transport
package/ios/HybridEcr17Transport.swift        # Swift (Network.framework) transport
```

## 🧪 Testing

```bash
cmake -S package/cpp/tests -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build && ctest --test-dir build --output-on-failure
```

83 tests cover LRC, packet (de)framing edge cases, every builder's byte layout,
every response parser, and the documented payment / reversal / re-pay / progress
/ receipt / NAK-retransmit / timeout flows (against an in-memory `FakeTransport`).

## 🧾 Tokenization & receipts

```ts
// Tokenization: attach a contract to a payment/preAuth/verifyCard. The 'U'
// additional-data message is sent automatically (P -> ACK -> U -> ACK -> result).
await client.pay({
  amountCents: 1000,
  tokenization: { service: 'recurring', contractCode: '1666354841608' },
});

// Receipts printed by the ECR: enable printing, set receiptDrainMs in the config,
// and receive lines via the event.
await client.enableEcrPrinting(true);
client.setOnReceiptLine((l) => appendToReceipt(l.text));
```

## 🔌 Testing against a real terminal (opt-in)

An opt-in C++ integration test runs the full core over a real TCP socket. It is
**skipped** unless `ECR17_TERMINAL_HOST` is set:

```bash
cmake -S package/cpp/tests -B build && cmake --build build
ECR17_TERMINAL_HOST=192.168.1.50 ECR17_TERMINAL_PORT=1024 \
ECR17_TERMINAL_ID=00000000 ECR17_LRC_MODE=std \
ctest --test-dir build -R Integration --output-on-failure
```

## 🤖 Vibe-coding batteries included

Building on an undocumented payment protocol is exactly where AI assistants get
things subtly wrong. This repo ships the context to prevent that, so an agent (or
a new contributor) is productive and *safe* from minute one:

- **[`AGENTS.md`](https://github.com/padosoft/react-native-ecr17-protocol/blob/main/AGENTS.md)** /
  **[`CLAUDE.md`](https://github.com/padosoft/react-native-ecr17-protocol/blob/main/CLAUDE.md)** — project guide, the mandatory
  per-phase workflow, CI strategy, and the **money-critical** rules (e.g. never
  blindly retry a payment).
- **`docs/LESSON.md`** — accumulated, verified engineering lessons (Nitro APIs,
  C++↔Kotlin JNI, build traps, payment-safety) — the gotchas already solved.
- **`PROGRESS.md`** — crash-safe resume state across sessions.

The result: less hallucination, fewer footguns, and changes that respect the
payment-safety invariants by default.

## 📄 License

[MIT](https://github.com/padosoft/react-native-ecr17-protocol/blob/main/LICENSE) © [padosoft](https://github.com/padosoft)

> **Disclaimer:** independent integration library. "ECR17", "Nexi" and related
> marks belong to their respective owners and are referenced for interoperability only.
