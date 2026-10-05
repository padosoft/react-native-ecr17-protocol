# @padosoft/ecr17

The **ECR17** payment-terminal protocol (Nexi Group POS terminals over LAN) as a
React-free C++20 library, with a **Node.js API** on top. It is the native core of
[`@padosoft/react-native-ecr17`](../react-native-ecr17), and it is usable on its own from
Node.js and from native apps.

| Part | Folder | What |
|---|---|---|
| Protocol core | `cpp/` | LRC, framing, command builders, response parsers, the ACK/NAK session, and `Ecr17Client` (auto-connect, money-safe retry) |
| POSIX transport | `posix/` | `PosixTransport` (macOS, Linux): TCP with the write-free pre-send liveness probe |
| Windows transport | `windows/` | `WinsockTransport`: the same, on Winsock |
| Node.js API | `src/`, `node/` | `Ecr17Client` in TypeScript over a Node-API addon (`node/addon.cpp`) |

The core builds anywhere a C++20 compiler does. On iOS and Android, the React
Native binding supplies the TCP transport, in Swift and Kotlin. On macOS, Linux and
Windows, this package has its own.

> ⚠️ **Not published on npm yet**, like the `@padosoft` build tooling it uses
> (`@padosoft/native-modules`, and `@padosoft/expo` for the SwiftPM opt-in). Those live on the private
> GitHub Packages registry for now (see `bunfig.toml` in this repo).

---

## Node.js

```ts
import { createEcr17Client } from "@padosoft/ecr17";

const client = createEcr17Client({
  host: "192.168.1.50",
  port: 10000,
  terminalId: "12345678",
  cashRegisterId: "00000001",
  autoReconnect: true,
});
client.setOnProgress(({ message }) => console.log(message)); // "ATTENDERE PREGO" …

const result = await client.pay({ amountCents: 650 });
if (result.outcome === "ok") {
  /* approved: result.authCode, result.pan, … */
}
client.close();
```

The API is the one of [`@padosoft/react-native-ecr17`](../react-native-ecr17): the same
`createEcr17Client`, methods, requests, results and events, so code moves between React
Native and Node.js unchanged. Two additions: `close()` (also `using client = …` through
`Symbol.dispose`), and `new Ecr17Client(config)`.

- It runs on the same C++ client, through Node-API: same auto-connect, pre-send probe and
  money-safe retry. Commands run in order on a native worker thread of the client, never on
  the event loop; results and events come back on the event loop, in order (a command's
  events before its result). An idle client doesn't keep the process alive.
- Invalid input rejects with a `TypeError` before anything is sent; command failures reject
  with an `Error` whose `code` is `ECR17_COMMAND_FAILED`.
- `close()` rejects queued commands. A command already sent fails like a dropped
  connection: a financial one is **never** re-sent, so recover its outcome with
  `sendLastResult()`.

The npm package ships the addon for `linux-x64`, `linux-arm64` (glibc 2.35+),
`darwin-arm64` and `win32-x64` (`prebuilds/`, Node-API 8: every Node.js from 20 on). On any
other platform, build it from source in the package folder (CMake and a C++20 compiler):

```bash
npx cmake-js compile --directory node --out build
```

`ECR17_NODE_ADDON=/path/to/ecr17.node` overrides the lookup.

## C++ API

```cpp
#include <ecr17/Ecr17Client.hpp>
#include <ecr17/PosixTransport.hpp>    // macOS, Linux (WinsockTransport.hpp on Windows)

using namespace padosoft::ecr17;

ClientConfig config;
config.host = "192.168.1.50";
config.port = 10000;
config.terminalId = "12345678";
config.cashRegisterId = "00000001";
config.autoReconnect = true;

Ecr17Client client(std::make_shared<PosixTransport>(), config);
client.setOnProgress([](const std::string& message) { /* "ATTENDERE PREGO" … */ });

PaymentRequest request;
request.amountCents = 650;
PaymentResponse result = client.pay(request);   // blocks: call it off the UI thread
if (result.outcome == Outcome::Ok) { /* approved: result.authCode, result.pan, … */ }
```

- Every command auto-connects. Before it sends anything, it probes the socket:
  ECR17/Nexi terminals close TCP between transactions, and a payment must never
  go out on a dead socket.
- Commands are synchronous and serialized. Errors throw `std::runtime_error`
  (connection, timeout, NAK exhaustion, drop) or `std::invalid_argument` (a
  field that does not fit).
- Responses are the parsed protocol fields: `PaymentResponse`,
  `PreAuthResponse`, `StatusResponse`, `TotalsResponse`, `CloseResponse`,
  `VasResponse` (`ecr17/Ecr17Response.hpp`).

### ⚠️ Money safety

With `autoReconnect`, a command interrupted by a dropped connection reconnects
the socket. It is **re-sent only if it is read-only**: `status`, `totals`,
`sendLastResult` and `enableEcrPrinting`. A financial command is **never**
re-sent, because the terminal may already have charged the card.

