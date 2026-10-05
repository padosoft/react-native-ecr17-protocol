# @padosoft/ecr17-node

The **ECR17** payment-terminal protocol (Nexi Group POS terminals over LAN) for **Node.js**:
the API of [`@padosoft/react-native-ecr17`](../react-native-ecr17) on the same C++ core,
[`@padosoft/ecr17`](../ecr17), through a Node-API addon. For POS back-ends, kiosks,
command-line tools and tests on macOS, Linux and Windows.

> ⚠️ **Not published on npm yet**: it depends on `@padosoft/ecr17`, whose build tooling
> (`@padosoft/native-modules`) is on the private GitHub Packages registry for now.

## Usage

```ts
import { createEcr17Client } from "@padosoft/ecr17-node";

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

- It runs on the same C++ client ([`@padosoft/ecr17`](../ecr17)), through Node-API: same auto-connect, pre-send probe and
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
other platform, build it from source in the package folder (CMake 3.21+ and a C++20
compiler; the core's sources come from the `@padosoft/ecr17` dependency):

```bash
npx cmake-js compile --directory node --out build
```

`ECR17_NODE_ADDON=/path/to/ecr17.node` overrides the lookup.

## How it works

```
Ecr17Client (src/, TypeScript)          the public API, mapping like the RN binding's C++
  └─ ecr17.node (node/addon.cpp)        Node-API: requests in, the core's raw structs out;
                                         one worker thread per client, one ordered queue back
       └─ padosoft::ecr17::Ecr17Client  @padosoft/ecr17: protocol, session, money-safe retry
            └─ PosixTransport | WinsockTransport
```

## Development

```bash
cd packages/ecr17-node
bun run build:node   # node/ -> build/Release/ecr17.node (cmake-js)
bun run test:node    # node --test: 17 tests against a scripted fake terminal over real TCP
bun run build        # src/ -> dist/ (tsdown, @padosoft/config preset; ESM + CJS)
bun run typecheck
```

The tests drive TypeScript → addon → core → TCP. They include a payment interrupted by a
drop, which must go out exactly once and be recovered with `sendLastResult()`. CI
(`Node.js addon`) runs them on linux-x64, linux-arm64, darwin-arm64 and win32-x64 and
uploads each `ecr17.node`; the release ships them in `prebuilds/`.

```
src/              the TypeScript API (client, mappers, types, addon loader)
node/             the addon (addon.cpp) and its CMake project; finds @padosoft/ecr17 with Node,
                  or the monorepo sibling, or -DECR17_DIR
test/             node --test suites + a fake ECR17 terminal (fake-terminal.ts)
tsconfig.ci.json  self-contained typecheck config for CI (no private @padosoft/config)
```

`src/types.ts` mirrors the React Native package's types and `src/mappers.ts` mirrors
`HybridEcr17Client.cpp`'s mapping: change them together.
