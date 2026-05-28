import { useEffect } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import Animated, {
  useAnimatedStyle,
  useSharedValue,
  withRepeat,
  withTiming,
} from 'react-native-reanimated';
import type { ConnectionState } from 'react-native-ecr17';
import { colors, font, radius, space } from '../../ecr17/theme';

interface Props {
  state: ConnectionState;
  busy: boolean;
  onConnect: () => void;
  onDisconnect: () => void;
}

const dotColor: Record<ConnectionState, string> = {
  disconnected: colors.disconnected,
  connecting: colors.connecting,
  connected: colors.connected,
};

export function ConnectionBar({ state, busy, onConnect, onDisconnect }: Props) {
  const pulse = useSharedValue(1);

  useEffect(() => {
    if (state === 'connected' || state === 'connecting') {
      pulse.value = withRepeat(withTiming(0.3, { duration: 700 }), -1, true);
    } else {
      pulse.value = withTiming(1, { duration: 200 });
    }
  }, [state, pulse]);

  const dotStyle = useAnimatedStyle(() => ({ opacity: pulse.value }));
  const connected = state === 'connected';

  return (
    <View style={styles.bar}>
      <View style={styles.left}>
        <Animated.View style={[styles.dot, { backgroundColor: dotColor[state] }, dotStyle]} />
        <Text style={styles.state}>{state.toUpperCase()}</Text>
      </View>
      <Pressable
        accessibilityRole="button"
        disabled={busy}
        onPress={connected ? onDisconnect : onConnect}
        style={({ pressed }) => [
          styles.btn,
          { backgroundColor: connected ? colors.danger : colors.accent },
          (pressed || busy) && styles.btnPressed,
        ]}
      >
        <Text style={styles.btnText}>{connected ? 'Disconnect' : 'Connect'}</Text>
      </Pressable>
    </View>
  );
}

const styles = StyleSheet.create({
  bar: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: space.lg,
    paddingVertical: space.md,
    backgroundColor: colors.surface,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: colors.border,
  },
  left: { flexDirection: 'row', alignItems: 'center', gap: space.sm },
  dot: { width: 12, height: 12, borderRadius: 6 },
  state: { color: colors.text, fontFamily: font.mono, fontSize: font.size.sm, letterSpacing: 1 },
  btn: { paddingHorizontal: space.lg, paddingVertical: space.sm, borderRadius: radius.md },
  btnPressed: { opacity: 0.6 },
  btnText: { color: colors.accentText, fontWeight: '700', fontSize: font.size.sm },
});
