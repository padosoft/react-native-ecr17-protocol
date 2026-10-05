# @padosoft/ecr17-kit

The **ECR17** payment-terminal protocol (Nexi Group POS terminals over LAN) as a
React-free C++20 library. It is the native core of
[`@padosoft/react-native-ecr17`](../package), and it is usable on its own from
native apps.

| Part | Folder | What |
|---|---|---|
| Protocol core | `cpp/` | LRC, framing, command builders, response parsers, the ACK/NAK session, and `Ecr17Client` (auto-connect, money-safe retry) |
| Windows transport | `windows/` | `WinsockTransport`: TCP with the write-free pre-send liveness probe |

The core builds anywhere a C++20 compiler does. On iOS and Android, the React
Native binding supplies the TCP transport, in Swift and Kotlin. On Windows, the
Kit has its own.

> ⚠️ **Not published on npm yet**, like the `@padosoft` build tooling it uses
> (`@padosoft/native-modules`, `@padosoft/expo`). Those live on the private
> GitHub Packages registry for now (see `bunfig.toml` in this repo).

---

## API

```cpp
#include <Ecr17Kit/Ecr17Client.hpp>
#include <Ecr17Kit/WinsockTransport.hpp>  // Windows

using namespace padosoft::ecr17;

ClientConfig config;
config.host = "192.168.1.50";
config.port = 10000;
config.terminalId = "12345678";
config.cashRegisterId = "00000001";
config.autoReconnect = true;

Ecr17Client client(std::make_shared<WinsockTransport>(), config);
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
  `VasResponse` (`Ecr17Kit/Ecr17Response.hpp`).

### ⚠️ Money safety

With `autoReconnect`, a command interrupted by a dropped connection reconnects
the socket. It is **re-sent only if it is read-only**: `status`, `totals`,
`sendLastResult` and `enableEcrPrinting`. A financial command is **never**
re-sent, because the terminal may already have charged the card.

Recover the outcome of an interrupted payment with `sendLastResult()`, command
`G`. The rule is `shouldRetryAfterReconnect` in
`cpp/include/Ecr17Kit/RetryPolicy.hpp`. It is locked by `test_retry_policy.cpp`
and, end to end, by `Client.AFinancialCommandIsNeverResentAfterADrop`.

### Your own transport

`Transport` (`Ecr17Kit/Transport.hpp`) is a byte stream with five operations.
Any implementation has to keep the contract written in its header:

- `connect` blocks until connected or throws;
- `isConnected` may probe the socket, but never writes to it or consumes a byte;
- the disconnect callback fires once per unexpected drop, never for `disconnect()`.

## Using it

### CMake (Windows, Linux, macOS)

```cmake
add_subdirectory(path/to/ecr17-kit Ecr17Kit)
target_link_libraries(MyApp PRIVATE Ecr17Kit::Ecr17Kit)
```

On Windows the target also contains `WinsockTransport` and links `ws2_32`.
`cmake --install` exports `Ecr17Kit::Ecr17Kit` for `find_package(Ecr17Kit)`.

### React Native

Add [`@padosoft/react-native-ecr17`](../package). For iOS, it needs this pod in
the app's Podfile. Add the config plugin, which uses `@padosoft/expo` (an optional
peer: install it in Expo apps):

```json
{ "plugins": ["@padosoft/ecr17-kit"] }
```

Listing `@padosoft/ecr17-kit` among the app's direct dependencies works too,
since autolinking then finds `Ecr17Kit.podspec`. Android needs nothing: the
binding's CMake finds the Kit with Node. On React Native Windows, the Kit's
sources are listed under `nativeKit.windows` in `package.json`.

### iOS / visionOS pod

`Ecr17Kit.podspec` compiles the C++ sources (no prebuilt) with public headers
under `<Ecr17Kit/…>`, and a C++-only module map. It loads
`@padosoft/native-modules` through Node at `pod install`.

## Tests

```bash
cmake -S kit -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

| Suite | Runs on | Covers |
|---|---|---|
| `ecr17_kit_tests` | any host (CI: `cpp-tests`) | LRC, framing, builders, parsers, session, retry policy, client, protocol flows; `Integration.RealTerminalStatus` with `ECR17_TERMINAL_HOST` set |
| `ecr17_kit_winsock_tests` | Windows | `WinsockTransport` against a loopback server: pre-send probe, never writes, never consumes a protocol byte, one drop signal |

GoogleTest comes from `find_package(GTest)` when installed, otherwise from FetchContent.

## Layout

```
cpp/include/Ecr17Kit/   public headers: LrcMode, Lcr, PacketCodec, Ecr17Protocol, Ecr17Response,
                        Ecr17Session, RetryPolicy, Transport, ClientTypes, Ecr17Client
cpp/src/                the core's sources
cpp/tests/              GoogleTest + FakeTransport + PosixTcpTransport (integration test)
windows/                WinsockTransport + its loopback tests
Ecr17Kit.podspec        iOS / visionOS
app.plugin.js           Expo config plugin (adds the pod)
CMakeLists.txt          Ecr17Kit::Ecr17Kit + tests
```

A new source in `cpp/src/` goes in `CMakeLists.txt`. The pod, the Android CMake
of the binding, the Windows DLL project and the Nitro host pick it up by glob.
