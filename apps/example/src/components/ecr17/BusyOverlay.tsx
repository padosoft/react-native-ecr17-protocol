import { ActivityIndicator, StyleSheet, Text, View } from 'react-native';
import Animated, { FadeIn, FadeOut } from 'react-native-reanimated';
import { colors, font, radius, space } from '../../ecr17/theme';

interface Props {
  visible: boolean;
  progress: string;
}

export function BusyOverlay({ visible, progress }: Props) {
  if (!visible) {
    return null;
  }
  return (
    <Animated.View entering={FadeIn.duration(150)} exiting={FadeOut.duration(150)} style={styles.overlay}>
      <View style={styles.card}>
        <ActivityIndicator size="large" color={colors.accent} />
        <Text style={styles.text}>{progress || 'Working…'}</Text>
      </View>
    </Animated.View>
  );
}

const styles = StyleSheet.create({
  overlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: '#000000bb',
    alignItems: 'center',
    justifyContent: 'center',
  },
  card: {
    backgroundColor: colors.surface,
    borderRadius: radius.lg,
    paddingHorizontal: space.xl,
    paddingVertical: space.xl,
    alignItems: 'center',
    gap: space.md,
    minWidth: 200,
  },
  text: { color: colors.text, fontSize: font.size.md, textAlign: 'center' },
});
