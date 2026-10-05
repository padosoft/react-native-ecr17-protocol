import type { HybridObject } from "react-native-nitro-modules";
import type {
	CardVerificationRequest,
	CardVerificationResult,
	CloseSessionResult,
	ConnectionState,
	IncrementalAuthRequest,
	LrcMode,
	PaymentRequest,
	PaymentResult,
	PreAuthClosureRequest,
	PreAuthRequest,
	PreAuthResult,
	ProgressEvent,
	ReceiptLine,
	ReversalRequest,
	ReversalResult,
	TotalsResult,
	VasResult,
} from "../types/client";

export interface Ecr17Config {
	host: string;
	port?: number;

	terminalId: string;
	cashRegisterId: string;

	lrcMode?: LrcMode;

	keepAlive?: boolean;
	autoReconnect?: boolean;

	connectionTimeoutMs?: number;
	responseTimeoutMs?: number;
	ackTimeoutMs?: number;

	// After a transaction result, keep forwarding 'S' receipt lines (onReceiptLine)
	// until this many ms of silence. 0/undefined = off. Set when ECR-printing is on.
	receiptDrainMs?: number;

	retryCount?: number;
	retryDelayMs?: number;

	debug?: boolean;
}

export const PosTerminalStatusMessage = {
	0: "Terminal not configured",
	1: "Terminal configured, no DLL",
	2: "Terminal operative (after a DLL)",
	3: "Terminal not aligned (first DLL requested)",
	4: "KMPB/KPOS key corrupted (first DLL requested)",
	5: "DLL solicited by GT pending",
	6: "Remote SW updated request pending",
	[-1]: "Unknown",
} as const;

export type PosTerminalStatus = keyof typeof PosTerminalStatusMessage;

export interface PosStatusResponse {
	terminalId: string;
	terminalDateTime: Date;
	status: PosTerminalStatus;
	softwareRelease: string;
}

export interface Ecr17Client
	extends HybridObject<{
		ios: "c++";
		android: "c++";
	}> {
	// --- Configuration (synchronous) ---
	configure(config: Ecr17Config): void;
	configuration(): Ecr17Config;

	// --- Connection ---
	connect(): Promise<void>;
	disconnect(): void;
	isConnected(): boolean;

	// --- Commands (asynchronous: each performs a full request/response exchange) ---
	status(): Promise<PosStatusResponse>; //                's'
	pay(request: PaymentRequest): Promise<PaymentResult>; // 'P'
	payExtended(request: PaymentRequest): Promise<PaymentResult>; // 'X'
	reverse(request: ReversalRequest): Promise<ReversalResult>; //  'S'
	preAuth(request: PreAuthRequest): Promise<PreAuthResult>; //    'p'
	incrementalAuth(request: IncrementalAuthRequest): Promise<PreAuthResult>; // 'i'
	preAuthClosure(request: PreAuthClosureRequest): Promise<PaymentResult>; //   'c'
	verifyCard(request: CardVerificationRequest): Promise<CardVerificationResult>; // 'H'
	closeSession(): Promise<CloseSessionResult>; // 'C'
	totals(): Promise<TotalsResult>; //             'T'
	sendLastResult(): Promise<PaymentResult>; //    'G'
	enableEcrPrinting(enabled: boolean): Promise<void>; // 'E'
	reprint(toEcr: boolean): Promise<void>; //             'R'
	vas(xmlRequest: string): Promise<VasResult>; //        'K'

	// --- Events ---
	setOnProgress(callback: (event: ProgressEvent) => void): void;
	setOnReceiptLine(callback: (line: ReceiptLine) => void): void;
	setOnConnectionStateChange(callback: (state: ConnectionState) => void): void;
}
