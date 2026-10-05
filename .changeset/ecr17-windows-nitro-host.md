---
"@padosoft/react-native-ecr17": minor
---

React Native Windows: the package no longer ships its own Windows project (`windows/Ecr17`, which installed Nitro itself). The app adds the shared Nitro host, `@padosoft/react-native-nitro-windows`, which installs Nitro once and compiles the Windows C++ declared under `nitroWindows` in `package.json` together with the Kit's. Apps use the host's helper in `react-native.config.js` (`require("@padosoft/react-native-nitro-windows")`), then run `npx react-native autolink-windows`. The host is not published yet.
