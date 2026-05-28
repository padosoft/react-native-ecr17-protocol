import { Pressable, StyleSheet, Text, View } from 'react-native';
import { COMMANDS, type CommandDef } from '../../ecr17/commands';
import { colors, font, radius, space } from '../../ecr17/theme';

interface Props {
  onPick: (cmd: CommandDef) => void;
  disabled: boolean;
}

export function CommandPalette({ onPick, disabled }: Props) {
  return (
    <View style={styles.wrap}>
      <Text style={styles.title}>Commands</Text>
      <View style={styles.grid}>
        {COMMANDS.map((cmd) => (
          <Pressable
            key={cmd.key}
            disabled={disabled}
            onPress={() => onPick(cmd)}
            style={({ pressed }) => [
              styles.btn,
              cmd.danger && styles.btnDanger,
              (pressed || disabled) && styles.btnPressed,
            ]}
          >
            <View style={styles.chip}>
              <Text style={styles.chipText}>{cmd.letter}</Text>
            </View>
            <Text style={styles.btnLabel} numberOfLines={1}>
              {cmd.label}
            </Text>
          </Pressable>
        ))}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { paddingHorizontal: space.lg, paddingVertical: space.md },
  title: { color: colors.text, fontWeight: '700', fontSize: font.size.md, marginBottom: space.sm },
  grid: { flexDirection: 'row', flexWrap: 'wrap', gap: space.sm },
  btn: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: space.xs,
    paddingHorizontal: space.md,
    paddingVertical: space.sm,
    borderRadius: radius.md,
    backgroundColor: colors.surfaceAlt,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: colors.border,
  },
  btnDanger: { borderColor: colors.danger },
  btnPressed: { opacity: 0.55 },
  chip: {
    width: 20,
    height: 20,
    borderRadius: radius.sm,
    backgroundColor: colors.bg,
    alignItems: 'center',
    justifyContent: 'center',
  },
  chipText: { color: colors.accent, fontFamily: font.mono, fontSize: font.size.xs, fontWeight: '700' },
  btnLabel: { color: colors.text, fontSize: font.size.sm },
});
