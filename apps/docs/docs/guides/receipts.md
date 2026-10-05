---
title: Receipts
description: Enable ECR printing and collect streamed receipt lines.
---

# Receipts

ECR17 terminals can send receipt lines as `S` messages when ECR printing is enabled. The library exposes them through `setOnReceiptLine`.

```ts
client.setOnReceiptLine((line) => {
  appendReceiptLine(line.text);
});

await client.enableEcrPrinting(true);
```

## Drain window

Set `receiptDrainMs` when configuring the client. After a transaction result, the session keeps forwarding receipt lines until the configured silence window expires.

::: callout tip "Choose a small drain"
Start with `receiptDrainMs: 1500`. Increase only if the terminal emits receipt lines slowly on your LAN.
:::

## Display model

::: tabs
== tab "Operator UI"
Append receipt lines to a log area so the operator can confirm what the terminal printed.

== tab "Back office"
Persist raw lines with the payment record only when your compliance and privacy policy allows it.
:::

## Limits

Receipt lines are terminal-generated display text. Do not parse them as the source of truth for authorization, amount, or reversal state. Use the structured response fields for business decisions.
