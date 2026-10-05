#include "Ecr17Kit/Ecr17Client.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

#include "Ecr17Kit/Ecr17Protocol.hpp"
#include "Ecr17Kit/RetryPolicy.hpp"

namespace padosoft::ecr17 {

namespace {

SessionConfig sessionConfig(const ClientConfig& config) {
    SessionConfig sc;
    sc.lrcMode = config.lrcMode;
    sc.ackTimeoutMs = config.ackTimeoutMs;
    sc.responseTimeoutMs = config.responseTimeoutMs;
    sc.retryCount = config.retryCount;
    sc.retryDelayMs = config.retryDelayMs;
    sc.receiptDrainMs = config.receiptDrainMs;
    return sc;
}

}  // namespace

Ecr17Client::Ecr17Client(std::shared_ptr<Transport> transport, ClientConfig config)
    : transport_(std::move(transport)), config_(std::move(config)) {
    if (!transport_) {
        throw std::invalid_argument("ECR17: Ecr17Client needs a transport");
    }
    session_ = std::make_unique<Ecr17Session>(*transport_, sessionConfig(config_));
    session_->setOnProgress([this](const std::string& message) {
        std::function<void(const std::string&)> callback;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callback = onProgress_;
        }
        if (callback) callback(message);
    });
    session_->setOnReceiptLine([this](const std::string& line) {
        std::function<void(const std::string&)> callback;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callback = onReceiptLine_;
        }
        if (callback) callback(line);
    });
}

Ecr17Client::~Ecr17Client() {
    // The session registered callbacks on the transport that point at it: detach
    // them before it goes away (the transport may outlive this client).
    transport_->setDataCallback(nullptr);
    transport_->setDisconnectCallback(nullptr);
}

void Ecr17Client::setOnProgress(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    onProgress_ = std::move(callback);
}

void Ecr17Client::setOnReceiptLine(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    onReceiptLine_ = std::move(callback);
}

void Ecr17Client::setOnConnectionStateChange(std::function<void(ConnectionState)> callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    onConnectionStateChange_ = std::move(callback);
}

void Ecr17Client::emit(ConnectionState state) {
    std::function<void(ConnectionState)> callback;
    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callback = onConnectionStateChange_;
    }
    if (callback) callback(state);
}

void Ecr17Client::ensureConnected() {
    // PROACTIVE reconnect: isConnected() is a synchronous, non-destructive liveness
    // probe, so a peer-closed or half-open socket (ECR17/Nexi terminals close TCP
    // between transactions) is found HERE, before any command is sent. Without it a
    // financial command would go out on a stale socket and hit the (correct)
    // never-replay path with a false "transport disconnected".
    if (transport_->isConnected()) {
        return;
    }
    emit(ConnectionState::Connecting);
    try {
        transport_->connect(Endpoint{config_.host, config_.port, config_.connectionTimeoutMs});
    } catch (...) {
        // Don't leave listeners stuck on Connecting when the connection fails.
        emit(ConnectionState::Disconnected);
        throw;
    }
    emit(ConnectionState::Connected);
}

void Ecr17Client::connect() { ensureConnected(); }

void Ecr17Client::disconnect() {
    transport_->disconnect();
    emit(ConnectionState::Disconnected);
}

bool Ecr17Client::isConnected() { return transport_->isConnected(); }

std::string Ecr17Client::cashRegisterIdOr(const std::optional<std::string>& override) const {
    return override.value_or(config_.cashRegisterId);
}

DecodedPacket Ecr17Client::runTransaction(const std::string& mainPayload,
                                          const std::optional<TokenizationRequest>& tokenization,
                                          bool safeToRetry) {
    std::lock_guard<std::mutex> txLock(txMutex_);
    auto doExchange = [&]() -> DecodedPacket {
        if (tokenization.has_value()) {
            const bool recurring = tokenization->service == TokenizationService::Recurring;
            const std::string tag = Ecr17Protocol::formatTokenizationTag(recurring, tokenization->contractCode);
            const std::string additional = Ecr17Protocol::buildAdditionalTagsMessage(config_.terminalId, tag);
            return session_->exchangeWithAdditionalData(mainPayload, additional);
        }
        return session_->exchange(mainPayload);
    };

    try {
        return doExchange();
    } catch (const std::exception&) {
        const auto originalError = std::current_exception();
        const bool dropped = !transport_->isConnected();
        if (config_.autoReconnect && dropped) {
            try {
                ensureConnected();  // restore the socket for subsequent commands
            } catch (...) {
                // Surface the exchange error, not the reconnect failure.
                std::rethrow_exception(originalError);
            }
        }
        if (shouldRetryAfterReconnect(config_.autoReconnect, dropped, safeToRetry)) {
            return doExchange();  // only read-only/idempotent commands may be replayed
        }
        throw;  // financial command: surface the error (recover via sendLastResult / 'G')
    }
}

void Ecr17Client::runAckOnly(const std::string& payload, bool safeToRetry) {
    std::lock_guard<std::mutex> txLock(txMutex_);
    try {
        session_->sendAckOnly(payload);
    } catch (const std::exception&) {
        const auto originalError = std::current_exception();
        const bool dropped = !transport_->isConnected();
        if (config_.autoReconnect && dropped) {
            try {
                ensureConnected();
            } catch (...) {
                std::rethrow_exception(originalError);
            }
        }
        if (!shouldRetryAfterReconnect(config_.autoReconnect, dropped, safeToRetry)) {
            throw;
        }
        session_->sendAckOnly(payload);  // read-only/idempotent command: safe to replay
    }
}

