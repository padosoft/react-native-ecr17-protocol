# ECR17 Debug Console — example app design

Date: 2026-05-28
Status: approved (brainstorming)
Branch: `feat/example-ecr17-debug-console`

## Goal

Turn the Expo example app into a single **ECR17 Debug Console** screen that is
both (a) a developer debugging tool for the `react-native-ecr17` package and
(b) a real, copy-pasteable implementation example of the whole public API.

It must let a developer:
- configure and connect to a POS terminal,
- run every implemented command,
- see, in real time, what happens behind the scenes (sent commands, progress
  events, receipt lines, results, errors) on screen **and** in a log file,
- do this with a polished UI (loading/progress effects).

It also serves as the runtime verification vehicle for the native module: the
`android-build` CI compiles the Nitro module into the example and publishes an
installable APK artifact.

## Decisions (from brainstorming)

- **Runtime verification**: build a debug APK (dev-client capable, JS bundled so
  it is installable/launchable standalone) in the existing `android-build` CI and
  upload it as an artifact. The user installs it on a device/emulator and tries
  the screen. No local Android SDK/emulator assumed.
- **Placement**: the debug console **replaces the home** route
  (`example/src/app/index.tsx`).
- **Styling**: plain React Native `StyleSheet` with a small dark "console" theme
  module. (NativeWind v5 was considered but is preview + nightly `react-native-css`
  and cannot be build-verified locally; StyleSheet maximizes build robustness.)
- **Logging**: on-screen live console **and** file logging via
  `expo-file-system`, with export/share via `expo-sharing`.
- **Polish**: progress/loading effects using `react-native-reanimated` (already a
  dependency of the example).

## Architecture (isolated units)

Each unit has one purpose, a clear interface, and is independently understandable.

### `example/src/ecr17/logger.ts`
- In-memory ring buffer of log entries `{ id, ts, level, label, detail? }`,
  `level ∈ {info, sent, recv, progress, receipt, ok, ko, error}`.
- Subscribe API for the UI (`subscribe(cb): unsubscribe`), `add(entry)`,
  `clear()`.
- Mirror to a file (`<documentDirectory>/ecr17-debug.log`, appended) and
  `exportShare()` via `expo-sharing`.
- Depends on: `expo-file-system`, `expo-sharing`. No UI/client knowledge.

### `example/src/ecr17/useEcr17.ts`
- React hook owning the `Ecr17Client` lifecycle.
- Inputs: an `Ecr17Config`. Builds the client lazily via `createEcr17Client`.
- Wires `setOnProgress`/`setOnReceiptLine`/`setOnConnectionStateChange` →
  logger + connection state.
- Exposes: `connectionState`, `busy`, `lastProgress`, `connect()`,
  `disconnect()`, and `run(commandKey, params)` which returns the typed result,
  logs `sent`→(`progress`/`receipt`)→`ok`/`ko`/`error`, and never throws to the UI.
- Depends on: the package public API, `logger`.

### `example/src/theme.ts`
- Small dark "console" palette + spacing/typography constants used by all UI
  components (plain `StyleSheet`). One source of truth for colors per log level.

### `example/src/ecr17/commands.ts`
- Static metadata describing every command: key, label, ECR17 letter, whether it
  needs params, and a typed param schema (field name, kind: money/text/number/
  enum/bool, options, required). Drives the dynamic params form.
- Pure data; no React, no client.

### UI components (`example/src/components/ecr17/`)
- `ConnectionBar.tsx` — animated status dot (pulse via Reanimated) +
  Connect/Disconnect.
- `ConfigForm.tsx` — all `Ecr17Config` fields; persisted (see below) so it is not
  re-typed each launch; collapsible.
- `CommandPalette.tsx` — a button per command; param-less commands run directly,
  param commands open the params sheet.
- `CommandParamsSheet.tsx` — bottom sheet rendering fields from `commands.ts`
  (money input shows € and converts to cents), submits to `run`.
- `LogConsole.tsx` — inverted live list, color-coded by level, auto-scroll,
  copy + export/share, clear; new rows fade in.
- `BusyOverlay.tsx` — overlay spinner + the latest `onProgress` message while a
  command is in flight.

### Screen `example/src/app/index.tsx`
Composes: `ConnectionBar` (top), `ConfigForm` (collapsible), `CommandPalette`
(scrollable), `LogConsole` (bottom, flex-1), `BusyOverlay`.

## Config persistence
Persist the last-used `Ecr17Config` to a JSON file in `documentDirectory` (or
AsyncStorage if already available); load on mount; sensible defaults
(host empty, port 1024, lrcMode "std", timeouts as in the C++ defaults).

## Data flow (one command)
1. User taps a command → (optional) fills params sheet → `run(key, params)`.
2. `run` sets `busy=true`, logs `sent` with the human-readable request.
3. During the exchange, `onProgress` updates `BusyOverlay` and logs `progress`;
   `onReceiptLine` logs `receipt`.
4. On resolve: log `ok`/`ko` with the structured result; on throw: log `error`.
5. `busy=false`; a toast shows the outcome.

## Commands covered
status, pay, payExtended, reverse, preAuth, incrementalAuth, preAuthClosure,
verifyCard, closeSession, totals, sendLastResult, enableEcrPrinting, reprint, vas.
Money fields are entered in euros and converted to integer cents.

## Effects / polish
- Pulsing connection dot; color by state.
- `BusyOverlay` spinner + live progress text.
- Log rows fade-in; outcome toast (green ok / red ko / amber error).
- Button press feedback.

## Error handling
`run` wraps every call in try/catch; results (including `ko`) are logged
structurally; the UI never crashes and stays usable after an error or a
mid-session disconnect (auto-reconnect is exercised when enabled in config).

## Packaging / CI
- Add `react-native-ecr17` (workspace), `nativewind`, `tailwindcss`,
  `expo-file-system`, `expo-sharing` (and `expo-dev-client` if needed) to
  `example/package.json`.
- Extend `.github/workflows/android-build.yml`: after `assembleDebug`, bundle the
  JS so the debug APK is launchable standalone, and `actions/upload-artifact` the
  APK. The job already runs `expo prebuild` + `assembleDebug`, so adding the
  package as an example dependency makes it compile the Nitro module for real.

## Testing / verification
- `tsc` (ts-checks) over the example.
- `android-build`: prebuild + assembleDebug with the package linked + APK artifact.
- Runtime end-to-end: user installs the APK and exercises the screen; bugs found
  in the package are fixed, recompiled, and re-verified until it works (per the
  project Definition of Done — local loop + remote loop).

## Out of scope
- iOS dev-client build (no macOS CI).
- Automated on-device UI tests (manual runtime verification for now).
