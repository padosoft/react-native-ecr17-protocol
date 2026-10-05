// A scripted ECR17 terminal on 127.0.0.1 for the Node tests: real TCP, real framing
// (STX payload ETX LRC, ACK/NAK + ETX + LRC, SOH progress + EOT), LRC mode "std".
import { createServer, type Server, type Socket } from "node:net";

const STX = 0x02;
const ETX = 0x03;
const SOH = 0x01;
const EOT = 0x04;
const ACK = 0x06;

export const TERMINAL_ID = "12345678";

function lrc(bytes: Buffer): number {
	let value = 0x7f;
	for (const byte of bytes) value ^= byte;
	return value;
}

export function applicationFrame(payload: string): Buffer {
	const body = Buffer.from(payload, "latin1");
	return Buffer.concat([
		Buffer.from([STX]),
		body,
		Buffer.from([ETX, lrc(body)]),
	]);
}

export function ackFrame(): Buffer {
	return Buffer.from([ACK, ETX, lrc(Buffer.from([ACK]))]);
}

export function progressFrame(message: string): Buffer {
	return Buffer.concat([
		Buffer.from([SOH]),
		Buffer.from(message.padEnd(20).slice(0, 20), "latin1"),
		Buffer.from([EOT]),
	]);
}

/** A payment-family result ('E'), laid out as Ecr17Response::parsePayment reads it. */
export function paymentResult(fields: {
	resultCode?: string;
	pan?: string;
	entry?: string;
	authCode?: string;
	hostDateTime?: string;
	cardType?: string;
	acquirerId?: string;
	stan?: string;
	onlineId?: string;
	errorDescription?: string;
}): string {
	const resultCode = fields.resultCode ?? "00";
	const head = `${TERMINAL_ID}0E${resultCode}`;
	const body =
		resultCode === "01"
			? (fields.errorDescription ?? "").padEnd(35)
			: (fields.pan ?? "").padEnd(19) +
				(fields.entry ?? "ICC").padEnd(3) +
				(fields.authCode ?? "").padEnd(6) +
				(fields.hostDateTime ?? "").padEnd(7);
	return (
		head +
		body +
		(fields.cardType ?? "2") +
		(fields.acquirerId ?? "").padEnd(11) +
		(fields.stan ?? "000000") +
		(fields.onlineId ?? "000000")
	);
}

/** A status ('s') response, laid out as Ecr17Response::parseStatus reads it. */
export function statusResult(
	dateTime: string,
	status: number,
	release: string,
): string {
	return `${TERMINAL_ID}0s${"0".repeat(10)}${dateTime}${status}${release}`;
}

export interface Exchange {
	/** The request payload (between STX and ETX). */
	request: string;
	/** Index of the TCP connection it arrived on (0 = first). */
	connection: number;
}

/** What the terminal does with a request: bytes to write, or close the socket. */
export type Reply = (
	request: string,
	socket: Socket,
	connection: number,
) => void | Promise<void>;

export class FakeTerminal {
	readonly requests: Exchange[] = [];
	readonly acksReceived: number[] = [];
	connections = 0;
	#server: Server;
	#replies: Reply[] = [];
	#sockets = new Set<Socket>();

	constructor() {
		this.#server = createServer((socket) => this.#accept(socket));
	}

	async listen(): Promise<number> {
		await new Promise<void>((resolve) =>
			this.#server.listen(0, "127.0.0.1", resolve),
		);
		const address = this.#server.address();
		if (address === null || typeof address === "string") {
			throw new Error("no port");
		}
		return address.port;
	}

	/** Queues the reply to the next application request. */
	reply(reply: Reply): this {
		this.#replies.push(reply);
		return this;
	}

	/** Closes every open connection (FIN), like a terminal between transactions. */
	dropConnections(): void {
		for (const socket of this.#sockets) socket.end();
	}

	async close(): Promise<void> {
		for (const socket of this.#sockets) socket.destroy();
		await new Promise<void>((resolve) => this.#server.close(() => resolve()));
	}

	#accept(socket: Socket): void {
		const connection = this.connections++;
		this.#sockets.add(socket);
		socket.on("close", () => this.#sockets.delete(socket));
		socket.on("error", () => {});
		let buffer = Buffer.alloc(0);
		socket.on("data", (chunk: Buffer) => {
			buffer = Buffer.concat([buffer, chunk]);
			for (;;) {
				if (buffer.length >= 3 && buffer[0] === ACK && buffer[1] === ETX) {
					this.acksReceived.push(connection);
					buffer = buffer.subarray(3);
					continue;
				}
				if (buffer[0] !== STX) break;
				const end = buffer.indexOf(ETX);
				if (end < 0 || buffer.length < end + 2) break;
				const request = buffer.subarray(1, end).toString("latin1");
				buffer = buffer.subarray(end + 2);
				this.requests.push({ request, connection });
				const reply = this.#replies.shift();
				void reply?.(request, socket, connection);
			}
		});
	}
}

/** ACK, optional progress messages, then the result frame. */
export function respond(payload: string, progress: string[] = []): Reply {
	return (_request, socket) => {
		socket.write(ackFrame());
		for (const message of progress) socket.write(progressFrame(message));
		socket.write(applicationFrame(payload));
	};
}

/** The command code of a request payload (byte 10, after the terminal id and '0'). */
export function commandOf(request: string): string {
	return request.charAt(9);
}
