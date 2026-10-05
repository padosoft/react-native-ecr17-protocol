// Helpers to classify and safely render ECR17 command results for logging/UI.
// Shared by useEcr17 (log level) and the screen (toast) so the two never drift.

/**
 * Whether a resolved command result represents a non-success.
 * - void / no status field (e.g. status, enableEcrPrinting) → success
 * - VAS results report success via `responseId === "0"`
 * - everything else uses `outcome` ('ok' is the only success)
 */
export function isFailure(result: unknown): boolean {
  if (result === undefined || result === null) {
    return false;
  }
  const r = result as { outcome?: string; responseId?: string };
  if (typeof r.responseId === 'string') {
    return r.responseId !== '0';
  }
  if (typeof r.outcome === 'string') {
    return r.outcome !== 'ok';
  }
  return false;
}

function maskPan(pan: string): string {
  const digits = pan.replace(/\D/g, '');
  if (digits.length <= 4) {
    return '****';
  }
  return '*'.repeat(digits.length - 4) + digits.slice(-4);
}

function maskSensitive(value: unknown): unknown {
  if (value instanceof Date) {
    // Preserve dates (e.g. PosStatusResponse.terminalDateTime) — a plain object
    // copy would serialize them as `{}`.
    return value.toISOString();
  }
  if (Array.isArray(value)) {
    return value.map(maskSensitive);
  }
  if (value && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [k, v] of Object.entries(value)) {
      out[k] = k === 'pan' && typeof v === 'string' ? maskPan(v) : maskSensitive(v);
    }
    return out;
  }
  return value;
}

/** JSON string of a result with cardholder PAN masked, safe for screen + file logs. */
export function safeDetail(result: unknown): string {
  if (result === undefined) {
    return 'ok';
  }
  return JSON.stringify(maskSensitive(result));
}
