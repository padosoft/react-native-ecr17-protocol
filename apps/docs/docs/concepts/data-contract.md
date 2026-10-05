---
title: Data Contract
description: Request, result, event, and configuration contracts.
---

# Data Contract

The JavaScript API is Promise-based and uses TypeScript request/result types exported by the package.

## Configuration contract

```ts
interface Ecr17Config {
  host: string;
  port?: number;
  terminalId: string;
  cashRegisterId: string;
  lrcMode?: "stx" | "std" | "noext" | "stx_noext";
  keepAlive?: boolean;
  autoReconnect?: boolean;
  connectionTimeoutMs?: number;
  responseTimeoutMs?: number;
  ackTimeoutMs?: number;
  receiptDrainMs?: number;
  retryCount?: number;
  retryDelayMs?: number;
  debug?: boolean;
}
```

## Result contract

Payment-like results normalize terminal codes into `outcome` while preserving raw fields such as `resultCode`, `stan`, `onlineId`, `authCode`, and `errorDescription`.

::: tabs
== tab "PaymentResult"
```ts
interface PaymentResult {
  outcome: "ok" | "ko" | "cardNotPresent" | "unknownTag" | "unknown";
  resultCode: string;
  pan?: string;
  authCode?: string;
  stan?: string;
  onlineId?: string;
  errorDescription?: string;
}
```

== tab "Events"
```ts
client.setOnProgress((event) => console.log(event.message));
client.setOnReceiptLine((line) => console.log(line.text));
client.setOnConnectionStateChange((state) => console.log(state));
```
:::

## Contract invariant

The core validates fixed-width request fields before sending. Invalid local input should fail before bytes reach the terminal.
