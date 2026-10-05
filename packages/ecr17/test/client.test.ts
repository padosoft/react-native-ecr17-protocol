// The Node.js API end to end: TypeScript -> Node-API addon -> C++ core -> PosixTransport
// (WinsockTransport on Windows) -> TCP -> a scripted fake terminal.
import assert from "node:assert/strict";
import { after, afterEach, beforeEach, describe, test } from "node:test";

import {
	type ConnectionState,
	createEcr17Client,
	type Ecr17Client,
	type Ecr17Config,
	type ProgressEvent,
} from "../src/index.ts";
import {
	ackFrame,
	commandOf,
	FakeTerminal,
	paymentResult,
	respond,
	statusResult,
	TERMINAL_ID,
} from "./fake-terminal.ts";

let terminal: FakeTerminal;
let client: Ecr17Client;
const clients: Ecr17Client[] = [];

async function setup(overrides: Partial<Ecr17Config> = {}): Promise<void> {
	terminal = new FakeTerminal();
	const port = await terminal.listen();
	client = createEcr17Client({
		host: "127.0.0.1",
		port,
		terminalId: TERMINAL_ID,
		cashRegisterId: "00000001",
		ackTimeoutMs: 500,
		responseTimeoutMs: 2000,
		retryCount: 1,
		retryDelayMs: 10,
		...overrides,
	});
	clients.push(client);
}

function payments(): number {
	return terminal.requests.filter((r) => commandOf(r.request) === "P").length;
}

describe("Ecr17Client (Node.js)", () => {
	beforeEach(() => setup());
	afterEach(async () => {
		client.close();
		await terminal.close();
	});
	after(() => {
		for (const c of clients) c.close();
	});

	test("status: maps the terminal's status response", async () => {
		terminal.reply(respond(statusResult("0510261430", 2, "SW1.2.3   ")));

		const status = await client.status();

		assert.equal(status.terminalId, TERMINAL_ID);
		assert.equal(status.status, 2);
		assert.equal(status.softwareRelease, "SW1.2.3");
		assert.deepEqual(status.terminalDateTime, new Date(2026, 9, 5, 14, 30));
		assert.equal(commandOf(terminal.requests[0]?.request ?? ""), "s");
	});

	test("pay: sends the amount, streams progress, ACKs and maps the result", async () => {
		const progress: ProgressEvent[] = [];
		const states: ConnectionState[] = [];
		client.setOnProgress((event) => progress.push(event));
		client.setOnConnectionStateChange((state) => states.push(state));
		terminal.reply(
			respond(
				paymentResult({
					pan: "***************1234",
					entry: "CLI",
					authCode: "A1B2C3",
					hostDateTime: "2781430",
					cardType: "2",
					acquirerId: "ACQ01",
					stan: "000123",
					onlineId: "000456",
				}),
				["ATTENDERE PREGO"],
			),
		);

		const result = await client.pay({ amountCents: 650 });

		assert.deepEqual(result, {
			outcome: "ok",
			resultCode: "00",
			pan: "***************1234",
			entryMode: "clessIcc",
			authCode: "A1B2C3",
			hostDateTime: "2781430",
			cardType: "credit",
			acquirerId: "ACQ01",
			stan: "000123",
			onlineId: "000456",
		});
		const request = terminal.requests[0]?.request ?? "";
		assert.equal(commandOf(request), "P");
		assert.equal(request.substring(23, 31), "00000650");
		// Checked right after the await: a command's events arrive BEFORE its result
		// (one ordered queue per client in the addon).
		assert.deepEqual(progress, [{ message: "ATTENDERE PREGO     " }]);
		assert.deepEqual(states, ["connecting", "connected"]);
		await waitFor(() => terminal.acksReceived.length === 1);
	});

	test("a declined payment carries the error description", async () => {
		terminal.reply(
			respond(
				paymentResult({
					resultCode: "01",
					errorDescription: "CARTA RIFIUTATA",
				}),
			),
		);

		const result = await client.pay({ amountCents: 100 });

		assert.equal(result.outcome, "ko");
		assert.equal(result.errorDescription, "CARTA RIFIUTATA");
		assert.equal(result.pan, undefined);
		assert.ok(!("pan" in result), "optional fields are omitted, not undefined");
	});

	test("reconnects BEFORE sending when the terminal closed the idle socket", async () => {
		terminal.reply(respond(statusResult("0510261430", 2, "SW")));
		await client.status();
		terminal.dropConnections();
		await waitFor(() => !client.isConnected());

		terminal.reply(respond(paymentResult({ pan: "4111" })));
		const result = await client.pay({ amountCents: 650 });

		assert.equal(result.outcome, "ok");
		assert.equal(payments(), 1);
		assert.equal(terminal.requests.at(-1)?.connection, 1);
	});

	test("💰 a payment interrupted by a drop is NEVER re-sent, and sendLastResult recovers it", async () => {
		client.close();
		await terminal.close();
		await setup({ autoReconnect: true });

		// The terminal accepts the payment, then the connection dies before the result.
		terminal.reply((_request, socket) => {
			socket.write(ackFrame());
			setTimeout(() => socket.destroy(), 50);
		});

		await assert.rejects(client.pay({ amountCents: 650 }));
		assert.equal(payments(), 1, "the payment must go out exactly once");

		terminal.reply(respond(paymentResult({ pan: "4111", stan: "000777" })));
		const recovered = await client.sendLastResult();

		assert.equal(recovered.stan, "000777");
		assert.equal(payments(), 1);
		assert.equal(commandOf(terminal.requests.at(-1)?.request ?? ""), "G");
	});

	test("invalid input rejects with a TypeError before anything is sent", async () => {
		await assert.rejects(client.pay({ amountCents: Number.NaN }), TypeError);
		await assert.rejects(
			client.pay({
				amountCents: 1,
				tokenization: { service: "nope" as "recurring", contractCode: "x" },
			}),
			TypeError,
		);
		assert.equal(terminal.requests.length, 0);
		assert.equal(terminal.connections, 0);
	});

	test("configure() validates the required fields", () => {
		assert.throws(
			() =>
				createEcr17Client({ host: "", terminalId: "1", cashRegisterId: "1" }),
			TypeError,
		);
	});

	test("a refused connection rejects and reports disconnected", async () => {
		const closed = new FakeTerminal();
		const closedPort = await closed.listen();
		await closed.close();
		const states: ConnectionState[] = [];
		client.configure({ ...client.configuration(), port: closedPort });
		client.setOnConnectionStateChange((state) => states.push(state));

		await assert.rejects(client.status(), /connect to 127\.0\.0\.1/);
		await waitFor(() => states.length === 2);
		assert.deepEqual(states, ["connecting", "disconnected"]);
	});

	test("close() rejects queued commands and later ones", async () => {
		terminal.reply(() => {
			// never answers: the first command waits for its ACK
		});
		const first = client.status();
		const queued = client.totals();
		await waitFor(() => terminal.requests.length === 1);

		client.close();

		await assert.rejects(queued, /closed/);
		await assert.rejects(first);
		await assert.rejects(client.status(), /closed/);
		assert.equal(client.isConnected(), false);
	});
});

async function waitFor(
	condition: () => boolean,
	timeoutMs = 3000,
): Promise<void> {
	const deadline = Date.now() + timeoutMs;
	while (!condition()) {
		if (Date.now() > deadline) throw new Error("timed out waiting");
		await new Promise((resolve) => setTimeout(resolve, 10));
	}
}
