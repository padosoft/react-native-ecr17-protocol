#pragma once

#include <optional>
#include <string>

#include "ecr17/LrcMode.hpp"

namespace padosoft::ecr17 {

enum class ConnectionState { Disconnected, Connecting, Connected };

/// Requested card handling (request "Payment type" digit '0'..'3').
enum class PaymentCardType { Auto, Debit, Credit, Other };

enum class TokenizationService { Recurring, UnscheduledOrOneClick };

struct TokenizationRequest {
    TokenizationService service = TokenizationService::UnscheduledOrOneClick;
    std::string contractCode;
};

/// Terminal connection and protocol settings. The defaults are the JS API's.
struct ClientConfig {
    std::string host;
    int port = 10000;
    int connectionTimeoutMs = 5000;  // <= 0 waits indefinitely

    std::string terminalId;
    std::string cashRegisterId;

    LrcMode lrcMode = LrcMode::STD;
    int ackTimeoutMs = 2000;
    int responseTimeoutMs = 60000;
    int retryCount = 3;
    int retryDelayMs = 200;
    int receiptDrainMs = 0;  // keep forwarding receipt lines for this long after a result

    /// Reconnect the socket when it drops mid-command. Only read-only commands are then
    /// re-sent; a financial command never is (see RetryPolicy.hpp).
    bool autoReconnect = false;
};

/// Payment ('P'), extended payment ('X') and pre-authorization ('p').
struct PaymentRequest {
    int amountCents = 0;
    std::optional<std::string> cashRegisterId;  // default: ClientConfig::cashRegisterId
    PaymentCardType paymentType = PaymentCardType::Auto;
    bool cardAlreadyPresent = false;
    std::string receiptText;
    std::optional<TokenizationRequest> tokenization;
};

using PreAuthRequest = PaymentRequest;

struct ReversalRequest {
    std::optional<std::string> cashRegisterId;
    std::string stan = "000000";  // "000000": reverse the last transaction
};

/// Incremental authorization ('i') and pre-authorization closure ('c').
struct PreAuthFollowUpRequest {
    int amountCents = 0;
    std::string originalPreAuthCode;
    std::optional<std::string> cashRegisterId;
    std::string receiptText;
};

struct CardVerificationRequest {
    std::optional<std::string> cashRegisterId;
    PaymentCardType paymentType = PaymentCardType::Auto;
    std::optional<TokenizationRequest> tokenization;
};

/// The request digit for `type` ('0' auto, '1' debit, '2' credit, '3' other).
inline char paymentTypeCode(PaymentCardType type) {
    switch (type) {
        case PaymentCardType::Debit: return '1';
        case PaymentCardType::Credit: return '2';
        case PaymentCardType::Other: return '3';
        case PaymentCardType::Auto: break;
    }
    return '0';
}

}  // namespace padosoft::ecr17
