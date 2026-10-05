---
title: Runbook
description: Diagnose terminal connectivity, protocol, and payment-recovery incidents.
---

# Runbook

Use this runbook when the app cannot connect, commands timeout, the terminal NAKs frames, or a transaction result is uncertain.

## Connectivity

::: steps
1. Confirm the mobile device and terminal are on the same LAN.
2. Confirm the terminal IP and ECR port.
3. Run `status()` before a financial command.
4. Check whether the terminal closes the socket between transactions.
5. Reconnect before the next command if the connection state is stale.
:::

## Protocol failures

::: tabs
== tab "NAK"
Check `lrcMode`, fixed-width field lengths, terminal firmware expectations, and command availability.

== tab "ACK timeout"
Check LAN reachability, terminal busy state, and ECR port configuration.

== tab "Response timeout"
Check host-side acquirer delay, cardholder action, and whether the configured `responseTimeoutMs` is too short.
:::

## Uncertain payment

::: callout danger "Recovery path"
Do not run the original payment again. Reconnect and call `sendLastResult()`. If the result remains unavailable, reconcile with terminal totals, acquirer portal, or operator procedure before charging again.
:::
