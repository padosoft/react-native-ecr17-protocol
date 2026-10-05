---
title: API
description: Public Ecr17Client API reference.
---

# API

Import the factory and create one client per configured terminal connection.

```ts
import { createEcr17Client } from "react-native-ecr17";

const client = createEcr17Client(config);
```

## Configuration

- `configure(config): void`
- `configuration(): Ecr17Config`

## Connection

- `connect(): Promise<void>`
- `disconnect(): void`
- `isConnected(): boolean`

## Commands

| Method | Command | Return |
| --- | --- | --- |
| `status()` | `s` | `PosStatusResponse` |
| `pay(request)` | `P` | `PaymentResult` |
| `payExtended(request)` | `X` | `PaymentResult` |
| `reverse(request)` | `S` | `ReversalResult` |
| `preAuth(request)` | `p` | `PreAuthResult` |
| `incrementalAuth(request)` | `i` | `PreAuthResult` |
| `preAuthClosure(request)` | `c` | `PaymentResult` |
| `verifyCard(request)` | `H` | `CardVerificationResult` |
| `closeSession()` | `C` | `CloseSessionResult` |
| `totals()` | `T` | `TotalsResult` |
| `sendLastResult()` | `G` | `PaymentResult` |
| `enableEcrPrinting(enabled)` | `E` | `Promise<void>` |
| `reprint(toEcr)` | `R` | `Promise<void>` |
| `vas(xmlRequest)` | `K` | `VasResult` |

## Events

```ts
client.setOnProgress((event) => {});
client.setOnReceiptLine((line) => {});
client.setOnConnectionStateChange((state) => {});
```
