import type { HybridObject } from "react-native-nitro-modules";

// Low-level byte transport for the ECR17 LAN connection.
//
// Implemented natively (Swift on iOS, Kotlin on Android) so it can use the
// platform networking stacks. The C++ protocol core consumes it through a thin
// adapter; all ECR17 framing/orchestration stays in shared C++.
//
// Received bytes are delivered via setOnData (called from a native reader
// thread). `send` is fire-and-forget; back-pressure/ordering is the transport's
// responsibility.
export interface Ecr17Transport
	extends HybridObject<{
		ios: "swift";
		android: "kotlin";
	}> {
	connect(host: string, port: number, timeoutMs: number): Promise<void>;
	disconnect(): void;
	isConnected(): boolean;

	send(bytes: ArrayBuffer): void;

	setOnData(callback: (bytes: ArrayBuffer) => void): void;
	setOnDisconnect(callback: () => void): void;
}
