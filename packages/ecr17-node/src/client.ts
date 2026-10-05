import {
	mapCardVerification,
	mapClose,
	mapPayment,
	mapPreAuth,
	mapReversal,
	mapStatus,
	mapTotals,
	mapVas,
} from "./mappers.ts";
import {
	loadNativeClient,
	type NativeClient,
	type NativeFollowUpRequest,
	type NativePaymentRequest,
} from "./native.ts";
import type {
	CardVerificationRequest,
	CardVerificationResult,
	CloseSessionResult,
	ConnectionState,
	Ecr17Config,
	IncrementalAuthRequest,
	PaymentRequest,
	PaymentResult,
	PosStatusResponse,
	PreAuthClosureRequest,
	PreAuthRequest,
	PreAuthResult,
	ProgressEvent,
	ReceiptLine,
	ReversalRequest,
	ReversalResult,
	TotalsResult,
	VasResult,
} from "./types.ts";

/**
 * An ECR17 (Nexi) terminal client for Node.js: the same API as
 * `@padosoft/react-native-ecr17`, on the same C++ core.
 *
 * Every command auto-connects (probing the socket first: terminals close TCP between
 * transactions), runs one exchange and resolves with the parsed response. Commands run
 * in order on a native worker thread, never on the event loop.
 *
 * ⚠️ Money safety: with `autoReconnect`, a command interrupted by a drop is re-sent only
 * if it is read-only. A financial command is never re-sent (the terminal may already have
 * charged the card): recover its outcome with `sendLastResult()`.
 */
export class Ecr17Client {
	#config: Ecr17Config | undefined;
	#native: NativeClient | undefined;
	#onProgress: ((event: ProgressEvent) => void) | undefined;
	#onReceiptLine: ((line: ReceiptLine) => void) | undefined;
	#onConnectionStateChange: ((state: ConnectionState) => void) | undefined;

	constructor(config?: Ecr17Config) {
		if (config) this.configure(config);
	}

	// --- Configuration ---

	/**
	 * Sets the terminal configuration. A previous connection is closed; its queued
	 * commands are rejected.
	 */
	configure(config: Ecr17Config): void {
		requireString(config.host, "host");
		requireString(config.terminalId, "terminalId");
		requireString(config.cashRegisterId, "cashRegisterId");
		const NativeClientClass = loadNativeClient();
		const next = new NativeClientClass({ ...config });
		this.#native?.close();
		this.#config = { ...config };
		this.#native = next;
		this.#attachListeners();
	}

