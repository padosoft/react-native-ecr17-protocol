// Loads the Node-API addon (node/addon.cpp) and describes what it returns: the core's raw
// response structs. src/mappers.ts turns those into the public result types.
import { existsSync } from "node:fs";
import { createRequire } from "node:module";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

import type {
	ConnectionState,
	PaymentCardType,
	TokenizationRequest,
} from "./types.ts";

export type RawOutcome =
	| "ok"
	| "ko"
	| "cardNotPresent"
	| "unknownTag"
	| "unknown";

export interface RawPayment {
	outcome: RawOutcome;
	resultCode: string;
	pan: string;
	transactionType: string;
	authCode: string;
	hostDateTime: string;
	errorDescription: string;
	cardType: string;
	acquirerId: string;
	stan: string;
	onlineId: string;
	currency: {
		applied: boolean;
		rate: string;
		currencyCode: string;
		amount: string;
		precision: string;
	};
}

export interface RawPreAuth {
	outcome: RawOutcome;
	resultCode: string;
	pan: string;
	transactionType: string;
	authCode: string;
	preAuthorizedAmount: string;
	preAuthCode: string;
	actionCode: string;
	hostDateTime: string;
	errorDescription: string;
	cardType: string;
	acquirerId: string;
	stan: string;
	onlineId: string;
}

export interface RawStatus {
	terminalId: string;
	dateTimeRaw: string;
	status: number;
	softwareRelease: string;
}

export interface RawTotals {
	outcome: RawOutcome;
	resultCode: string;
	posTotal: string;
}

export interface RawClose {
	outcome: RawOutcome;
	resultCode: string;
	posTotal: string;
	hostTotal: string;
	errorDescription: string;
	actionCode: string;
}

export interface RawVas {
	responseId: string;
	responseMessage: string;
	orderId: string;
	rawXml: string;
}

// Requests as the addon reads them (src/client.ts fills the defaults).
export interface NativePaymentRequest {
	amountCents: number;
	cashRegisterId?: string;
	paymentType: PaymentCardType;
	cardAlreadyPresent: boolean;
	receiptText: string;
	tokenization?: TokenizationRequest;
}

export interface NativeFollowUpRequest {
	amountCents: number;
	originalPreAuthCode: string;
	cashRegisterId?: string;
	receiptText: string;
}

export interface NativeClient {
	connect(): Promise<void>;
	disconnect(): void;
	isConnected(): boolean;
	status(): Promise<RawStatus>;
	pay(request: NativePaymentRequest): Promise<RawPayment>;
	payExtended(request: NativePaymentRequest): Promise<RawPayment>;
	reverse(request: {
		cashRegisterId?: string;
		stan: string;
	}): Promise<RawPayment>;
	preAuth(request: NativePaymentRequest): Promise<RawPreAuth>;
	incrementalAuth(request: NativeFollowUpRequest): Promise<RawPreAuth>;
	preAuthClosure(request: NativeFollowUpRequest): Promise<RawPayment>;
	verifyCard(request: {
		cashRegisterId?: string;
		paymentType: PaymentCardType;
		tokenization?: TokenizationRequest;
	}): Promise<RawPayment>;
	closeSession(): Promise<RawClose>;
	totals(): Promise<RawTotals>;
	sendLastResult(): Promise<RawPayment>;
	enableEcrPrinting(enabled: boolean): Promise<void>;
	reprint(toEcr: boolean): Promise<void>;
	vas(xmlRequest: string): Promise<RawVas>;
	setOnProgress(callback: ((message: string) => void) | undefined): void;
	setOnReceiptLine(callback: ((line: string) => void) | undefined): void;
	setOnConnectionStateChange(
		callback: ((state: ConnectionState) => void) | undefined,
	): void;
	close(): void;
}

export interface NativeClientConstructor {
	new (config: object): NativeClient;
}

// Both dist/ (published) and src/ (tests) sit one level below the package root.
const packageRoot = fileURLToPath(new URL("..", import.meta.url));

/** Where the addon is looked for, in order. */
export function addonCandidates(): string[] {
	const platform = `${process.platform}-${process.arch}`;
	return [
		// biome-ignore lint/complexity/useLiteralKeys: tsconfig has noPropertyAccessFromIndexSignature
		process.env["ECR17_NODE_ADDON"],
		join(packageRoot, "build", "Release", "ecr17.node"), // `bun run build:node`
		join(packageRoot, "prebuilds", platform, "ecr17.node"), // shipped in the npm package
	].filter((path): path is string => Boolean(path));
}

let cached: NativeClientConstructor | undefined;

export function loadNativeClient(): NativeClientConstructor {
	if (cached) return cached;
	const candidates = addonCandidates();
	const path = candidates.find((candidate) => existsSync(candidate));
	if (!path) {
		throw new Error(
			`@padosoft/ecr17-node: no native addon for ${process.platform}-${process.arch}. ` +
				`Looked in:\n  ${candidates.join("\n  ")}\n` +
				"Build it from source with `npx cmake-js compile --directory node --out build` " +
				"in the package folder (needs CMake and a C++20 compiler).",
		);
	}
	const require = createRequire(import.meta.url);
	cached = (require(path) as { NativeClient: NativeClientConstructor })
		.NativeClient;
	return cached;
}
