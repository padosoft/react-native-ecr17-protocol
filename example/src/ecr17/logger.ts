// Log store for the ECR17 debug console.
//
// Keeps a bounded in-memory ring buffer for the live UI (pub/sub) and mirrors
// every line to a file in the app's document directory (debounced full rewrite,
// since the SDK 56 File API has no native append). Export/share the file via
// expo-sharing. Nothing here ever throws to its callers.

import { File, Paths } from 'expo-file-system';
import * as Sharing from 'expo-sharing';

export type LogLevel = 'info' | 'sent' | 'recv' | 'progress' | 'receipt' | 'ok' | 'ko' | 'error';

export interface LogEntry {
  id: string;
  ts: number;
  level: LogLevel;
  label: string;
  detail?: string;
}

const UI_MAX = 500; // entries kept for the live list
const FILE_MAX = 5000; // lines kept in the mirrored file
const FILE_NAME = 'ecr17-debug.log';
const FLUSH_DEBOUNCE_MS = 400;

let entries: LogEntry[] = [];
let fileLines: string[] = [];
let seq = 0;
let flushTimer: ReturnType<typeof setTimeout> | null = null;

const listeners = new Set<(entries: LogEntry[]) => void>();

function emit(): void {
  const snapshot = entries.slice();
  for (const listener of listeners) {
    listener(snapshot);
  }
}

function logFile(): File {
  return new File(Paths.document, FILE_NAME);
}

function scheduleFlush(): void {
  if (flushTimer != null) {
    return;
  }
  flushTimer = setTimeout(() => {
    flushTimer = null;
    try {
      logFile().write(fileLines.join('\n') + '\n');
    } catch {
      // Best-effort file mirror; ignore write failures.
    }
  }, FLUSH_DEBOUNCE_MS);
}

/** Subscribe to the live log. Immediately receives the current snapshot. */
export function subscribe(cb: (entries: LogEntry[]) => void): () => void {
  listeners.add(cb);
  cb(entries.slice());
  return () => {
    listeners.delete(cb);
  };
}

/** Append a log entry (memory + file). Never throws. */
export function log(level: LogLevel, label: string, detail?: string): void {
  const entry: LogEntry = { id: `${Date.now()}-${seq++}`, ts: Date.now(), level, label, detail };

  entries.push(entry);
  if (entries.length > UI_MAX) {
    entries = entries.slice(-UI_MAX);
  }
  emit();

  const line = `${new Date(entry.ts).toISOString()} [${level}] ${label}${detail ? ' ' + detail : ''}`;
  fileLines.push(line);
  if (fileLines.length > FILE_MAX) {
    fileLines = fileLines.slice(-FILE_MAX);
  }
  scheduleFlush();
}

/** Clear the in-memory log and delete the mirrored file. */
export function clear(): void {
  entries = [];
  fileLines = [];
  emit();
  // Cancel any pending flush so it can't recreate the file after deletion.
  if (flushTimer != null) {
    clearTimeout(flushTimer);
    flushTimer = null;
  }
  try {
    const file = logFile();
    if (file.exists) {
      file.delete();
    }
  } catch {
    // ignore
  }
}

/** Flush pending lines and open the OS share sheet for the log file. */
export async function exportShare(): Promise<void> {
  // Never throws: the Export button calls this without a catch, so a failed or
  // cancelled share must not surface as an unhandled rejection.
  try {
    logFile().write(fileLines.join('\n') + '\n');
    if (await Sharing.isAvailableAsync()) {
      await Sharing.shareAsync(logFile().uri);
    }
  } catch {
    // ignore write/share failures (incl. user cancellation)
  }
}
