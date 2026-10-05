---
title: ADR
description: Architecture decision records for the ECR17 module.
---

# ADR

This page captures the key architectural decisions behind the module.

::: collapsible "ADR 001: Shared C++ protocol core" open
## Status

Accepted.

## Decision

Implement LRC, frame codec, request builders, response parsers, and session orchestration in C++20.

## Consequences

The same protocol behavior is used on iOS and Android. Native code remains responsible for TCP sockets and platform lifecycle.
:::

::: collapsible "ADR 002: Nitro HybridObject API"
## Status

Accepted.

## Decision

Expose the client as a Nitro HybridObject with Promise-returning methods and callback setters for events.

## Consequences

The public API is ergonomic for React Native while still allowing native C++ execution for protocol logic.
:::

::: collapsible "ADR 003: Do not replay financial commands after disconnect"
## Status

Accepted.

## Decision

Reconnect transport on failure, but never automatically re-send payment, reversal, or pre-authorization commands after the exchange may have reached the terminal.

## Consequences

Applications recover with `sendLastResult()` and avoid duplicate charges.
:::

::: collapsible "ADR 004: Configurable LRC mode"
## Status

Accepted.

## Decision

Expose `lrcMode` because terminal firmware can differ in which framing bytes are folded into the LRC.

## Consequences

Integrators can adapt without patching protocol code, but they must validate the selected mode during terminal setup.
:::
