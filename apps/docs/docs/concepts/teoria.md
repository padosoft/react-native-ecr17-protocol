---
title: Teoria
description: ECR17 framing, LRC, ACK/NAK, progress messages, and timing.
---

# Teoria

ECR17 is a request-response protocol between an Electronic Cash Register and an EFT-POS terminal. The cash register sends a command frame, the terminal acknowledges transport-level receipt, performs the requested operation, and returns an application result frame.

## Frame families

::: grids
::: grid
::: card "Application frame"
`STX`, payload, `ETX`, then `LRC`.
:::
:::

::: grid
::: card "Progress frame"
`SOH`, 20-character display text, then `EOT`.
:::
:::

::: grid
::: card "Confirmation"
`ACK` or `NAK` response at the protocol handshake layer.
:::
:::
:::

## LRC

The implementation starts from base `0x7F` and XOR-folds the configured bytes.

For a byte sequence \(b_1, b_2, \ldots, b_n\):

$$
LRC = 0x7F \oplus b_1 \oplus b_2 \oplus \cdots \oplus b_n
$$

`lrcMode` controls whether `STX` and `ETX` participate in that fold:

- `stx`: `STX + payload + ETX`
- `std`: `payload`
- `noext`: `payload + ETX`
- `stx_noext`: `STX + payload`

## Timing

The session uses distinct timeouts for connection, ACK, and final response. Separating them helps identify whether the failure is the LAN socket, terminal handshake, or host-side transaction wait.

::: callout warning "Gotcha"
Progress frames do not carry LRC. Treating `SOH ... EOT` as an application frame will corrupt decoding.
:::
