// LRC scope selector. Which framing bytes are folded into the LRC (base 0x7F):
//  - "stx":       STX + payload + ETX
//  - "std":       payload only
//  - "noext":     payload + ETX
//  - "stx_noext": STX + payload
export type LrcMode = "stx" | "std" | "noext" | "stx_noext";

// Transport connection lifecycle.
export type ConnectionState = "disconnected" | "connecting" | "connected";

// Normalized transaction outcome (mapped from the raw 2-digit result code).
//  "00" -> ok, "01" -> ko, "05" -> cardNotPresent, "09" -> unknownTag.
export type TransactionOutcome = "ok" | "ko" | "cardNotPresent" | "unknownTag" | "unknown";

// Card category reported by the terminal (response "Card type": 1/2/3).
export type CardType = "debit" | "credit" | "other" | "unknown";

// How the card was read (response "Transaction type": ICC/MAG/MAN/CLM/CLI).
export type TransactionEntryMode = "icc" | "mag" | "manual" | "clessMag" | "clessIcc" | "unknown";

// Requested card handling for a payment (request "Payment type": 0..3).
export type PaymentCardType = "auto" | "debit" | "credit" | "other";

// Tokenization service requested alongside a payment/preauth/verification (command "U").
export type TokenizationService = "recurring" | "unscheduledOrOneClick";

export interface TokenizationRequest {
  service: TokenizationService;
  // Unique contract code at merchant level, alphanumeric, up to 18 chars.
  contractCode: string;
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

export interface PaymentRequest {
  amountCents: number;
  // Defaults to the configured cashRegisterId when omitted.
  cashRegisterId?: string;
  paymentType?: PaymentCardType;
  // true => start with the card already inserted in the terminal.
  cardAlreadyPresent?: boolean;
  // Text (e.g. contract code) printed at the end of the receipt (max 128 chars).
  receiptText?: string;
  // Attach tokenization additional data ("U") to this transaction.
  tokenization?: TokenizationRequest;
}

export interface ReversalRequest {
  cashRegisterId?: string;
  // STAN of the transaction to reverse; "000000" (default) reverses the last
  // payment with no STAN check.
  stan?: string;
}

export interface PreAuthRequest {
  amountCents: number;
  cashRegisterId?: string;
  paymentType?: PaymentCardType;
  cardAlreadyPresent?: boolean;
  receiptText?: string;
  tokenization?: TokenizationRequest;
}

export interface IncrementalAuthRequest {
  amountCents: number;
  // Unique pre-authorization code from the original pre-auth response.
  originalPreAuthCode: string;
  cashRegisterId?: string;
  receiptText?: string;
}

export interface PreAuthClosureRequest {
  amountCents: number;
  originalPreAuthCode: string;
  cashRegisterId?: string;
  receiptText?: string;
}

export interface CardVerificationRequest {
  cashRegisterId?: string;
  paymentType?: PaymentCardType;
  tokenization?: TokenizationRequest;
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

// DCC / currency-exchange block (only meaningful when applied === true).
export interface CurrencyExchange {
  applied: boolean;
  rate?: number;
  currencyCode?: string;
  amountCents?: number;
  precision?: number;
}

export interface PaymentResult {
  outcome: TransactionOutcome;
  resultCode: string; // raw "00"/"01"/"05"/"09"
  pan?: string;
  entryMode?: TransactionEntryMode;
  authCode?: string;
  hostDateTime?: string; // raw DDDHHMM as received from host
  cardType?: CardType;
  acquirerId?: string;
  stan?: string;
  onlineId?: string;
  errorDescription?: string;
  currencyExchange?: CurrencyExchange;
}

export interface ReversalResult {
  outcome: TransactionOutcome;
  resultCode: string;
  pan?: string;
  entryMode?: TransactionEntryMode;
  hostDateTime?: string;
  cardType?: CardType;
  acquirerId?: string;
  stan?: string;
  onlineId?: string;
  actionCode?: string;
  errorDescription?: string;
}

export interface PreAuthResult {
  outcome: TransactionOutcome;
  resultCode: string;
  pan?: string;
  entryMode?: TransactionEntryMode;
  authCode?: string;
  preAuthorizedAmountCents?: number;
  preAuthCode?: string;
  actionCode?: string;
  hostDateTime?: string;
  cardType?: CardType;
  acquirerId?: string;
  stan?: string;
  onlineId?: string;
  errorDescription?: string;
}

export interface CardVerificationResult {
  outcome: TransactionOutcome;
  resultCode: string;
  pan?: string;
  entryMode?: TransactionEntryMode;
  authCode?: string;
  hostDateTime?: string;
  cardType?: CardType;
  acquirerId?: string;
  stan?: string;
  onlineId?: string;
  actionCode?: string;
  errorDescription?: string;
}

export interface TotalsResult {
  outcome: TransactionOutcome;
  resultCode: string;
  posTotalCents: number;
}

export interface CloseSessionResult {
  outcome: TransactionOutcome;
  resultCode: string;
  posTotalCents?: number;
  hostTotalCents?: number;
  actionCode?: string;
  errorDescription?: string;
}

export interface VasResult {
  responseId: string; // RESPID ("0" = OK)
  responseMessage: string; // RESPMSG
  orderId?: string; // ORDER_ID when present
  rawXml: string; // full concatenated XML response
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

// Progress-update message shown on the ECR display during a procedure (SOH frame).
export interface ProgressEvent {
  message: string;
}

// A single receipt line streamed by the terminal ("S" message) when ECR
// printing is enabled.
export interface ReceiptLine {
  text: string;
}
