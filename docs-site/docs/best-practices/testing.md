---
title: Testing
description: Test strategy for protocol, TypeScript, native transport, and real-terminal behavior.
---

# Testing

Testing has three levels: deterministic protocol tests, generated TypeScript/Nitro checks, and real-device validation.

## Protocol tests

The C++ test suite covers:

- LRC modes.
- Packet framing and progress decoding.
- Request builders.
- Response parsers.
- Session orchestration with ACK, NAK, progress, receipt, timeout, and retry policy.

```bash
cmake -S package/cpp/tests -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## TypeScript checks

```bash
cd package
bunx tsc --noEmit -p tsconfig.ci.json
```

## Real terminal checks

Set terminal environment variables for opt-in integration tests, or use the example debug console to exercise all commands.

::: callout warning "Unit tests are not enough"
Native socket behavior, Android class-loader behavior, and terminal half-close timing require native app execution to validate.
:::
