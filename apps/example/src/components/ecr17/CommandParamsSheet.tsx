import { useEffect, useState } from "react";
import {
	KeyboardAvoidingView,
	Modal,
	Platform,
	Pressable,
	ScrollView,
	StyleSheet,
	Text,
	View,
} from "react-native";
import type { CommandDef, Field } from "../../ecr17/commands";
import { colors, font, radius, space } from "../../ecr17/theme";
import { BoolField, EnumField, MoneyField, TextField } from "./fields";

interface Props {
	command: CommandDef | null;
	onSubmit: (key: string, params: Record<string, unknown>) => void;
	onClose: () => void;
}

function initialValue(field: Field): unknown {
	switch (field.kind) {
		case "money":
		case "number":
			return 0;
		case "bool":
			return false;
		case "enum":
			return field.options?.[0]?.value ?? "";
		default:
			return "";
	}
}

export function CommandParamsSheet({ command, onSubmit, onClose }: Props) {
	const [params, setParams] = useState<Record<string, unknown>>({});

	useEffect(() => {
		if (command) {
			setParams(
				Object.fromEntries(
					command.fields.map((f) => [f.name, initialValue(f)]),
				),
			);
		}
	}, [command]);

	if (!command) {
		return null;
	}

	const set = (name: string, v: unknown) =>
		setParams((p) => ({ ...p, [name]: v }));
	const missingRequired = command.fields.some((f) => {
		if (!f.required) {
			return false;
		}
		const v = params[f.name];
		// A required amount must be a positive number — guard against submitting a
		// zero-amount financial transaction with the default 0.
		if (f.kind === "money" || f.kind === "number") {
			return typeof v !== "number" || v <= 0;
		}
		return v === "" || v === undefined;
	});

	return (
		<Modal
			allowSwipeDismissal
			animationType="slide"
			onRequestClose={onClose}
			transparent
			visible
		>
			<KeyboardAvoidingView
				behavior={Platform.OS === "ios" ? "padding" : "height"}
				style={{ flex: 1 }}
			>
				<View style={styles.backdrop}>
					<View style={styles.sheet}>
						<View style={styles.header}>
							<Text style={styles.title}>{command.label}</Text>
							<Pressable hitSlop={10} onPress={onClose}>
								<Text style={styles.close}>✕</Text>
							</Pressable>
						</View>

						<ScrollView
							keyboardShouldPersistTaps="handled"
							style={styles.fields}
						>
							{command.fields.map((f) => {
								if (f.kind === "money") {
									return (
										<MoneyField
											cents={(params[f.name] as number) ?? 0}
											key={f.name}
											label={f.label}
											onChange={(c) => set(f.name, c)}
										/>
									);
								}
								if (f.kind === "bool") {
									return (
										<BoolField
											key={f.name}
											label={f.label}
											onChange={(v) => set(f.name, v)}
											value={(params[f.name] as boolean) ?? false}
										/>
									);
								}
								if (f.kind === "enum") {
									return (
										<EnumField
											key={f.name}
											label={f.label}
											onChange={(v) => set(f.name, v)}
											options={f.options ?? []}
											value={(params[f.name] as string) ?? ""}
										/>
									);
								}
								return (
									<TextField
										key={f.name}
										label={f.label}
										onChange={(v) => set(f.name, v)}
										placeholder={f.placeholder}
										value={(params[f.name] as string) ?? ""}
									/>
								);
							})}
						</ScrollView>

						<Pressable
							disabled={missingRequired}
							onPress={() => {
								onSubmit(command.key, params);
								onClose();
							}}
							style={({ pressed }) => [
								styles.submit,
								command.danger && styles.submitDanger,
								(pressed || missingRequired) && styles.submitDisabled,
							]}
						>
							<Text style={styles.submitText}>
								{command.danger ? "Run (financial)" : "Run"}
							</Text>
						</Pressable>
					</View>
				</View>
			</KeyboardAvoidingView>
		</Modal>
	);
}

const styles = StyleSheet.create({
	backdrop: {
		flex: 1,
		backgroundColor: "#000000aa",
		justifyContent: "flex-end",
	},
	sheet: {
		backgroundColor: colors.surface,
		borderTopLeftRadius: radius.lg,
		borderTopRightRadius: radius.lg,
		padding: space.lg,
		maxHeight: "80%",
	},
	header: {
		flexDirection: "row",
		alignItems: "center",
		justifyContent: "space-between",
		marginBottom: space.md,
	},
	title: { color: colors.text, fontWeight: "700", fontSize: font.size.lg },
	close: { color: colors.textDim, fontSize: font.size.lg },
	fields: { flexGrow: 0 },
	submit: {
		marginTop: space.md,
		backgroundColor: colors.accent,
		borderRadius: radius.md,
		paddingVertical: space.md,
		alignItems: "center",
	},
	submitDanger: { backgroundColor: colors.danger },
	submitDisabled: { opacity: 0.5 },
	submitText: {
		color: colors.accentText,
		fontWeight: "700",
		fontSize: font.size.md,
	},
});
