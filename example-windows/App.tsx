import {
	type ConnectionState,
	createEcr17Client,
	type Ecr17Client,
} from "@padosoft/react-native-ecr17";
import { useCallback, useRef, useState } from "react";
import {
	Button,
	ScrollView,
	StyleSheet,
	Text,
	TextInput,
	View,
} from "react-native";

// Minimal ECR17 console for React Native Windows: connect to a terminal on the
// LAN, run read-only commands, and (deliberately) one small payment.
//
// Money-safety: a payment is NEVER retried by this app or the library after a
// connection drop. If a payment result is lost, use "Last result" (command G)
// to read the terminal's outcome instead of paying again.

type LogLine = { id: number; text: string };

export default function App() {
	const [host, setHost] = useState("192.168.1.100");
	const [port, setPort] = useState("10000");
	const [terminalId, setTerminalId] = useState("00000000");
	const [cashRegisterId, setCashRegisterId] = useState("00000001");
	const [amount, setAmount] = useState("1");
	const [state, setState] = useState<ConnectionState>("disconnected");
	const [busy, setBusy] = useState(false);
	const [log, setLog] = useState<LogLine[]>([]);
	const clientRef = useRef<Ecr17Client | null>(null);
	const nextId = useRef(0);

	const append = useCallback((text: string) => {
		nextId.current += 1;
		const line = {
			id: nextId.current,
			text: `${new Date().toLocaleTimeString()}  ${text}`,
		};
		setLog((lines) => [line, ...lines].slice(0, 200));
	}, []);

	const client = useCallback((): Ecr17Client => {
		if (clientRef.current == null) {
			const created = createEcr17Client({
				host,
				port: Number(port),
				terminalId,
				cashRegisterId,
				autoReconnect: true,
			});
			created.setOnConnectionStateChange(setState);
			created.setOnProgress((event) => append(`progress: ${event.message}`));
			created.setOnReceiptLine((line) => append(`receipt: ${line.text}`));
			clientRef.current = created;
		}
		return clientRef.current;
	}, [append, cashRegisterId, host, port, terminalId]);

	const run = useCallback(
		async (label: string, action: (c: Ecr17Client) => Promise<unknown>) => {
			setBusy(true);
			append(`${label}...`);
			try {
				const result = await action(client());
				append(
					`${label}: ${result === undefined ? "ok" : JSON.stringify(result)}`,
				);
			} catch (error) {
				append(
					`${label} failed: ${error instanceof Error ? error.message : String(error)}`,
				);
			} finally {
				setBusy(false);
			}
		},
		[append, client],
	);

	const applyConfig = useCallback(() => {
		clientRef.current?.disconnect();
		clientRef.current = null;
		setState("disconnected");
		append(`config: ${host}:${port}`);
	}, [append, host, port]);

	const amountCents = Math.round(Number(amount.replace(",", ".")) * 100);

	return (
		<View style={styles.root}>
			<Text style={styles.title}>ECR17 on Windows</Text>
			<Text style={styles.state}>Connection: {state}</Text>

			<View style={styles.row}>
				<Field label="Host" onChangeText={setHost} value={host} />
				<Field label="Port" onChangeText={setPort} value={port} />
				<Field
					label="Terminal ID"
					onChangeText={setTerminalId}
					value={terminalId}
				/>
				<Field
					label="Cash register ID"
					onChangeText={setCashRegisterId}
					value={cashRegisterId}
				/>
			</View>

			<View style={styles.row}>
				<Button disabled={busy} onPress={applyConfig} title="Apply config" />
				<Button
					disabled={busy}
					onPress={() => run("connect", (c) => c.connect())}
					title="Connect"
				/>
				<Button
					disabled={busy}
					onPress={() => clientRef.current?.disconnect()}
					title="Disconnect"
				/>
				<Button
					disabled={busy}
					onPress={() => run("status", (c) => c.status())}
					title="Status"
				/>
				<Button
					disabled={busy}
					onPress={() => run("totals", (c) => c.totals())}
					title="Totals"
				/>
				<Button
					disabled={busy}
					onPress={() => run("last result", (c) => c.sendLastResult())}
					title="Last result (G)"
				/>
			</View>

			<View style={styles.row}>
				<Field label="Amount (EUR)" onChangeText={setAmount} value={amount} />
				<Button
					disabled={busy || !Number.isFinite(amountCents) || amountCents <= 0}
					onPress={() => run("pay", (c) => c.pay({ amountCents }))}
					title={`Pay ${Number.isFinite(amountCents) ? (amountCents / 100).toFixed(2) : "?"} EUR`}
				/>
			</View>

			<ScrollView style={styles.log}>
				{log.map((line) => (
					<Text key={line.id} style={styles.logLine}>
						{line.text}
					</Text>
				))}
			</ScrollView>
		</View>
	);
}

function Field(props: {
	label: string;
	value: string;
	onChangeText: (text: string) => void;
}) {
	return (
		<View style={styles.field}>
			<Text style={styles.label}>{props.label}</Text>
			<TextInput
				onChangeText={props.onChangeText}
				style={styles.input}
				value={props.value}
			/>
		</View>
	);
}

const styles = StyleSheet.create({
	root: { flex: 1, padding: 24, gap: 12 },
	title: { fontSize: 24, fontWeight: "600" },
	state: { fontSize: 16 },
	row: {
		flexDirection: "row",
		flexWrap: "wrap",
		alignItems: "flex-end",
		gap: 8,
	},
	field: { minWidth: 160 },
	label: { fontSize: 12, opacity: 0.7 },
	input: {
		borderWidth: 1,
		borderColor: "#999",
		borderRadius: 4,
		paddingHorizontal: 8,
		paddingVertical: 4,
	},
	log: {
		flex: 1,
		borderWidth: 1,
		borderColor: "#ccc",
		borderRadius: 4,
		padding: 8,
	},
	logLine: { fontFamily: "Consolas", fontSize: 12 },
});
