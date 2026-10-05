// The core's raw response structs -> the public result types. The same mapping as the
// React Native binding's C++ (HybridEcr17Client.cpp), so both platforms return the same
// objects for the same terminal response.
import type {
	RawClose,
	RawPayment,
	RawPreAuth,
	RawStatus,
	RawTotals,
	RawVas,
} from "./native.ts";
import type {
	CardType,
	CardVerificationResult,
	CloseSessionResult,
	CurrencyExchange,
	PaymentResult,
	PosStatusResponse,
	PosTerminalStatus,
	PreAuthResult,
	ReversalResult,
	TotalsResult,
	TransactionEntryMode,
	VasResult,
} from "./types.ts";

// Optional result fields are omitted, never `undefined`, as on React Native.
type Loose<T> = { [K in keyof T]: T[K] | undefined };

function defined<T extends object>(object: Loose<T>): T {
	const result: Record<string, unknown> = {};
	for (const [key, value] of Object.entries(object)) {
		if (value !== undefined) result[key] = value;
	}
	return result as T;
}

function optStr(value: string): string | undefined {
	return value === "" ? undefined : value;
}

// Numeric protocol fields are digit strings (amounts in cents). Empty or not a number:
// omitted. Values are not rescaled (e.g. the DCC rate keeps its 4 implied decimals).
function optNum(value: string): number | undefined {
	if (value.trim() === "") return undefined;
	const number = Number(value);
	return Number.isFinite(number) ? number : undefined;
}

export function mapCardType(raw: string): CardType | undefined {
	switch (raw) {
		case "1":
			return "debit";
		case "2":
			return "credit";
		case "3":
			return "other";
		default:
			return undefined;
	}
}

export function mapEntryMode(raw: string): TransactionEntryMode | undefined {
	switch (raw) {
		case "ICC":
			return "icc";
		case "MAG":
			return "mag";
		case "MAN":
			return "manual";
		case "CLM":
			return "clessMag";
		case "CLI":
			return "clessIcc";
		default:
			return undefined;
	}
}

export function mapPayment(p: RawPayment): PaymentResult {
	const currencyExchange: CurrencyExchange | undefined = p.currency.applied
		? defined<CurrencyExchange>({
				applied: true,
				rate: optNum(p.currency.rate),
				currencyCode: optStr(p.currency.currencyCode),
				amountCents: optNum(p.currency.amount),
				precision: optNum(p.currency.precision),
			})
		: undefined;
	return defined<PaymentResult>({
		outcome: p.outcome,
		resultCode: p.resultCode,
		pan: optStr(p.pan),
		entryMode: mapEntryMode(p.transactionType),
		authCode: optStr(p.authCode),
		hostDateTime: optStr(p.hostDateTime),
		cardType: mapCardType(p.cardType),
		acquirerId: optStr(p.acquirerId),
		stan: optStr(p.stan),
		onlineId: optStr(p.onlineId),
		errorDescription: optStr(p.errorDescription),
		currencyExchange,
	});
}

export function mapReversal(p: RawPayment): ReversalResult {
	return defined<ReversalResult>({
		outcome: p.outcome,
		resultCode: p.resultCode,
		pan: optStr(p.pan),
		entryMode: mapEntryMode(p.transactionType),
		hostDateTime: optStr(p.hostDateTime),
		cardType: mapCardType(p.cardType),
		acquirerId: optStr(p.acquirerId),
		stan: optStr(p.stan),
		onlineId: optStr(p.onlineId),
		actionCode: undefined,
		errorDescription: optStr(p.errorDescription),
	});
}

export function mapCardVerification(p: RawPayment): CardVerificationResult {
	return defined<CardVerificationResult>({
		outcome: p.outcome,
		resultCode: p.resultCode,
		pan: optStr(p.pan),
		entryMode: mapEntryMode(p.transactionType),
		authCode: optStr(p.authCode),
		hostDateTime: optStr(p.hostDateTime),
		cardType: mapCardType(p.cardType),
		acquirerId: optStr(p.acquirerId),
		stan: optStr(p.stan),
		onlineId: optStr(p.onlineId),
		actionCode: undefined,
		errorDescription: optStr(p.errorDescription),
	});
}

export function mapPreAuth(p: RawPreAuth): PreAuthResult {
	return defined<PreAuthResult>({
		outcome: p.outcome,
		resultCode: p.resultCode,
		pan: optStr(p.pan),
		entryMode: mapEntryMode(p.transactionType),
		authCode: optStr(p.authCode),
		preAuthorizedAmountCents: optNum(p.preAuthorizedAmount),
		preAuthCode: optStr(p.preAuthCode),
		actionCode: optStr(p.actionCode),
		hostDateTime: optStr(p.hostDateTime),
		cardType: mapCardType(p.cardType),
		acquirerId: optStr(p.acquirerId),
		stan: optStr(p.stan),
		onlineId: optStr(p.onlineId),
		errorDescription: optStr(p.errorDescription),
	});
}

// "DDMMYYhhmm" in the terminal's local time; epoch when missing or malformed.
export function parseTerminalDateTime(raw: string): Date {
	const match = /^(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})/.exec(raw);
	if (!match) return new Date(0);
	const [, day, month, year, hour, minute] = match.map(Number) as [
		number,
		number,
		number,
		number,
		number,
		number,
	];
	const date = new Date(2000 + year, month - 1, day, hour, minute);
	return Number.isNaN(date.getTime()) ? new Date(0) : date;
}

export function mapStatus(s: RawStatus): PosStatusResponse {
	const status = (
		s.status >= 0 && s.status <= 6 ? s.status : -1
	) as PosTerminalStatus;
	return {
		terminalId: s.terminalId,
		terminalDateTime: parseTerminalDateTime(s.dateTimeRaw),
		status,
		softwareRelease: s.softwareRelease,
	};
}

export function mapTotals(t: RawTotals): TotalsResult {
	return {
		outcome: t.outcome,
		resultCode: t.resultCode,
		posTotalCents: optNum(t.posTotal) ?? 0,
	};
}

export function mapClose(c: RawClose): CloseSessionResult {
	return defined<CloseSessionResult>({
		outcome: c.outcome,
		resultCode: c.resultCode,
		posTotalCents: optNum(c.posTotal),
		hostTotalCents: optNum(c.hostTotal),
		actionCode: optStr(c.actionCode),
		errorDescription: optStr(c.errorDescription),
	});
}

export function mapVas(v: RawVas): VasResult {
	return defined<VasResult>({
		responseId: v.responseId,
		responseMessage: v.responseMessage,
		orderId: optStr(v.orderId),
		rawXml: v.rawXml,
	});
}