	configuration(): Ecr17Config {
		return { ...this.#requireConfig() };
	}

	// --- Connection ---

	connect(): Promise<void> {
		return this.#run((n) => n.connect());
	}

	disconnect(): void {
		this.#native?.disconnect();
	}

	isConnected(): boolean {
		return this.#native?.isConnected() ?? false;
	}

	/**
	 * Closes the connection and releases the native client. Queued commands are rejected.
	 * A command already sent fails like a dropped connection: a financial one is never
	 * re-sent, so recover its outcome with `sendLastResult()` on a new client.
	 */
	close(): void {
		this.#native?.close();
		this.#native = undefined;
	}

	[Symbol.dispose](): void {
		this.close();
	}

	// --- Commands ---

	async status(): Promise<PosStatusResponse> {
		return mapStatus(await this.#run((n) => n.status()));
	}

	async pay(request: PaymentRequest): Promise<PaymentResult> {
		return mapPayment(await this.#run((n) => n.pay(paymentRequest(request))));
	}

	async payExtended(request: PaymentRequest): Promise<PaymentResult> {
		return mapPayment(
			await this.#run((n) => n.payExtended(paymentRequest(request))),
		);
	}

	async reverse(request: ReversalRequest = {}): Promise<ReversalResult> {
		return mapReversal(
			await this.#run((n) =>
				n.reverse({
					...optional("cashRegisterId", request.cashRegisterId),
					stan: request.stan ?? "000000",
				}),
			),
		);
	}

	async preAuth(request: PreAuthRequest): Promise<PreAuthResult> {
		return mapPreAuth(
			await this.#run((n) => n.preAuth(paymentRequest(request))),
		);
	}

	async incrementalAuth(
		request: IncrementalAuthRequest,
	): Promise<PreAuthResult> {
		return mapPreAuth(
			await this.#run((n) => n.incrementalAuth(followUpRequest(request))),
		);
	}

	async preAuthClosure(request: PreAuthClosureRequest): Promise<PaymentResult> {
		return mapPayment(
			await this.#run((n) => n.preAuthClosure(followUpRequest(request))),
		);
	}

	async verifyCard(
		request: CardVerificationRequest = {},
	): Promise<CardVerificationResult> {
		return mapCardVerification(
			await this.#run((n) =>
				n.verifyCard({
					...optional("cashRegisterId", request.cashRegisterId),
					paymentType: request.paymentType ?? "auto",
					...optional("tokenization", request.tokenization),
				}),
			),
		);
	}

	async closeSession(): Promise<CloseSessionResult> {
		return mapClose(await this.#run((n) => n.closeSession()));
	}

	async totals(): Promise<TotalsResult> {
		return mapTotals(await this.#run((n) => n.totals()));
	}

	/** Recovers the outcome of the last transaction (command `G`), e.g. after a drop. */
	async sendLastResult(): Promise<PaymentResult> {
		return mapPayment(await this.#run((n) => n.sendLastResult()));
	}

	enableEcrPrinting(enabled: boolean): Promise<void> {
		return this.#run((n) => n.enableEcrPrinting(enabled));
	}

	reprint(toEcr: boolean): Promise<void> {
		return this.#run((n) => n.reprint(toEcr));
	}

	async vas(xmlRequest: string): Promise<VasResult> {
		return mapVas(await this.#run((n) => n.vas(xmlRequest)));
	}

	// --- Events (delivered on the event loop) ---

	setOnProgress(callback: (event: ProgressEvent) => void): void {
		this.#onProgress = callback;
		this.#attachListeners();
	}

	setOnReceiptLine(callback: (line: ReceiptLine) => void): void {
		this.#onReceiptLine = callback;
		this.#attachListeners();
	}

	setOnConnectionStateChange(callback: (state: ConnectionState) => void): void {
		this.#onConnectionStateChange = callback;
		this.#attachListeners();
	}

	#attachListeners(): void {
		const native = this.#native;
		if (!native) return;
		const onProgress = this.#onProgress;
		const onReceiptLine = this.#onReceiptLine;
		const onState = this.#onConnectionStateChange;
		native.setOnProgress(
			onProgress && surfacing((message: string) => onProgress({ message })),
		);
		native.setOnReceiptLine(
			onReceiptLine && surfacing((text: string) => onReceiptLine({ text })),
		);
		native.setOnConnectionStateChange(onState && surfacing(onState));
	}

	#requireConfig(): Ecr17Config {
		if (!this.#config) {
			throw new Error("ECR17: call configure() first");
		}
		return this.#config;
	}

	// Runs a command on the native client. Validation errors thrown synchronously by
	// the addon become a rejected promise, like every other failure.
	async #run<T>(command: (native: NativeClient) => Promise<T>): Promise<T> {
		this.#requireConfig();
		const native = this.#native;
		if (!native) {
			throw new Error("ECR17: the client is closed");
		}
		return command(native);
	}
}

/** Creates a configured client: the same entry point as on React Native. */
export function createEcr17Client(config: Ecr17Config): Ecr17Client {
	return new Ecr17Client(config);
}

// A listener that throws surfaces as an uncaught exception, like an EventEmitter
// listener's would. (The addon calls listeners from a thread-safe function, where Node
// would drop the exception silently.)
function surfacing<A>(listener: (arg: A) => void): (arg: A) => void {
	return (arg) => {
		try {
			listener(arg);
		} catch (error) {
			process.nextTick(() => {
				throw error;
			});
		}
	};
}

function requireString(value: unknown, field: string): void {
	if (typeof value !== "string" || value === "") {
		throw new TypeError(`ECR17: ${field} is required`);
	}
}

// Includes `key` only when the value is set (the addon treats a missing field as unset).
function optional<K extends string, V>(
	key: K,
	value: V | undefined,
): { [P in K]?: V } {
	return (value === undefined ? {} : { [key]: value }) as { [P in K]?: V };
}

function paymentRequest(
	request: PaymentRequest | PreAuthRequest,
): NativePaymentRequest {
	return {
		amountCents: request.amountCents,
		...optional("cashRegisterId", request.cashRegisterId),
		paymentType: request.paymentType ?? "auto",
		cardAlreadyPresent: request.cardAlreadyPresent ?? false,
		receiptText: request.receiptText ?? "",
		...optional("tokenization", request.tokenization),
	};
}

function followUpRequest(
	request: IncrementalAuthRequest | PreAuthClosureRequest,
): NativeFollowUpRequest {
	return {
		amountCents: request.amountCents,
		originalPreAuthCode: request.originalPreAuthCode,
		...optional("cashRegisterId", request.cashRegisterId),
		receiptText: request.receiptText ?? "",
	};
}
