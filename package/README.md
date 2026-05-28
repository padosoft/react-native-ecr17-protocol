<div align="center">

# 💳 react-native-ecr17

**A React Native / Nitro module for the Italian ECR17 payment protocol — talk to Nexi Group POS terminals straight from your cash-register app.**

[![npm version](https://img.shields.io/npm/v/react-native-ecr17.svg?style=flat-square)](https://www.npmjs.com/package/react-native-ecr17)
[![npm downloads](https://img.shields.io/npm/dm/react-native-ecr17.svg?style=flat-square)](https://www.npmjs.com/package/react-native-ecr17)
[![License: MIT](https://img.shields.io/npm/l/react-native-ecr17.svg?style=flat-square)](https://github.com/padosoft/react-native-ecr17-protocol/blob/main/LICENSE)
[![C++ tests](https://github.com/padosoft/react-native-ecr17-protocol/actions/workflows/cpp-tests.yml/badge.svg)](https://github.com/padosoft/react-native-ecr17-protocol/actions/workflows/cpp-tests.yml)
[![Built with Nitro](https://img.shields.io/badge/built%20with-Nitro-8B5CF6?style=flat-square)](https://nitro.margelo.com)
[![Platforms](https://img.shields.io/badge/platforms-iOS%20%7C%20Android-555?style=flat-square)](#requirements)

</div>

---

> [!IMPORTANT]
> **Early foundation release.** The protocol core (packet framing, LRC, message
> builders) is implemented and unit-tested, but the JavaScript surface is still
> minimal: you can `configure()` a client and read the configuration back.
> Sending payments end-to-end requires the transport layer, which is on the
> [roadmap](#-roadmap). See [Feature status](#-feature-status) for the exact
> state of every piece — nothing here is oversold.

## 📚 Table of contents

- [What is ECR17?](#-what-is-ecr17)
- [Why this library](#-why-this-library)
- [Feature status](#-feature-status)
- [Requirements](#requirements)
- [Installation](#-installation)
- [Quick start](#-quick-start)
- [Configuration](#%EF%B8%8F-configuration)
- [API reference](#-api-reference)
- [Protocol cheat-sheet](#-protocol-cheat-sheet)
- [Architecture](#%EF%B8%8F-architecture)
- [Running the tests](#-running-the-tests)
- [Roadmap](#-roadmap)
- [Contributing](#-contributing)
- [License](#-license)

## 🧭 What is ECR17?

**ECR17** is the Italian standard protocol — supported by **Nexi Group**
terminals — used to integrate an *Electronic Cash Register* (ECR) with an
*EFT-POS* payment terminal over a local LAN connection. The cash register sends
a request (a payment, a reversal, a status check…), the terminal talks to the
acquiring host, and replies synchronously.

This library speaks that protocol from a React Native app, with the
performance-sensitive parts written in C++ and bridged via
[Nitro Modules](https://nitro.margelo.com).

The protocol reference used to build this library is vendored in
[`docs/`](https://github.com/padosoft/react-native-ecr17-protocol/tree/main/docs).

## ✨ Why this library

- ⚡️ **C++ core, Nitro-bridged** — framing/LRC run natively on both iOS &
  Android, no JS-thread overhead.
- 🧱 **Spec-faithful** — message layouts validated byte-for-byte against the
  Nexi ECR17 documentation.
- 🛡️ **Fails loudly, not silently** — oversized fixed-width fields and negative
  amounts throw instead of emitting a malformed frame to a payment terminal.
- ✅ **Tested** — 34 unit + flow tests (LRC, packet codec, builders, and
  documented payment/reversal flows) run in CI on every PR.
- 🔌 **Configurable LRC modes** — the standard leaves the LRC scope to the
  integrator; all four variants are first-class.

## 📊 Feature status

| Area | Status | Notes |
|------|:------:|-------|
| Packet framing (`STX`/`ETX`/`SOH`/`EOT`, ACK/NAK) | ✅ | `PacketCodec` encode/decode |
| LRC computation (4 modes, base `0x7F`) | ✅ | `Lrc` |
| Payment request builder (`P`) | 🟡 | C++ (`Ecr17Protocol`), not yet exposed to JS |
| Reversal / *annullamento* builder (`S`) | 🟡 | C++, not yet exposed to JS |
| Terminal status builder (`s`) | 🟡 | C++, not yet exposed to JS |
| `Ecr17Client.configure()` / `configuration()` | ✅ | usable from JS today |
| `Ecr17Client.status()` | 🚧 | throws "not implemented" until transport lands |
| Transport (TCP/LAN, ACK/NAK retry) | ❌ | [roadmap](#-roadmap) |
| Response field parsing (`E`/`V`/`s`/…) | ❌ | [roadmap](#-roadmap) |

Legend: ✅ done · 🟡 implemented natively, not yet on the JS API · 🚧 stub · ❌ not started.

## Requirements

- **React Native** 0.76.0+ (new architecture)
- **react-native-nitro-modules** (peer dependency)
- **Node** 18+
- A Nexi Group ECR17-compatible terminal configured for **LAN integration**

## 📦 Installation

```bash
# with bun
bun add react-native-ecr17 react-native-nitro-modules

# or npm / yarn
npm install react-native-ecr17 react-native-nitro-modules
```

Then install native deps:

```bash
cd ios && pod install   # iOS
# Android autolinks — just rebuild
```

> This is a Nitro module: it requires the React Native **new architecture**
> (`newArchEnabled=true`), which is the default on RN 0.76+.

## 🚀 Quick start

A junior-proof, copy-paste example of what works **today** — create a client,
configure it, and read the configuration back:

```ts
import { createEcr17Client } from 'react-native-ecr17';

// 1. Create + configure a client pointed at your terminal on the LAN.
const client = createEcr17Client({
  host: '192.168.1.50',   // terminal IP
  port: 1024,             // configured ECR port (Linux EFT-POS: > 1024)
  terminalId: '12345678', // 8-digit terminal id (or '00000000')
  cashRegisterId: '00000001',
  lrcMode: 'std',         // see "Protocol cheat-sheet"
});

// 2. Read back the effective configuration (handy for debugging).
const cfg = client.configuration();
console.log('Configured for terminal', cfg.terminalId, 'at', cfg.host);
```

> [!NOTE]
> `client.status()` currently **throws** `"... not implemented yet ..."` on
> purpose — querying the terminal needs the transport layer, which is on the
> roadmap. This is intentional so you never get a fake/garbage response.

Here's the shape the payment API will take once the transport lands (🚧 **not
functional yet** — shown so you can plan your integration):

```ts
// ⚠️ ROADMAP — does not work yet.
// const result = await client.pay({ amountCents: 650 });
// if (result.outcome === 'OK') { /* print receipt, etc. */ }
```

## ⚙️ Configuration

`createEcr17Client(config)` / `client.configure(config)` accept an `Ecr17Config`:

| Field | Type | Required | Description |
|-------|------|:--------:|-------------|
| `host` | `string` | ✅ | Terminal IP address on the LAN |
| `port` | `number` | | TCP port (Linux EFT-POS requires > 1024) |
| `terminalId` | `string` | ✅ | 8-digit terminal id (`00000000`–`99999999`) |
| `cashRegisterId` | `string` | ✅ | Cash register / ECR identifier |
| `lrcMode` | `LrcMode` | | LRC scope — see below (default per terminal) |
| `keepAlive` | `boolean` | | Keep the socket open between requests |
| `autoReconnect` | `boolean` | | Reconnect automatically on drop |
| `connectionTimeoutMs` | `number` | | Connect timeout |
| `responseTimeoutMs` | `number` | | Per-request response timeout |
| `ackTimeoutMs` | `number` | | Physical ACK/NAK timeout |
| `retryCount` | `number` | | Retransmission attempts (spec: up to 3) |
| `retryDelayMs` | `number` | | Delay between retransmissions |
| `debug` | `boolean` | | Verbose protocol logging |

## 📖 API reference

### `createEcr17Client(config: Ecr17Config): Ecr17Client`

Creates a Nitro `Ecr17Client` HybridObject and configures it in one call.

### `client.configure(config: Ecr17Config): void`

(Re)applies configuration to an existing client.

### `client.configuration(): Ecr17Config`

Returns the currently applied configuration.

### `client.status(): PosStatusResponse` 🚧

Will return the terminal status. **Currently throws** until transport +
response parsing are implemented.

```ts
interface PosStatusResponse {
  terminalId: string;
  terminalDateTime: Date;
  status: PosTerminalStatus;   // -1 (Unknown) … 6
  softwareRelease: string;
}
```

`PosTerminalStatus` maps to human-readable strings via `PosTerminalStatusMessage`
(e.g. `2` → `"Terminal operative (after a DLL)"`, `-1` → `"Unknown"`).

## 🔐 Protocol cheat-sheet

**Application packet:** `STX(0x02)` · payload · `ETX(0x03)` · `LRC`
**Progress update:** `SOH(0x01)` · 20-char message · `EOT(0x04)` (no LRC)
**Confirmation:** `ACK(0x06)` / `NAK(0x15)` · `ETX` · `LRC`

**LRC** = `0x7F` XOR-folded over the message bytes. Which framing bytes are
included is configurable via `lrcMode`:

| `lrcMode` | Bytes folded into the LRC |
|-----------|---------------------------|
| `'stx'` | `STX` + payload + `ETX` |
| `'std'` | payload only |
| `'noext'` | payload + `ETX` |
| `'stx_noext'` | `STX` + payload |

**Command codes** (from the Nexi ECR17 spec):

| Code | Command | Builder |
|:----:|---------|:-------:|
| `s` | Terminal status | ✅ (native) |
| `P` | Payment | ✅ (native) |
| `S` | Reversal (*annullamento*) | ✅ (native) |
| `X` | Extended payment | ❌ |
| `p` `i` `c` | Pre-auth / incremental / closure | ❌ |
| `H` | Card verification | ❌ |
| `U` | Additional GT data / tokenization | ❌ |
| `C` `T` | Close session / totals | ❌ |
| `G` `E` `R` | Last result / ECR print / reprint | ❌ |
| `K` | VAS / APM (BancomatPay, Alipay, …) | ❌ |

## 🏗️ Architecture

```
package/cpp/
├── Lcr/            # Lrc — LRC computation (4 modes, base 0x7F)
├── PacketCodec/    # framing: encode/decode STX·ETX·SOH·EOT·ACK·NAK + LRC check
├── Ecr17Protocol/  # message builders (P / S / s) — fixed-width, validated
├── Transport/      # abstract send/receive interface (impl on the roadmap)
└── Ecr17Client/    # HybridEcr17Client — the Nitro-exposed entry point
```

The JS/TS spec lives in `package/src/specs/client.nitro.ts`; the C++/Swift/Kotlin
glue is generated by Nitrogen into `package/nitrogen/generated/`.

## 🧪 Running the tests

The C++ core is covered by a standalone GoogleTest suite that builds without
Nitro (it stubs the generated `LrcMode` enum):

```bash
cmake -S package/cpp/tests -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The same suite runs in CI on every pull request. It covers LRC (all modes),
packet (de)framing and its edge cases, the message builders' exact byte layout
and validation, and the documented **payment / reversal / re-payment** flows.

## 🛣️ Roadmap

- [ ] `Transport` TCP/LAN implementation (iOS + Android)
- [ ] Send/receive orchestration with ACK/NAK + retransmit-up-to-3 (per spec)
- [ ] Response field parsing (`E`/`V` results, `s` status, `S` receipt stream, `U` GT data)
- [ ] Expose `pay()`, `reverse()`, `status()` on the JS API
- [ ] Remaining command builders (`X`, `p`, `i`, `c`, `H`, `U`, `C`, `T`, `G`, `E`, `R`, `K`)
- [ ] Tokenization helpers

## 🤝 Contributing

Issues and PRs welcome. Please keep the C++ core spec-faithful and add tests for
any new builder or codec path — CI must stay green.

## 📄 License

[MIT](https://github.com/padosoft/react-native-ecr17-protocol/blob/main/LICENSE) © [padosoft](https://github.com/padosoft)

> **Disclaimer:** this is an independent integration library. "ECR17", "Nexi"
> and related marks belong to their respective owners and are referenced for
> interoperability only.
