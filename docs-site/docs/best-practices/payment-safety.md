---
title: Payment Safety
description: Operational safety rules for financial commands.
---

# Payment Safety

The core safety rule is simple: transport retry is allowed only while the protocol exchange itself is still controlled; business retry of a financial command is not automatic.

::: callout danger "Never blindly retry a financial command"
If a payment-like command may have reached the terminal, replaying it can duplicate money movement. Recover the result with `sendLastResult()`.
:::

## Financial commands

Treat these as non-idempotent:

- `pay`
- `payExtended`
- `reverse`
- `preAuth`
- `incrementalAuth`
- `preAuthClosure`

## Safer commands

Read-only or control commands such as `status`, `totals`, `sendLastResult`, and ECR-printing toggles are safer to run after reconnect, subject to your application flow.

## Application policy

::: steps
1. Persist a local payment attempt before calling `pay`.
2. Store the amount and sale identifier outside the terminal result.
3. On success, attach `stan`, `onlineId`, `authCode`, and `resultCode`.
4. On disconnect, call `sendLastResult()`.
5. Reconcile unresolved attempts before allowing another charge for the same sale.
:::
