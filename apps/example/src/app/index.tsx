import type { Ecr17Config } from "@padosoft/react-native-ecr17";
import { useCallback, useEffect, useState } from "react";
import { ScrollView, StyleSheet, Text } from "react-native";
import Animated, { FadeIn, FadeOut } from "react-native-reanimated";
import { SafeAreaView } from "react-native-safe-area-context";

import { BusyOverlay } from "@/components/ecr17/BusyOverlay";
import { CommandPalette } from "@/components/ecr17/CommandPalette";
import { CommandParamsSheet } from "@/components/ecr17/CommandParamsSheet";
import { ConfigForm } from "@/components/ecr17/ConfigForm";
import { ConnectionBar } from "@/components/ecr17/ConnectionBar";
import type { CommandDef } from "@/ecr17/commands";
import { DEFAULT_CONFIG, loadConfig, saveConfig } from "@/ecr17/storage";
import { colors, font, radius, space } from "@/ecr17/theme";
import { type RunResult, useEcr17 } from "@/ecr17/useEcr17";

interface Toast {
	text: string;
	color: string;
}

export default function DebugConsoleScreen() {
	const [config, setConfig] = useState<Ecr17Config | null>(null);
	const [sheetCmd, setSheetCmd] = useState<CommandDef | null>(null);
	const [toast, setToast] = useState<Toast | null>(null);

	useEffect(() => {
		loadConfig().then(setConfig);
	}, []);

	const { connectionState, busy, lastProgress, connect, disconnect, run } =
		useEcr17(config ?? DEFAULT_CONFIG);

	const onChangeConfig = useCallback((next: Ecr17Config) => {
		setConfig(next);
		saveConfig(next);
	}, []);

	const showToast = useCallback((r: RunResult) => {
		if (r.status === "error") {
			setToast({ text: "Error — see log", color: colors.error });
		} else if (r.status === "ko") {
			const res = r.result as
				| { outcome?: string; responseId?: string }
				| undefined;
			const label =
				res?.outcome ??
				(res?.responseId != null ? `vas ${res.responseId}` : "ko");
			setToast({ text: `KO (${label})`, color: colors.ko });
		} else {
			setToast({ text: "OK", color: colors.ok });
		}
		setTimeout(() => setToast(null), 2500);
	}, []);

	const doRun = useCallback(
		async (key: string, params: Record<string, unknown>) => {
			const result = await run(key, params);
			showToast(result);
		},
		[run, showToast],
	);

	const onPick = useCallback(
		(cmd: CommandDef) => {
			if (cmd.fields.length === 0) {
				void doRun(cmd.key, {});
			} else {
				setSheetCmd(cmd);
			}
		},
		[doRun],
	);

	if (!config) {
		return (
			<SafeAreaView edges={["top"]} style={styles.screen}>
				<Text style={styles.loading}>Loading…</Text>
			</SafeAreaView>
		);
	}

	return (
		<SafeAreaView edges={["top"]} style={styles.screen}>
			<Text style={styles.heading}>ECR17 Debug Console</Text>
			<ConnectionBar
				busy={busy}
				onConnect={() => void connect()}
				onDisconnect={disconnect}
				state={connectionState}
			/>
			<ScrollView keyboardShouldPersistTaps="handled" style={styles.body}>
				<ConfigForm onChange={onChangeConfig} value={config} />
				<CommandPalette disabled={busy} onPick={onPick} />
			</ScrollView>

			<CommandParamsSheet
				command={sheetCmd}
				onClose={() => setSheetCmd(null)}
				onSubmit={(key, params) => void doRun(key, params)}
			/>
			<BusyOverlay progress={lastProgress} visible={busy} />

			{toast && (
				<Animated.View
					entering={FadeIn}
					exiting={FadeOut}
					style={[styles.toast, { backgroundColor: toast.color }]}
				>
					<Text style={styles.toastText}>{toast.text}</Text>
				</Animated.View>
			)}
		</SafeAreaView>
	);
}

const styles = StyleSheet.create({
	screen: { flex: 1, backgroundColor: colors.bg },
	heading: {
		color: colors.text,
		fontWeight: "800",
		fontSize: font.size.lg,
		paddingHorizontal: space.lg,
		paddingTop: space.sm,
		paddingBottom: space.xs,
	},
	loading: { color: colors.textDim, textAlign: "center", marginTop: space.xl },
	body: { flex: 1 },
	toast: {
		position: "absolute",
		bottom: space.xl,
		alignSelf: "center",
		paddingHorizontal: space.xl,
		paddingVertical: space.md,
		borderRadius: radius.lg,
	},
	toastText: {
		color: colors.accentText,
		fontWeight: "700",
		fontSize: font.size.md,
	},
});
