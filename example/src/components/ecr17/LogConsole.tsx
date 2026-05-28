import { useEffect, useState } from 'react';
import { FlatList, Pressable, StyleSheet, Text, View } from 'react-native';
import Animated, { FadeIn } from 'react-native-reanimated';
import { clear, exportShare, subscribe, type LogEntry } from '../../ecr17/logger';
import { colors, font, radius, space } from '../../ecr17/theme';
import { logLevelColor } from '../../ecr17/theme';

function hhmmss(ts: number): string {
  const d = new Date(ts);
  const p = (n: number) => String(n).padStart(2, '0');
  return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

function Row({ entry }: { entry: LogEntry }) {
  const color = logLevelColor[entry.level];
  return (
    <Animated.View entering={FadeIn.duration(180)} style={styles.row}>
      <Text style={styles.time}>{hhmmss(entry.ts)}</Text>
      <Text style={[styles.level, { color }]}>{entry.level}</Text>
      <View style={styles.msg}>
        <Text style={[styles.label, { color }]}>{entry.label}</Text>
        {entry.detail ? <Text style={styles.detail}>{entry.detail}</Text> : null}
      </View>
    </Animated.View>
  );
}

export function LogConsole() {
  const [entries, setEntries] = useState<LogEntry[]>([]);

  useEffect(() => subscribe(setEntries), []);

  // Newest first.
  const data = entries.slice().reverse();

  return (
    <View style={styles.wrap}>
      <View style={styles.toolbar}>
        <Text style={styles.title}>Log ({entries.length})</Text>
        <View style={styles.actions}>
          <Pressable onPress={() => exportShare()} style={styles.action}>
            <Text style={styles.actionText}>Export</Text>
          </Pressable>
          <Pressable onPress={() => clear()} style={styles.action}>
            <Text style={styles.actionText}>Clear</Text>
          </Pressable>
        </View>
      </View>
      <FlatList
        data={data}
        keyExtractor={(e) => e.id}
        renderItem={({ item }) => <Row entry={item} />}
        style={styles.list}
        contentContainerStyle={styles.listContent}
        ListEmptyComponent={<Text style={styles.empty}>No activity yet. Connect and run a command.</Text>}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { flex: 1, backgroundColor: colors.bg },
  toolbar: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: space.lg,
    paddingVertical: space.sm,
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: colors.border,
  },
  title: { color: colors.text, fontWeight: '700', fontSize: font.size.sm },
  actions: { flexDirection: 'row', gap: space.sm },
  action: { paddingHorizontal: space.md, paddingVertical: space.xs, borderRadius: radius.sm, backgroundColor: colors.surfaceAlt },
  actionText: { color: colors.text, fontSize: font.size.sm },
  list: { flex: 1 },
  listContent: { paddingHorizontal: space.lg, paddingBottom: space.lg },
  empty: { color: colors.textDim, fontSize: font.size.sm, textAlign: 'center', marginTop: space.xl },
  row: { flexDirection: 'row', gap: space.sm, paddingVertical: space.xs, alignItems: 'flex-start' },
  time: { color: colors.textDim, fontFamily: font.mono, fontSize: font.size.xs, width: 58 },
  level: { fontFamily: font.mono, fontSize: font.size.xs, width: 56 },
  msg: { flex: 1 },
  label: { fontFamily: font.mono, fontSize: font.size.sm },
  detail: { color: colors.textDim, fontFamily: font.mono, fontSize: font.size.xs },
});
