#pragma once

#include <cstdint>
#include <string>

// Parsers for ECR17 terminal *response* application messages. They take the
// application payload (the bytes between STX and ETX, i.e. DecodedPacket.payload)
// and return plain C++ structs — intentionally independent of Nitro so they can
// be unit-tested standalone. HybridEcr17Client maps these to the generated Nitro
// result types. Field offsets follow the spec response tables in docs/.
//
// Parsing is defensive: fields beyond the payload length come back empty rather
// than throwing, so a short/truncated response degrades gracefully.

namespace margelo::nitro::ecr17 {

enum class Outcome {
    Ok,             // "00"
    Ko,             // "01"
    CardNotPresent, // "05"
    UnknownTag,     // "09"
    Unknown,        // anything else / missing
};

Outcome outcomeFromCode(const std::string& code);

// Optional DCC / currency-exchange block (parsed from a 'V'/'v' response).
struct CurrencyExchange {
    bool applied = false;
    std::string rate;          // 8 digits, 4 decimals
    std::string currencyCode;  // alpha-3
    std::string amount;        // 12 digits, in transaction currency
    std::string precision;     // decimals
};

// Result of a payment-family response ('E' without DCC, 'V' with DCC). Reused
// for reversal / card verification / pre-auth closure which share the layout.
struct PaymentResponse {
    Outcome outcome = Outcome::Unknown;
    std::string resultCode;       // raw "00"/"01"/"05"/"09"
    std::string pan;              // positive
    std::string transactionType;  // positive, raw "ICC"/"MAG"/...
    std::string authCode;         // positive
    std::string hostDateTime;     // positive, raw DDDHHMM
    std::string errorDescription; // negative
    std::string cardType;         // common, raw "1"/"2"/"3"
    std::string acquirerId;       // common
    std::string stan;             // common
    std::string onlineId;         // common
    CurrencyExchange currency;    // only when DCC present
};

struct StatusResponse {
    std::string terminalId;
    std::string dateTimeRaw;  // "DDMMYYhhmm"
    int status = -1;          // 0..6, -1 = unknown/missing
    std::string softwareRelease;
};

struct TotalsResponse {
    Outcome outcome = Outcome::Unknown;
    std::string resultCode;
    std::string posTotal;  // 16 digits, cents
};

struct CloseResponse {
    Outcome outcome = Outcome::Unknown;
    std::string resultCode;
    std::string posTotal;          // positive, 16 digits
    std::string hostTotal;         // positive, 16 digits
    std::string errorDescription;  // negative
    std::string actionCode;        // negative
};

struct PreAuthResponse {
    Outcome outcome = Outcome::Unknown;
    std::string resultCode;
    std::string pan;
    std::string transactionType;
    std::string authCode;
    std::string preAuthorizedAmount;  // 8 digits, cents
    std::string preAuthCode;          // 9 digits, unique pre-auth id
    std::string actionCode;
    std::string hostDateTime;
    std::string errorDescription;
    std::string cardType;
    std::string acquirerId;
    std::string stan;
    std::string onlineId;
};

struct VasResponse {
    std::string responseId;       // RESPID parsed from XML ("0" = OK), "" if absent
    std::string responseMessage;  // RESPMSG
    std::string orderId;          // ORDER_ID
    bool moreMessages = false;    // concatenation flag "1"
    std::string idMessage;        // 3-digit sequence
    std::string rawXml;           // the XML body of this message
};

class Ecr17Response {
   public:
    static PaymentResponse parsePayment(const std::string& payload);
    static StatusResponse parseStatus(const std::string& payload);
    static TotalsResponse parseTotals(const std::string& payload);
    static CloseResponse parseClose(const std::string& payload);
    static PreAuthResponse parsePreAuth(const std::string& payload);
    static VasResponse parseVas(const std::string& payload);
};

}  // namespace margelo::nitro::ecr17
