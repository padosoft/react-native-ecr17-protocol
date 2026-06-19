---
title: Protocol Frames
description: Byte-level model for application, progress, and acknowledgement frames.
---

# Protocol Frames

The C++ `PacketCodec` treats each decoded buffer as exactly one frame. Stream splitting belongs to the transport/session boundary.

```mermaid
flowchart TD
  Bytes[Incoming bytes] --> Type{Leading byte}
  Type -->|STX| App[Application frame]
  Type -->|SOH| Progress[Progress frame]
  Type -->|ACK or NAK| Ack[Handshake confirmation]
  App --> LRC[Validate LRC]
  LRC --> Payload[Parse command payload]
  Progress --> Display[Emit progress event]
  Ack --> Session[Advance retransmit state]
```

## Application frame

```text
STX payload ETX LRC
```

## Progress frame

```text
SOH 20-char-message EOT
```

## ACK and NAK

The terminal can acknowledge a frame or request retransmission. The session retransmits according to the configured retry count and the command safety policy.

::: callout danger "Retransmit is not business retry"
ACK/NAK retransmission happens inside one physical exchange. It is different from replaying a payment after a disconnected transaction.
:::
