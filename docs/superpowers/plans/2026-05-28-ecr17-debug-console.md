# ECR17 Debug Console Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Expo example home screen with an ECR17 Debug Console that configures/connects to a POS, runs every command, and streams behind-the-scenes logs (on screen + file) with progress/loading effects; verified by an android-build APK artifact.

**Architecture:** Isolated units — `logger` (file+memory log store), `commands` (command metadata/param schema), `useEcr17` (client lifecycle hook), and focused UI components composed by the home route. NativeWind for styling, Reanimated for effects.

**Tech Stack:** Expo (RN 0.85), expo-router, NativeWind v5 + Tailwind, react-native-reanimated, expo-file-system, expo-sharing, react-native-ecr17 (workspace), Nitro.

---

## Phase 0 — Dependencies & tooling

### Task 0.1: Add the package + libs to the example
**Files:** Modify `example/package.json`

- [ ] Add deps: `"react-native-ecr17": "workspace:*"`, `"react-native-nitro-modules": "*"`, `"nativewind"`, `"tailwindcss"`, `"expo-file-system"`, `"expo-sharing"`. Keep versions Expo-SDK-56 compatible (use `bunx expo install` for the expo-* ones).
- [ ] Run `bun install` at repo root.
- [ ] Verify: `bun install` succeeds; `react-native-ecr17` resolves to the workspace.

### Task 0.2: NativeWind v5 + Tailwind setup
Use the `expo-app-design:expo-tailwind-setup` skill. Produces/updates: `example/tailwind.config.js`, `example/metro.config.js`, `example/babel.config.js`, `example/global.css` (Tailwind directives), `example/nativewind-env.d.ts`.
- [ ] Configure `content` globs to include `src/**/*.{ts,tsx}`.
- [ ] Verify: `cd example && bunx tsc --noEmit` passes; a `className` on a View typechecks.

## Phase 1 — Core logic units (no UI)

### Task 1.1: `logger.ts`
**Files:** Create `example/src/ecr17/logger.ts`

Implements an in-memory ring buffer + file mirror + pub/sub.

```ts
import * as FileSystem from 'expo-file-system';
import * as Sharing from 'expo-sharing';

export type LogLevel = 'info' | 'sent' | 'recv' | 'progress' | 'receipt' | 'ok' | 'ko' | 'error';
export interface LogEntry { id: string; ts: number; level: LogLevel; label: string; detail?: string; }

const MAX = 500;
const FILE = FileSystem.documentDirectory + 'ecr17-debug.log';
let entries: LogEntry[] = [];
const listeners = new Set<(e: LogEntry[]) => void>();
let seq = 0;

function emit() { const snap = entries.slice(); listeners.forEach((l) => l(snap)); }

export function subscribe(cb: (e: LogEntry[]) => void): () => void {
  listeners.add(cb); cb(entries.slice()); return () => listeners.delete(cb);
}

export function log(level: LogLevel, label: string, detail?: string) {
  const e: LogEntry = { id: `${Date.now()}-${seq++}`, ts: Date.now(), level, label, detail };
  entries.push(e); if (entries.length > MAX) entries = entries.slice(-MAX); emit();
  const line = `${new Date(e.ts).toISOString()} [${level}] ${label}${detail ? ' ' + detail : ''}\n`;
  // Fire-and-forget append; never throw to callers.
  FileSystem.writeAsStringAsync(FILE, line, { encoding: FileSystem.EncodingType.UTF8 })
    .catch(() => {}); // NOTE: replaced by append helper below
}

export function clear() { entries = []; emit(); FileSystem.deleteAsync(FILE, { idempotent: true }).catch(() => {}); }
export async function exportShare() {
  if (await Sharing.isAvailableAsync()) await Sharing.shareAsync(FILE);
}
```

- [ ] **Append semantics:** implement file append correctly. `expo-file-system` has no append; maintain an in-memory string buffer and `writeAsStringAsync(FILE, buffer)` (debounced), OR keep a module-level `fileBuffer` string and rewrite. Implement `fileBuffer += line` then debounced write.
- [ ] Verify: `tsc` passes; no `any`.

