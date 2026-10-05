---
title: Motivazione
description: Why this React Native ECR17 module exists.
---

# Motivazione

Italian POS integrations often require protocol details that are hard to discover, vary by terminal firmware, or sit behind vendor processes. This module packages the reusable parts into a native React Native library.

## Goals

- Let mobile cash-register apps talk to ECR17 terminals directly over LAN.
- Keep payment framing, LRC, parsing, and retry behavior in shared C++.
- Expose a small Promise API to application code.
- Preserve money-safety invariants in the core instead of scattering them across UI code.

## Non-goals

- It is not a payment gateway.
- It does not replace acquirer certification.
- It does not guarantee every terminal firmware variant without configuration.
- It does not make financial commands idempotent.

::: callout info "Protocol references"
Always validate field positions and terminal configuration against official Nexi documentation for the merchant estate. This site documents the repository behavior and integration patterns.
:::
