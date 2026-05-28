import { useState } from "react";
import { Pressable, StyleSheet, Text, View } from "react-native";
import type { Ecr17Config, LrcMode } from "react-native-ecr17";
import Animated, { FadeIn, FadeOut } from "react-native-reanimated";
import { colors, font, space } from "../../ecr17/theme";
import { BoolField, EnumField, NumberField, TextField } from "./fields";

interface Props {
	value: Ecr17Config;
	onChange: (next: Ecr17Config) => void;
}

const LRC_OPTIONS = [
	{ label: "std", value: "std" },
	{ label: "stx", value: "stx" },
	{ label: "noext", value: "noext" },
	{ label: "stx_noext", value: "stx_noext" },
];

export function ConfigForm({ value, onChange }: Props) {
	const [open, setOpen] = useState(true);
	const set = <K extends keyof Ecr17Config>(key: K, v: Ecr17Config[K]) =>
		onChange({ ...value, [key]: v });

	return (
		<View style={styles.card}>
			<Pressable onPress={() => setOpen((o) => !o)} style={styles.header}>
				<Text style={styles.title}>Configuration</Text>
				<Text style={styles.chevron}>{open ? "▾" : "▸"}</Text>
			</Pressable>

			{open && (
				<Animated.View entering={FadeIn} exiting={FadeOut} style={styles.body}>
					<TextField
						label="Host"
						onChange={(v) => set("host", v)}
						placeholder="192.168.1.50"
						value={value.host}
					/>
					<NumberField
						label="Port"
						onChange={(v) => set("port", v)}
						value={value.port ?? 10000}
					/>
					<TextField
						label="Terminal ID"
						onChange={(v) => set("terminalId", v)}
						value={value.terminalId}
					/>
					<TextField
						label="Cash register ID"
						onChange={(v) => set("cashRegisterId", v)}
						value={value.cashRegisterId}
					/>
					<EnumField
						label="LRC mode"
						onChange={(v) => set("lrcMode", v as LrcMode)}
						options={LRC_OPTIONS}
						value={value.lrcMode ?? "std"}
					/>
					<BoolField
						label="Keep alive"
						onChange={(v) => set("keepAlive", v)}
						value={value.keepAlive ?? false}
					/>
					<BoolField
						label="Auto reconnect"
						onChange={(v) => set("autoReconnect", v)}
						value={value.autoReconnect ?? false}
					/>
					<BoolField
						label="Debug"
						onChange={(v) => set("debug", v)}
						value={value.debug ?? false}
					/>
					<NumberField
						label="Connection timeout (ms)"
						onChange={(v) => set("connectionTimeoutMs", v)}
						value={value.connectionTimeoutMs ?? 5000}
					/>
					<NumberField
						label="Response timeout (ms)"
						onChange={(v) => set("responseTimeoutMs", v)}
						value={value.responseTimeoutMs ?? 60000}
					/>
					<NumberField
						label="ACK timeout (ms)"
						onChange={(v) => set("ackTimeoutMs", v)}
						value={value.ackTimeoutMs ?? 2000}
					/>
					<NumberField
						label="Retry count"
						onChange={(v) => set("retryCount", v)}
						value={value.retryCount ?? 3}
					/>
					<NumberField
						label="Retry delay (ms)"
						onChange={(v) => set("retryDelayMs", v)}
						value={value.retryDelayMs ?? 200}
					/>
					<NumberField
						label="Receipt drain (ms)"
						onChange={(v) => set("receiptDrainMs", v)}
						value={value.receiptDrainMs ?? 0}
					/>
				</Animated.View>
			)}
		</View>
	);
}

const styles = StyleSheet.create({
	card: {
		backgroundColor: colors.surface,
		borderBottomWidth: StyleSheet.hairlineWidth,
		borderBottomColor: colors.border,
	},
	header: {
		flexDirection: "row",
		alignItems: "center",
		justifyContent: "space-between",
		paddingHorizontal: space.lg,
		paddingVertical: space.md,
	},
	title: { color: colors.text, fontWeight: "700", fontSize: font.size.md },
	chevron: { color: colors.textDim, fontSize: font.size.md },
	body: { paddingHorizontal: space.lg, paddingBottom: space.lg },
});
