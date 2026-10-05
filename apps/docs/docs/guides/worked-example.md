---
title: Worked Example
description: Pair a terminal, collect a payment, recover after a connection drop, and store the result.
---

# Worked Example

This example shows a complete operator checkout flow.

## Scenario

- Terminal IP: `192.168.1.50`.
- ECR port: `10000`.
- Sale amount: EUR 6.50.
- Application must avoid double charges if the LAN socket drops.

::: steps
1. Configure the client.

   ```ts
   const client = createEcr17Client({
     host: "192.168.1.50",
     port: 10000,
     terminalId: "12345678",
     cashRegisterId: "00000001",
     lrcMode: "std",
     autoReconnect: true,
     responseTimeoutMs: 60000,
     ackTimeoutMs: 5000
   });
   ```

2. Register events.

   ```ts
   client.setOnConnectionStateChange(setConnectionState);
   client.setOnProgress((event) => setPaymentMessage(event.message));
   client.setOnReceiptLine((line) => receiptBuffer.push(line.text));
   ```

3. Confirm the terminal is reachable.

   ```ts
   await client.connect();
   const status = await client.status();
   ```

4. Collect payment.

   ```ts
   const result = await collectWithRecovery(650);
   ```

5. Persist the structured result.

   ```ts
   savePaymentAttempt({
     amountCents: 650,
     outcome: result.outcome,
     resultCode: result.resultCode,
     stan: result.stan,
     authCode: result.authCode,
     onlineId: result.onlineId
   });
   ```
:::

## Recovery helper

```ts
async function collectWithRecovery(amountCents: number) {
  try {
    return await client.pay({ amountCents });
  } catch (error) {
    await client.connect();
    return client.sendLastResult();
  }
}
```

::: callout danger "The helper does not retry `pay`"
It recovers the latest terminal outcome with command `G`. That is the safety boundary that prevents a duplicate payment command.
:::