StatusResponse Ecr17Client::status() {
    ensureConnected();
    auto pkt = runTransaction(Ecr17Protocol::buildStatusMessage(config_.terminalId), std::nullopt,
                              /*safeToRetry=*/true);
    return Ecr17Response::parseStatus(pkt.payload);
}

PaymentResponse Ecr17Client::pay(const PaymentRequest& request) {
    ensureConnected();
    auto payload = Ecr17Protocol::buildPaymentMessage(
        config_.terminalId, cashRegisterIdOr(request.cashRegisterId), request.amountCents,
        paymentTypeCode(request.paymentType), request.cardAlreadyPresent, request.tokenization.has_value(),
        request.receiptText);
    auto pkt = runTransaction(payload, request.tokenization, /*safeToRetry=*/false);
    return Ecr17Response::parsePayment(pkt.payload);
}

PaymentResponse Ecr17Client::payExtended(const PaymentRequest& request) {
    ensureConnected();
    auto payload = Ecr17Protocol::buildExtendedPaymentMessage(
        config_.terminalId, cashRegisterIdOr(request.cashRegisterId), request.amountCents,
        paymentTypeCode(request.paymentType), request.cardAlreadyPresent, request.tokenization.has_value(),
        request.receiptText);
    auto pkt = runTransaction(payload, request.tokenization, /*safeToRetry=*/false);
    return Ecr17Response::parsePayment(pkt.payload);
}

PaymentResponse Ecr17Client::reverse(const ReversalRequest& request) {
    ensureConnected();
    auto payload = Ecr17Protocol::buildReversalMessage(config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
                                                       request.stan);
    auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
    return Ecr17Response::parsePayment(pkt.payload);
}

PreAuthResponse Ecr17Client::preAuth(const PreAuthRequest& request) {
    ensureConnected();
    auto payload = Ecr17Protocol::buildPreAuthMessage(
        config_.terminalId, cashRegisterIdOr(request.cashRegisterId), request.amountCents,
        paymentTypeCode(request.paymentType), request.cardAlreadyPresent, request.tokenization.has_value(),
        request.receiptText);
    auto pkt = runTransaction(payload, request.tokenization, /*safeToRetry=*/false);
    return Ecr17Response::parsePreAuth(pkt.payload);
}

PreAuthResponse Ecr17Client::incrementalAuth(const PreAuthFollowUpRequest& request) {
    ensureConnected();
    auto payload = Ecr17Protocol::buildIncrementalMessage(config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
                                                          request.amountCents, request.originalPreAuthCode, false,
                                                          request.receiptText);
    auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
    return Ecr17Response::parsePreAuth(pkt.payload);
}

PaymentResponse Ecr17Client::preAuthClosure(const PreAuthFollowUpRequest& request) {
    ensureConnected();
    auto payload = Ecr17Protocol::buildPreAuthClosureMessage(config_.terminalId,
                                                             cashRegisterIdOr(request.cashRegisterId),
                                                             request.amountCents, request.originalPreAuthCode, false,
                                                             request.receiptText);
    auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
    return Ecr17Response::parsePayment(pkt.payload);
}

PaymentResponse Ecr17Client::verifyCard(const CardVerificationRequest& request) {
    ensureConnected();
    auto payload = Ecr17Protocol::buildCardVerificationMessage(
        config_.terminalId, cashRegisterIdOr(request.cashRegisterId), paymentTypeCode(request.paymentType),
        request.tokenization.has_value());
    auto pkt = runTransaction(payload, request.tokenization, /*safeToRetry=*/false);
    return Ecr17Response::parsePayment(pkt.payload);
}

CloseResponse Ecr17Client::closeSession() {
    ensureConnected();
    auto payload = Ecr17Protocol::buildCloseSessionMessage(config_.terminalId, config_.cashRegisterId);
    auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
    return Ecr17Response::parseClose(pkt.payload);
}

TotalsResponse Ecr17Client::totals() {
    ensureConnected();
    auto payload = Ecr17Protocol::buildTotalsMessage(config_.terminalId, config_.cashRegisterId);
    auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/true);
    return Ecr17Response::parseTotals(pkt.payload);
}

PaymentResponse Ecr17Client::sendLastResult() {
    ensureConnected();
    auto payload = Ecr17Protocol::buildSendLastResultMessage(config_.terminalId, config_.cashRegisterId);
    auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/true);
    return Ecr17Response::parsePayment(pkt.payload);
}

void Ecr17Client::enableEcrPrinting(bool enabled) {
    ensureConnected();
    runAckOnly(Ecr17Protocol::buildEnableEcrPrintMessage(config_.terminalId, enabled), /*safeToRetry=*/true);
}

void Ecr17Client::reprint(bool toEcr) {
    ensureConnected();
    runAckOnly(Ecr17Protocol::buildReprintMessage(config_.terminalId, toEcr), /*safeToRetry=*/false);
}

VasResponse Ecr17Client::vas(const std::string& xmlRequest) {
    ensureConnected();
    auto payload = Ecr17Protocol::buildVasMessage(config_.terminalId, config_.cashRegisterId, xmlRequest);
    auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
    return Ecr17Response::parseVas(pkt.payload);
}

}  // namespace padosoft::ecr17
