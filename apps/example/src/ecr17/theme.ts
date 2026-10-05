// Dark "console" theme for the ECR17 debug screen. Plain values consumed by
// StyleSheet across the ecr17 components — one source of truth for colors.

import type { LogLevel } from './logger';

export const colors = {
  bg: '#0b0f14',
  surface: '#121821',
  surfaceAlt: '#1a2230',
  border: '#243040',
  text: '#e6edf3',
  textDim: '#8b98a5',
  accent: '#3b82f6',
  accentText: '#ffffff',
  danger: '#ef4444',
  ok: '#22c55e',
  ko: '#f59e0b',
  warn: '#f59e0b',
  error: '#ef4444',
  // connection states
  disconnected: '#6b7280',
  connecting: '#f59e0b',
  connected: '#22c55e',
} as const;

export const space = { xs: 4, sm: 8, md: 12, lg: 16, xl: 24 } as const;

export const radius = { sm: 6, md: 10, lg: 14 } as const;

export const font = {
  mono: 'monospace',
  size: { xs: 11, sm: 13, md: 15, lg: 18, xl: 22 },
} as const;

// Color used to render a given log level in the console.
export const logLevelColor: Record<LogLevel, string> = {
  info: colors.textDim,
  sent: colors.accent,
  recv: colors.text,
  progress: colors.connecting,
  receipt: '#a78bfa',
  ok: colors.ok,
  ko: colors.ko,
  error: colors.error,
};
