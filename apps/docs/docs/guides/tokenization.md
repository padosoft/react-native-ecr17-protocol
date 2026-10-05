---
title: Tokenization
description: Attach ECR17 additional data for recurring and unscheduled card flows.
---

# Tokenization

Tokenization is requested with command `U` as additional data attached to a payment, pre-authorization, or card verification flow.

```ts
await client.pay({
  amountCents: 1000,
  tokenization: {
    service: "recurring",
    contractCode: "1666354841608"
  }
});
```

## Supported service values

- `recurring`: merchant-initiated recurring agreement.
- `unscheduledOrOneClick`: unscheduled credential-on-file or one-click use.

::: callout warning "Acquirer rules still apply"
The protocol can carry tokenization data, but the merchant contract, terminal estate, and acquirer configuration decide whether tokenization is accepted.
:::

## Flow

```mermaid
sequenceDiagram
  participant App
  participant Core
  participant POS

  App->>Core: pay({ tokenization })
  Core->>POS: P payment request
  POS-->>Core: ACK
  Core->>POS: U additional data
  POS-->>Core: ACK
  POS-->>Core: payment result
  Core-->>App: PaymentResult
```
