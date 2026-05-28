import { StyleSheet, Text } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';

import { LogConsole } from '@/components/ecr17/LogConsole';
import { colors, font, space } from '@/ecr17/theme';

// Second tab: the live ECR17 log console (moved off the Home screen so Home is a
// single scroll of config + commands, not a split screen).
export default function LogsScreen() {
  return (
    <SafeAreaView style={styles.screen} edges={['top']}>
      <Text style={styles.heading}>ECR17 Logs</Text>
      <LogConsole />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  screen: { flex: 1, backgroundColor: colors.bg },
  heading: {
    color: colors.text,
    fontWeight: '800',
    fontSize: font.size.lg,
    paddingHorizontal: space.lg,
    paddingVertical: space.sm,
  },
});
