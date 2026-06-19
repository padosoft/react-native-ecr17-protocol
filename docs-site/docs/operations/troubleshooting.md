---
title: Troubleshooting
description: Common ECR17 integration problems and fixes.
---

# Troubleshooting

## `createHybridObject` fails

Rebuild the native app. Nitro-generated native classes are not available in JavaScript-only or stale native builds.

## Android works once then fails

Some terminals close TCP sockets between transactions. The transport must detect half-open sockets before the next send and reconnect before sending a new command.

## Commands NAK immediately

Verify:

- `lrcMode`
- terminal ID
- cash-register ID
- command support on the terminal estate
- fixed-width field sizes

## Progress appears but no result

The terminal accepted the request and is waiting for cardholder action, host response, or operator action. Review `responseTimeoutMs` before assuming a protocol error.

## Receipt lines are missing

Confirm ECR printing is enabled and `receiptDrainMs` is non-zero. Receipt messages arrive after the final result on some terminals.

::: callout warning "Limit"
The package cannot infer acquirer configuration, merchant contract capabilities, or terminal firmware quirks. Keep a terminal-specific compatibility note for each deployed estate.
:::
