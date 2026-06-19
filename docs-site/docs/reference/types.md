---
title: Types
description: Public TypeScript types for requests, responses, and events.
---

# Types

This page summarizes the most important public types. The source of truth is `package/src/types/client.ts` and `package/src/specs/client.nitro.ts`.

## Unions

```ts
type LrcMode = "stx" | "std" | "noext" | "stx_noext";
type ConnectionState = "disconnected" | "connecting" | "connected";
type TransactionOutcome = "ok" | "ko" | "cardNotPresent" | "unknownTag" | "unknown";
type PaymentCardType = "auto" | "debit" | "credit" | "other";
type TokenizationService = "recurring" | "unscheduledOrOneClick";
```

## Requests

```ts
interface PaymentRequest {
  amountCents: number;
  cashRegisterId?: string;
  paymentType?: PaymentCardType;
  cardAlreadyPresent?: boolean;
  receiptText?: string;
  tokenization?: TokenizationRequest;
}
```

## Events

```ts
interface ProgressEvent {
  message: string;
}

interface ReceiptLine {
  text: string;
}
```

::: callout info "Generated native signatures"
Nitro converts Promise-returning TypeScript methods into native async methods. Keep spec types Nitro-compatible when changing API shape.
:::
