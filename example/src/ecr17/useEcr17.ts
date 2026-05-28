// React hook that owns the Ecr17Client lifecycle and exposes a uniform `run`
// for the debug screen. Wires native events to the logger and never throws to
// the UI (errors are logged).

import { useCallback, useEffect, useRef, useState } from 'react';
import {
  createEcr17Client,
  type ConnectionState,
  type Ecr17Client,
  type Ecr17Config,
  type PaymentCardType,
} from 'react-native-ecr17';
import { log } from './logger';

type Params = Record<string, unknown>;

export interface UseEcr17 {
  connectionState: ConnectionState;
  busy: boolean;
  lastProgress: string;
  connect: () => Promise<void>;
  disconnect: () => void;
  run: (key: string, params: Params) => Promise<unknown>;
}

function dispatch(client: Ecr17Client, key: string, p: Params): Promise<unknown> {
  const str = (k: string): string | undefined => {
    const v = p[k];
    return typeof v === 'string' && v.length > 0 ? v : undefined;
  };
  const num = (k: string): number => (typeof p[k] === 'number' ? (p[k] as number) : 0);
  const bool = (k: string): boolean => p[k] === true;
  const card = (): PaymentCardType | undefined => {
    const v = p.paymentType;
    return typeof v === 'string' ? (v as PaymentCardType) : undefined;
  };

  switch (key) {
    case 'status':
      return client.status();
    case 'pay':
      return client.pay({
        amountCents: num('amountCents'),
        paymentType: card(),
        cardAlreadyPresent: bool('cardAlreadyPresent'),
        receiptText: str('receiptText'),
      });
    case 'payExtended':
      return client.payExtended({
        amountCents: num('amountCents'),
        paymentType: card(),
        cardAlreadyPresent: bool('cardAlreadyPresent'),
        receiptText: str('receiptText'),
      });
    case 'reverse':
      return client.reverse({ stan: str('stan') });
    case 'preAuth':
      return client.preAuth({
        amountCents: num('amountCents'),
        paymentType: card(),
        cardAlreadyPresent: bool('cardAlreadyPresent'),
        receiptText: str('receiptText'),
      });
    case 'incrementalAuth':
      return client.incrementalAuth({
        amountCents: num('amountCents'),
        originalPreAuthCode: str('originalPreAuthCode') ?? '',
        receiptText: str('receiptText'),
      });
    case 'preAuthClosure':
      return client.preAuthClosure({
        amountCents: num('amountCents'),
        originalPreAuthCode: str('originalPreAuthCode') ?? '',
        receiptText: str('receiptText'),
      });
    case 'verifyCard':
      return client.verifyCard({ paymentType: card() });
    case 'closeSession':
      return client.closeSession();
    case 'totals':
      return client.totals();
    case 'sendLastResult':
      return client.sendLastResult();
    case 'enableEcrPrinting':
      return client.enableEcrPrinting(bool('enabled'));
    case 'reprint':
      return client.reprint(bool('toEcr'));
    case 'vas':
      return client.vas(str('xmlRequest') ?? '');
    default:
      return Promise.reject(new Error(`Unknown command: ${key}`));
  }
}

export function useEcr17(config: Ecr17Config): UseEcr17 {
  const clientRef = useRef<Ecr17Client | null>(null);
  const configRef = useRef(config);
  configRef.current = config;
  // Serialized form of the config last pushed to the client, so we re-`configure`
  // only when the operator actually changed something (configure resets the
  // transport, so we must not call it on every command).
  const appliedRef = useRef('');

  const [connectionState, setConnectionState] = useState<ConnectionState>('disconnected');
  const [busy, setBusy] = useState(false);
  const [lastProgress, setLastProgress] = useState('');

  const ensureClient = useCallback((): Ecr17Client => {
    if (!clientRef.current) {
      const c = createEcr17Client(configRef.current);
      c.setOnConnectionStateChange((s) => {
        setConnectionState(s);
        log('info', `connection: ${s}`);
      });
      c.setOnProgress((e) => {
        setLastProgress(e.message);
        log('progress', e.message);
      });
      c.setOnReceiptLine((l) => log('receipt', l.text));
      clientRef.current = c;
    }
    return clientRef.current;
  }, []);

  // Push the latest form config into the client before connecting/running, so a
  // command never uses stale settings just because Connect wasn't pressed again.
  const applyConfig = useCallback((c: Ecr17Client) => {
    const serialized = JSON.stringify(configRef.current);
    if (serialized !== appliedRef.current) {
      c.configure(configRef.current);
      appliedRef.current = serialized;
    }
  }, []);

  const connect = useCallback(async () => {
    const c = ensureClient();
    setBusy(true);
    log('sent', 'connect()', JSON.stringify(configRef.current));
    try {
      applyConfig(c); // push the latest form config
      await c.connect();
      log('ok', 'connected');
    } catch (e) {
      log('error', 'connect failed', String(e));
    } finally {
      setBusy(false);
    }
  }, [ensureClient, applyConfig]);

  const disconnect = useCallback(() => {
    try {
      clientRef.current?.disconnect();
      log('info', 'disconnect()');
    } catch (e) {
      log('error', 'disconnect failed', String(e));
    }
  }, []);

  const run = useCallback(
    async (key: string, params: Params): Promise<unknown> => {
      const c = ensureClient();
      applyConfig(c); // ensure the command uses the current form config
      setBusy(true);
      setLastProgress('');
      log('sent', key, JSON.stringify(params));
      try {
        const result = await dispatch(c, key, params);
        const outcome = (result as { outcome?: string } | undefined)?.outcome;
        const detail = result === undefined ? 'ok' : JSON.stringify(result);
        // Any defined outcome other than 'ok' (ko / cardNotPresent / unknownTag /
        // unknown) is a non-success; void results (no outcome) are ok.
        const failed = outcome !== undefined && outcome !== 'ok';
        log(failed ? 'ko' : 'ok', `${key} →`, detail);
        return result;
      } catch (e) {
        log('error', `${key} failed`, String(e));
        return undefined;
      } finally {
        setBusy(false);
      }
    },
    [ensureClient, applyConfig]
  );

  useEffect(
    () => () => {
      try {
        clientRef.current?.disconnect();
      } catch {
        // ignore on unmount
      }
    },
    []
  );

  return { connectionState, busy, lastProgress, connect, disconnect, run };
}
