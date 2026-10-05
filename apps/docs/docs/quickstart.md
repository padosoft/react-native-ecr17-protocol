---
title: Quickstart
description: Connect to an ECR17 terminal and run a first payment safely.
---

# Quickstart

Use this path when the terminal is already configured for LAN ECR integration and your React Native app uses the new architecture.

::: steps
1. Install the package and Nitro peer dependency.

   ```bash
   npm install react-native-ecr17 react-native-nitro-modules
   cd ios && pod install
   ```

2. Create a client.

   ```ts
   import { createEcr17Client } from "react-native-ecr17";

   const client = createEcr17Client({
     host: "192.168.1.50",
     port: 10000,
     terminalId: "12345678",
     cashRegisterId: "00000001",
     lrcMode: "std",
     responseTimeoutMs: 60000
   });
   ```

3. Connect before commands.

   ```ts
   await client.connect();
   ```

4. Start with a read-only command.

   ```ts
   const status = await client.status();
   console.log(status.status, status.softwareRelease);
   ```

5. Run a payment only after terminal identity and LRC mode are confirmed.

   ```ts
   const result = await client.pay({ amountCents: 650 });

   if (result.outcome === "ok") {
     console.log("Approved", result.authCode, result.stan);
   } else {
     console.warn("Declined", result.resultCode, result.errorDescription);
   }
   ```
:::

::: callout danger "Do not replay payments automatically"
If a payment fails because the socket drops, do not call `pay()` again as an automatic retry. Reconnect and call `sendLastResult()` to recover the terminal's latest result.
:::

## First success checklist

- The phone and terminal are on the same LAN.
- The terminal ECR port matches `port`.
- `terminalId` and `cashRegisterId` use the values configured on the terminal estate.
- `lrcMode` matches the terminal firmware behavior.
- The app is a native iOS or Android build, not only a web preview.

::: tabs
== tab "iOS"
Run `pod install` after dependency changes, then rebuild the app so Nitro codegen and Swift transport are included.

== tab "Android"
Rebuild the native app after dependency changes so autolinking loads the `Ecr17Package` and native library.
:::
