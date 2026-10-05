---
title: Design
description: Design of the session, transport adapter, and command pipeline.
---

# Design

The design separates byte transport from protocol semantics.

```mermaid
classDiagram
  class Ecr17Client {
    +configure(config)
    +connect()
    +pay(request)
    +sendLastResult()
  }
  class Ecr17Session {
    +runTransaction(command)
    +runAckOnly(command)
  }
  class PacketCodec {
    +encode(payload)
    +decode(frame)
  }
  class Transport {
    +connect(host, port)
    +send(bytes)
    +setOnData(callback)
  }
  Ecr17Client --> Ecr17Session
  Ecr17Session --> PacketCodec
  Ecr17Session --> Transport
```

## Design principles

- Validate local request fields before transport send.
- Decode defensively and never read past malformed payloads.
- Keep ACK/NAK retransmission inside the session boundary.
- Make business-level recovery explicit through `sendLastResult()`.
- Emit terminal progress without making UI code parse protocol bytes.

::: collapsible "Design tradeoff: one transaction at a time" open
ECR17 is a synchronous request-response protocol. The client serializes commands because parallel transactions on one terminal would create ambiguous terminal state and unsafe recovery behavior.
:::
