import type { HybridObject } from "react-native-nitro-modules";
import type { LrcMode } from "../types/client";

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
	configure(config: Ecr17Config): void;
	configuration(): Ecr17Config;

	status(): PosStatusResponse;
}
