import { Pressable, StyleSheet, Switch, Text, TextInput, View } from 'react-native';
import { colors, font, radius, space } from '../../ecr17/theme';

export function FieldLabel({ children }: { children: string }) {
  return <Text style={styles.label}>{children}</Text>;
}

export function TextField(props: {
  label: string;
  value: string;
  onChange: (v: string) => void;
  placeholder?: string;
  keyboardType?: 'default' | 'numeric';
  autoCapitalize?: 'none' | 'sentences';
}) {
  return (
    <View style={styles.row}>
      <FieldLabel>{props.label}</FieldLabel>
      <TextInput
        value={props.value}
        onChangeText={props.onChange}
        placeholder={props.placeholder}
        placeholderTextColor={colors.textDim}
        keyboardType={props.keyboardType ?? 'default'}
        autoCapitalize={props.autoCapitalize ?? 'none'}
        style={styles.input}
      />
    </View>
  );
}

export function NumberField(props: { label: string; value: number; onChange: (v: number) => void }) {
  return (
    <TextField
      label={props.label}
      value={String(props.value)}
      onChange={(t) => props.onChange(Number(t.replace(/[^0-9]/g, '')) || 0)}
      keyboardType="numeric"
    />
  );
}

export function MoneyField(props: { label: string; cents: number; onChange: (cents: number) => void }) {
  // Edited in euros; stored/emitted as integer cents.
  const euros = (props.cents / 100).toFixed(2);
  return (
    <TextField
      label={props.label}
      value={euros}
      onChange={(t) => {
        const n = Number(t.replace(',', '.').replace(/[^0-9.]/g, ''));
        props.onChange(Number.isFinite(n) ? Math.round(n * 100) : 0);
      }}
      keyboardType="numeric"
      placeholder="0.00"
    />
  );
}

export function BoolField(props: { label: string; value: boolean; onChange: (v: boolean) => void }) {
  return (
    <View style={[styles.row, styles.boolRow]}>
      <FieldLabel>{props.label}</FieldLabel>
      <Switch
        value={props.value}
        onValueChange={props.onChange}
        trackColor={{ true: colors.accent, false: colors.border }}
      />
    </View>
  );
}

export function EnumField(props: {
  label: string;
  value: string;
  options: { label: string; value: string }[];
  onChange: (v: string) => void;
}) {
  return (
    <View style={styles.row}>
      <FieldLabel>{props.label}</FieldLabel>
      <View style={styles.segments}>
        {props.options.map((o) => {
          const active = o.value === props.value;
          return (
            <Pressable
              key={o.value}
              onPress={() => props.onChange(o.value)}
              style={[styles.segment, active && styles.segmentActive]}
            >
              <Text style={[styles.segmentText, active && styles.segmentTextActive]}>{o.label}</Text>
            </Pressable>
          );
        })}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  row: { gap: space.xs, marginBottom: space.md },
  boolRow: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between' },
  label: { color: colors.textDim, fontSize: font.size.sm },
  input: {
    backgroundColor: colors.surfaceAlt,
    borderRadius: radius.sm,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: colors.border,
    color: colors.text,
    paddingHorizontal: space.md,
    paddingVertical: space.sm,
    fontFamily: font.mono,
    fontSize: font.size.md,
  },
  segments: { flexDirection: 'row', flexWrap: 'wrap', gap: space.xs },
  segment: {
    paddingHorizontal: space.md,
    paddingVertical: space.xs,
    borderRadius: radius.sm,
    backgroundColor: colors.surfaceAlt,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: colors.border,
  },
  segmentActive: { backgroundColor: colors.accent, borderColor: colors.accent },
  segmentText: { color: colors.textDim, fontSize: font.size.sm },
  segmentTextActive: { color: colors.accentText, fontWeight: '700' },
});