### Task 1.2: `commands.ts`
**Files:** Create `example/src/ecr17/commands.ts`

Static metadata driving the palette + dynamic params form.

```ts
export type FieldKind = 'money' | 'text' | 'number' | 'bool' | 'enum';
export interface Field { name: string; label: string; kind: FieldKind; required?: boolean; options?: { label: string; value: string }[]; placeholder?: string; }
export interface CommandDef { key: string; label: string; letter: string; danger?: boolean; fields: Field[]; }

const paymentType: Field = { name: 'paymentType', label: 'Card type', kind: 'enum', options: [
  { label: 'auto', value: 'auto' }, { label: 'debit', value: 'debit' }, { label: 'credit', value: 'credit' }, { label: 'other', value: 'other' } ] };

export const COMMANDS: CommandDef[] = [
  { key: 'status', label: 'Status', letter: 's', fields: [] },
  { key: 'pay', label: 'Pay', letter: 'P', danger: true, fields: [ { name: 'amountCents', label: 'Amount', kind: 'money', required: true }, paymentType, { name: 'cardAlreadyPresent', label: 'Card already present', kind: 'bool' }, { name: 'receiptText', label: 'Receipt text', kind: 'text' } ] },
  { key: 'payExtended', label: 'Pay (extended)', letter: 'X', danger: true, fields: [ /* same as pay */ ] },
  { key: 'reverse', label: 'Reverse', letter: 'S', danger: true, fields: [ { name: 'stan', label: 'STAN (blank = last)', kind: 'text' } ] },
  { key: 'preAuth', label: 'Pre-auth', letter: 'p', danger: true, fields: [ { name: 'amountCents', label: 'Amount', kind: 'money', required: true }, paymentType, { name: 'cardAlreadyPresent', label: 'Card already present', kind: 'bool' }, { name: 'receiptText', label: 'Receipt text', kind: 'text' } ] },
  { key: 'incrementalAuth', label: 'Incremental auth', letter: 'i', danger: true, fields: [ { name: 'amountCents', label: 'Amount', kind: 'money', required: true }, { name: 'originalPreAuthCode', label: 'Original pre-auth code', kind: 'text', required: true } ] },
  { key: 'preAuthClosure', label: 'Pre-auth closure', letter: 'c', danger: true, fields: [ { name: 'amountCents', label: 'Amount', kind: 'money', required: true }, { name: 'originalPreAuthCode', label: 'Original pre-auth code', kind: 'text', required: true } ] },
  { key: 'verifyCard', label: 'Verify card', letter: 'H', fields: [ paymentType ] },
  { key: 'closeSession', label: 'Close session', letter: 'C', fields: [] },
  { key: 'totals', label: 'Totals', letter: 'T', fields: [] },
  { key: 'sendLastResult', label: 'Send last result (G)', letter: 'G', fields: [] },
  { key: 'enableEcrPrinting', label: 'Enable ECR printing', letter: 'E', fields: [ { name: 'enabled', label: 'Enabled', kind: 'bool' } ] },
  { key: 'reprint', label: 'Reprint', letter: 'R', fields: [ { name: 'toEcr', label: 'To ECR', kind: 'bool' } ] },
  { key: 'vas', label: 'VAS', letter: 'K', fields: [ { name: 'xmlRequest', label: 'XML request', kind: 'text', required: true } ] },
];
```
- [ ] Fill `payExtended.fields` identical to `pay` (no placeholder).
- [ ] Verify: `tsc` passes.

### Task 1.3: `useEcr17.ts`
**Files:** Create `example/src/ecr17/useEcr17.ts`

