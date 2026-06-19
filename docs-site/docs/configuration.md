---
title: Configuration
description: Configure terminal connection, identifiers, LRC behavior, timeouts, retries, and events.
---

# Configuration

`createEcr17Client(config)` creates the Nitro HybridObject and immediately applies the configuration.

```ts
const client = createEcr17Client({
  host: "192.168.1.50",
  port: 10000,
  terminalId: "12345678",
  cashRegisterId: "00000001",
  lrcMode: "std",
  keepAlive: true,
  autoReconnect: true,
  connectionTimeoutMs: 10000,
  responseTimeoutMs: 60000,
  ackTimeoutMs: 5000,
  receiptDrainMs: 1500,
  retryCount: 3,
  retryDelayMs: 250,
  debug: false
});
```

## Required fields

- `host`: terminal IP address or hostname reachable from the device.
- `terminalId`: terminal identifier expected by the ECR17 estate.
- `cashRegisterId`: cash-register identifier used in requests.

## Important defaults

- `port`: terminal-specific ECR port, commonly configured by the acquirer or estate.
- `lrcMode`: one of `stx`, `std`, `noext`, or `stx_noext`.
- `autoReconnect`: useful before commands, but never a license to replay financial requests.
- `receiptDrainMs`: enables receipt-line draining after a result when ECR printing is active.

::: callout warning "LRC mode is a compatibility switch"
Different terminal firmware can fold different framing bytes into the LRC. If status works but other commands NAK or timeout, verify `lrcMode` against the terminal configuration and official Nexi documentation.
:::
