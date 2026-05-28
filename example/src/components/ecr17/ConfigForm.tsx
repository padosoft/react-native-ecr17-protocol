import { useState } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import type { Ecr17Config, LrcMode } from 'react-native-ecr17';
import { colors, font, space } from '../../ecr17/theme';
import { BoolField, EnumField, NumberField, TextField } from './fields';

interface Props {
  value: Ecr17Config;
  onChange: (next: Ecr17Config) => void;
}

const LRC_OPTIONS = [
  { label: 'std', value: 'std' },
  { label: 'stx', value: 'stx' },
  { label: 'noext', value: 'noext' },
  { label: 'stx_noext', value: 'stx_noext' },
];

export function ConfigForm({ value, onChange }: Props) {
  const [open, setOpen] = useState(true);
  const set = <K extends keyof Ecr17Config>(key: K, v: Ecr17Config[K]) =>
    onChange({ ...value, [key]: v });

  return (
    <View style={styles.card}>
      <Pressable style={styles.header} onPress={() => setOpen((o) => !o)}>
        <Text style={styles.title}>Configuration</Text>
        <Text style={styles.chevron}>{open ? '▾' : '▸'}</Text>
      </Pressable>

      {open && (
        <View style={styles.body}>
          <TextField label="Host" value={value.host} onChange={(v) => set('host', v)} placeholder="192.168.1.50" />
          <NumberField label="Port" value={value.port ?? 10000} onChange={(v) => set('port', v)} />
          <TextField label="Terminal ID" value={value.terminalId} onChange={(v) => set('terminalId', v)} />
          <TextField
            label="Cash register ID"
            value={value.cashRegisterId}
            onChange={(v) => set('cashRegisterId', v)}
          />
          <EnumField
            label="LRC mode"
            value={value.lrcMode ?? 'std'}
            options={LRC_OPTIONS}
            onChange={(v) => set('lrcMode', v as LrcMode)}
          />
          <BoolField label="Keep alive" value={value.keepAlive ?? false} onChange={(v) => set('keepAlive', v)} />
          <BoolField
            label="Auto reconnect"
            value={value.autoReconnect ?? false}
            onChange={(v) => set('autoReconnect', v)}
          />
          <BoolField label="Debug" value={value.debug ?? false} onChange={(v) => set('debug', v)} />
          <NumberField
            label="Connection timeout (ms)"
            value={value.connectionTimeoutMs ?? 5000}
            onChange={(v) => set('connectionTimeoutMs', v)}
          />
          <NumberField
            label="Response timeout (ms)"
            value={value.responseTimeoutMs ?? 60000}
            onChange={(v) => set('responseTimeoutMs', v)}
          />
          <NumberField
            label="ACK timeout (ms)"
            value={value.ackTimeoutMs ?? 2000}
            onChange={(v) => set('ackTimeoutMs', v)}
          />
          <NumberField label="Retry count" value={value.retryCount ?? 3} onChange={(v) => set('retryCount', v)} />
          <NumberField
            label="Retry delay (ms)"
            value={value.retryDelayMs ?? 200}
            onChange={(v) => set('retryDelayMs', v)}
          />
          <NumberField
            label="Receipt drain (ms)"
            value={value.receiptDrainMs ?? 0}
            onChange={(v) => set('receiptDrainMs', v)}
          />
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  card: { backgroundColor: colors.surface, borderBottomWidth: StyleSheet.hairlineWidth, borderBottomColor: colors.border },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: space.lg,
    paddingVertical: space.md,
  },
  title: { color: colors.text, fontWeight: '700', fontSize: font.size.md },
  chevron: { color: colors.textDim, fontSize: font.size.md },
  body: { paddingHorizontal: space.lg, paddingBottom: space.lg },
});