```ts
import { useCallback, useEffect, useRef, useState } from 'react';
import { createEcr17Client, type Ecr17Client, type Ecr17Config, type ConnectionState } from 'react-native-ecr17';
import { log } from './logger';

export function useEcr17(config: Ecr17Config) {
  const clientRef = useRef<Ecr17Client | null>(null);
  const [connectionState, setConnectionState] = useState<ConnectionState>('disconnected');
  const [busy, setBusy] = useState(false);
  const [lastProgress, setLastProgress] = useState<string>('');

  const ensureClient = useCallback(() => {
    if (!clientRef.current) {
      const c = createEcr17Client(config);
      c.setOnConnectionStateChange((s) => { setConnectionState(s); log('info', `connection: ${s}`); });
      c.setOnProgress((e) => { setLastProgress(e.message); log('progress', e.message); });
      c.setOnReceiptLine((l) => log('receipt', l.text));
      clientRef.current = c;
    } else {
      clientRef.current.configure(config);
    }
    return clientRef.current;
  }, [config]);

  const connect = useCallback(async () => {
    setBusy(true); log('sent', 'connect()');
    try { await ensureClient().connect(); log('ok', 'connected'); }
    catch (e) { log('error', 'connect failed', String(e)); }
    finally { setBusy(false); }
  }, [ensureClient]);

  const disconnect = useCallback(() => { try { clientRef.current?.disconnect(); log('info', 'disconnect()'); } catch (e) { log('error', 'disconnect failed', String(e)); } }, []);

  const run = useCallback(async (key: string, params: Record<string, unknown>) => {
    const c = ensureClient(); setBusy(true); setLastProgress('');
    log('sent', key, JSON.stringify(params));
    try {
      // dispatch table maps key -> typed client call (see Task 3.x)
      const result = await dispatch(c, key, params);
      const outcome = (result as { outcome?: string })?.outcome;
      log(outcome === 'ko' ? 'ko' : 'ok', `${key} result`, JSON.stringify(result));
      return result;
    } catch (e) { log('error', `${key} failed`, String(e)); }
    finally { setBusy(false); }
  }, [ensureClient]);

  useEffect(() => () => { try { clientRef.current?.disconnect(); } catch {} }, []);
  return { connectionState, busy, lastProgress, connect, disconnect, run };
}
```
- [ ] Implement `dispatch(client, key, params)` mapping each command key to the typed client method, converting `amountCents` (euros→cents handled in UI), building request objects. No placeholders — one `case` per command.
- [ ] Verify: `tsc` passes.

## Phase 2 — UI components

### Task 2.1: `ConnectionBar.tsx`
**Files:** Create `example/src/components/ecr17/ConnectionBar.tsx`
- [ ] Props: `{ state: ConnectionState; busy: boolean; onConnect(); onDisconnect(); }`.
- [ ] Animated pulsing dot (Reanimated `withRepeat(withTiming(...))`), color by state (gray/amber/green). Connect/Disconnect buttons (disabled while busy).
- [ ] Verify: `tsc`.

### Task 2.2: `ConfigForm.tsx`
**Files:** Create `example/src/components/ecr17/ConfigForm.tsx`
- [ ] Props: `{ value: Ecr17Config; onChange(next): void; }`. Collapsible. Fields: host, port, terminalId, cashRegisterId, lrcMode (enum), keepAlive/autoReconnect/debug (switches), connection/response/ack timeouts, retryCount/Delay, receiptDrainMs.
- [ ] Verify: `tsc`.

### Task 2.3: `CommandParamsSheet.tsx`
**Files:** Create `example/src/components/ecr17/CommandParamsSheet.tsx`
- [ ] Props: `{ command: CommandDef | null; onSubmit(key, params); onClose(); }`. Renders a field per `command.fields` by `kind` (money input shows €, converts to cents on submit; bool→Switch; enum→segmented). Modal/bottom sheet.
- [ ] Verify: `tsc`.

### Task 2.4: `LogConsole.tsx`
**Files:** Create `example/src/components/ecr17/LogConsole.tsx`
- [ ] Subscribes to `logger.subscribe`; inverted `FlatList`; color per level; monospace; new rows fade-in (Reanimated `entering`); buttons: Clear, Export/Share (calls `exportShare`). Auto-scroll.
- [ ] Verify: `tsc`.