Recover the outcome of an interrupted payment with `sendLastResult()`, command
`G`. The rule is `shouldRetryAfterReconnect` in
`cpp/include/ecr17/RetryPolicy.hpp`. It is locked by `test_retry_policy.cpp`
and, end to end, by `Client.AFinancialCommandIsNeverResentAfterADrop`.

### Your own transport

`Transport` (`ecr17/Transport.hpp`) is a byte stream with five operations.
Any implementation has to keep the contract written in its header:

- `connect` blocks until connected or throws;
- `isConnected` may probe the socket, but never writes to it or consumes a byte;
- the disconnect callback fires once per unexpected drop, never for `disconnect()`.

## Using it

### CMake (Windows, Linux, macOS)

```cmake
add_subdirectory(path/to/ecr17 ecr17)
target_link_libraries(MyApp PRIVATE ecr17::ecr17)
```

The target also contains the host's transport: `PosixTransport` on macOS and Linux,
`WinsockTransport` on Windows (and links `ws2_32`).
`cmake --install` exports `ecr17::ecr17` for `find_package(ecr17)`.

### Swift Package Manager (iOS, macOS, visionOS)

The repository root has a `Package.swift` with one product, `Ecr17` (the C++ core).
Each `@padosoft/ecr17` version is tagged `X.Y.Z` (from 2.0.0):

```swift
.package(url: "https://github.com/padosoft/react-native-ecr17-protocol.git", from: "2.0.0"),
// target dependency:
.product(name: "Ecr17", package: "react-native-ecr17-protocol"),
```

The headers are C++, so C++ / Objective-C++ code includes them
(`#include <ecr17/Ecr17Client.hpp>`); a Swift API is on the roadmap.

### React Native

Add [`@padosoft/react-native-ecr17`](../react-native-ecr17). On iOS its pod,
`ReactNativeEcr17`, links this package through `@padosoft/native-modules`'
`native_dependency`: as the `Ecr17` pod by default, or as the `Ecr17` Swift package when
the app switches SwiftPM on (`withSwiftPackageManager` from `@padosoft/expo`).

This package is not autolinked (`react-native.config.js`), so the app declares the pod.
In an Expo app, add the config plugin. It writes the line below and skips it when
SwiftPM is on:

```json
{ "plugins": ["@padosoft/ecr17"] }
```

In a bare app, add the line to the app target in the `Podfile` (and drop it if you
switch SwiftPM on):

```ruby
pod 'Ecr17', :path => '../node_modules/@padosoft/ecr17'
```

Android needs nothing: the binding's CMake finds this package with Node. On React
Native Windows, its sources are listed under `nativeKit.windows` in `package.json`.

### iOS / visionOS pod

`Ecr17.podspec` (`native_kit` from `@padosoft/native-modules`, loaded through Node at
`pod install`) compiles the C++ sources (no prebuilt) with public headers under
`<ecr17/…>`, and a C++-only module map.

## Tests

```bash
cmake -S packages/ecr17 -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

| Suite | Runs on | Covers |
|---|---|---|
| `ecr17_tests` | any host (CI: `cpp-tests`) | LRC, framing, builders, parsers, session, retry policy, client, protocol flows; `Integration.RealTerminalStatus` with `ECR17_TERMINAL_HOST` set |
| `ecr17_posix_tests` | macOS, Linux (CI: `cpp-tests`) | `PosixTransport` against a loopback server: pre-send probe, never writes, never consumes a protocol byte, one drop signal, no SIGPIPE |
| `ecr17_winsock_tests` | Windows | `WinsockTransport`: the same contract |

GoogleTest comes from `find_package(GTest)` when installed, otherwise from FetchContent.

The Node.js API is tested end to end (TypeScript → addon → core → TCP) against a scripted
fake terminal, including that a payment interrupted by a drop goes out exactly once
(CI: `Node.js addon`, on Linux, macOS and Windows):

```bash
cd packages/ecr17 && bun run build:node && bun run test:node
```

## Layout

```
cpp/include/ecr17/      public headers: LrcMode, Lcr, PacketCodec, Ecr17Protocol, Ecr17Response,
                        Ecr17Session, RetryPolicy, Transport, ClientTypes, Ecr17Client
cpp/src/                the core's sources
cpp/tests/              GoogleTest + FakeTransport
posix/                  PosixTransport + its loopback tests
windows/                WinsockTransport + its loopback tests
src/                    the Node.js API (TypeScript, built with tsdown into dist/)
node/                   the Node-API addon (addon.cpp) and its CMake project (cmake-js)
test/                   Node tests (node --test) + a fake ECR17 terminal
Ecr17.podspec           iOS / visionOS pod
app.plugin.js           Expo config plugin (adds the pod unless SwiftPM is on)
react-native.config.js  opts out of autolinking
../../Package.swift     the Swift package (repo root: SwiftPM resolves it there)
CMakeLists.txt          ecr17::ecr17 + tests
```

A new source in `cpp/src/` goes in `CMakeLists.txt`. The pod, the Android CMake
of the binding, the Windows DLL project and the Nitro host pick it up by glob.