### Task 2.5: `BusyOverlay.tsx`
**Files:** Create `example/src/components/ecr17/BusyOverlay.tsx`
- [ ] Props: `{ visible: boolean; progress: string; }`. Absolute overlay, ActivityIndicator + latest progress text + subtle fade.
- [ ] Verify: `tsc`.

## Phase 3 — Screen composition & persistence

### Task 3.1: Config persistence helper
**Files:** Create `example/src/ecr17/storage.ts`
- [ ] `loadConfig(): Promise<Ecr17Config>` (defaults if absent), `saveConfig(c)`, using a JSON file in `documentDirectory`. Defaults: `{ host:'', port:1024, terminalId:'', cashRegisterId:'', lrcMode:'std', keepAlive:true, autoReconnect:true, connectionTimeoutMs:5000, responseTimeoutMs:60000, ackTimeoutMs:2000, retryCount:3, retryDelayMs:200, receiptDrainMs:0, debug:true }`.
- [ ] Verify: `tsc`.

### Task 3.2: Home screen
**Files:** Modify `example/src/app/index.tsx` (replace contents)
- [ ] Load persisted config; hold in state; `useEcr17(config)`. Layout: `ConnectionBar` (top), collapsible `ConfigForm` (persists on change), `CommandPalette` (buttons from `COMMANDS`; param-less run directly, param ones open `CommandParamsSheet`), `LogConsole` (flex-1), `BusyOverlay`. Outcome toast.
- [ ] Verify: `tsc` passes for the whole example.

### Task 3.3: CommandPalette
**Files:** Create `example/src/components/ecr17/CommandPalette.tsx`
- [ ] Props: `{ onPick(cmd: CommandDef); disabled: boolean; }`. Grid of buttons from `COMMANDS`; danger commands styled distinctly. Press feedback.
- [ ] Verify: `tsc`.

## Phase 4 — CI: build & publish APK artifact

### Task 4.1: Extend android-build to upload a runnable APK
**Files:** Modify `.github/workflows/android-build.yml`
- [ ] After `assembleDebug`, upload the APK:
```yaml
      - name: Upload debug APK
        uses: actions/upload-artifact@v4
        with:
          name: ecr17-example-debug-apk
          path: example/android/app/build/outputs/apk/debug/*.apk
          if-no-found: error
```
  (use `if-no-files-found: error`).
- [ ] If the debug APK requires Metro, add a JS bundling step (or `assembleRelease` with a debug-signing config) so the artifact launches standalone. Decide during execution based on what `assembleDebug` produces; document the run instructions in the artifact/README.
- [ ] Verify: dispatch `android-build`; job green; artifact present.

## Phase 5 — Verification loop (Definition of Done)

- [ ] Local loop: `cd example && bunx tsc --noEmit` green; focused local Copilot review of new files.
- [ ] Remote loop: push; CI `ts-checks` + (native touched) `android-build` green; request Copilot review; fix valid comments until zero; then merge.
- [ ] Runtime: user installs the APK artifact, exercises connect + each command against a POS/emulator; any package bug found is fixed → recompile → re-verify until it works.

## Self-review notes
- Spec coverage: logger(1.1), commands(1.2), hook(1.3), ConnectionBar(2.1), ConfigForm(2.2), ParamsSheet(2.3), LogConsole(2.4), BusyOverlay(2.5), persistence(3.1), screen(3.2), palette(3.3), CI artifact(4.1), verification(5). All spec sections covered.
- The `logger.ts` first draft's `writeAsStringAsync` overwrites; Task 1.1 explicitly requires the append/debounce fix (no placeholder left in final code).
- `dispatch()` referenced in 1.3 is defined as a required sub-step (per-command case) in 1.3 — must be concrete in code, not a placeholder.
