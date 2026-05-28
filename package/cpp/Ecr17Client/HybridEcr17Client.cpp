#include "HybridEcr17Client.hpp"

#include <NitroModules/HybridObjectRegistry.hpp>

#include <chrono>
#include <ctime>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "Ecr17Protocol/Ecr17Protocol.hpp"
#include "Ecr17Response/Ecr17Response.hpp"
#include "Session/RetryPolicy.hpp"

// Commands run on Nitro's C++ thread pool. On Android, those worker threads are
// NOT attached to the JVM, and — even once attached — JNI `FindClass` on an
// attached worker thread resolves against the *system* class loader, which can't
// see app/NitroModules classes (e.g. com.margelo.nitro.core.ArrayBuffer, looked
// up lazily by the generated transport bridge in `send()` via JArrayBuffer::wrap).
// That yields "Unable to retrieve jni environment" (no JNIEnv) or
// "ClassNotFoundException ... DexPathList[... /system/lib64 ...]" (wrong loader).
#ifdef __ANDROID__
#include <fbjni/fbjni.h>
#endif

namespace margelo::nitro::ecr17 {

namespace {

// Runs `fn` on Android under fbjni's ThreadScope::WithClassLoader, which attaches
// the current thread to the JVM AND installs fbjni's cached app class loader for
// the duration — so every JNI FindClass inside (including NitroModules' lazy
// ArrayBuffer lookup) resolves app classes, not the system loader. On iOS (no JVM)
// `fn` is just called directly. Returns whatever `fn` returns (incl. void) and
// propagates exceptions, so the caller's try/catch and return-value logic is
// unchanged. WithClassLoader takes a `std::function<void()>`, so on Android the
// result is captured in a local and any exception via std::exception_ptr, then
// rethrown after the scope. `fn` is a lambda, so a `return` inside it returns from
// the lambda (not the caller) on BOTH platforms — safe in value-returning callers.
template <typename Fn>
auto runOnJvmThread(Fn&& fn) -> decltype(fn()) {
#ifdef __ANDROID__
    using Ret = decltype(fn());
    std::exception_ptr err;
    if constexpr (std::is_void_v<Ret>) {
        ::facebook::jni::ThreadScope::WithClassLoader([&]() {
            try {
                fn();
            } catch (...) {
                err = std::current_exception();
            }
        });
        if (err) std::rethrow_exception(err);
    } else {
        std::optional<Ret> result;
        ::facebook::jni::ThreadScope::WithClassLoader([&]() {
            try {
                result.emplace(fn());
            } catch (...) {
                err = std::current_exception();
            }
        });
        if (err) std::rethrow_exception(err);
        return std::move(*result);
    }
#else
    return fn();
#endif
}

}  // namespace

using margelo::nitro::HybridObjectRegistry;
using margelo::nitro::Promise;

namespace {

std::optional<std::string> optStr(const std::string& s) {
    return s.empty() ? std::nullopt : std::optional<std::string>(s);
}

std::optional<double> optNum(const std::string& s) {
    if (s.empty()) {
        return std::nullopt;
    }
    try {
        return std::stod(s);
    } catch (...) {
        return std::nullopt;
    }
}

TransactionOutcome mapOutcome(Outcome o) {
    switch (o) {
        case Outcome::Ok: return TransactionOutcome::OK;
        case Outcome::Ko: return TransactionOutcome::KO;
        case Outcome::CardNotPresent: return TransactionOutcome::CARDNOTPRESENT;
        case Outcome::UnknownTag: return TransactionOutcome::UNKNOWNTAG;
        default: return TransactionOutcome::UNKNOWN;
    }
}

std::optional<CardType> mapCardType(const std::string& raw) {
    if (raw == "1") return CardType::DEBIT;
    if (raw == "2") return CardType::CREDIT;
    if (raw == "3") return CardType::OTHER;
    return std::nullopt;
}

std::optional<TransactionEntryMode> mapEntryMode(const std::string& raw) {
    if (raw == "ICC") return TransactionEntryMode::ICC;
    if (raw == "MAG") return TransactionEntryMode::MAG;
    if (raw == "MAN") return TransactionEntryMode::MANUAL;
    if (raw == "CLM") return TransactionEntryMode::CLESSMAG;
    if (raw == "CLI") return TransactionEntryMode::CLESSICC;
    return std::nullopt;
}

char mapPaymentType(const std::optional<PaymentCardType>& t) {
    if (!t.has_value()) return '0';
    switch (*t) {
        case PaymentCardType::DEBIT: return '1';
        case PaymentCardType::CREDIT: return '2';
        case PaymentCardType::OTHER: return '3';
        default: return '0';  // AUTO
    }
}

PaymentResult mapPayment(const PaymentResponse& p) {
    PaymentResult r;
    r.outcome = mapOutcome(p.outcome);
    r.resultCode = p.resultCode;
    r.pan = optStr(p.pan);
    r.entryMode = mapEntryMode(p.transactionType);
    r.authCode = optStr(p.authCode);
    r.hostDateTime = optStr(p.hostDateTime);
    r.cardType = mapCardType(p.cardType);
    r.acquirerId = optStr(p.acquirerId);
    r.stan = optStr(p.stan);
    r.onlineId = optStr(p.onlineId);
    r.errorDescription = optStr(p.errorDescription);
    if (p.currency.applied) {
        CurrencyExchange ce;  // the Nitro-generated struct
        ce.applied = true;
        ce.rate = optNum(p.currency.rate);
        ce.currencyCode = optStr(p.currency.currencyCode);
        ce.amountCents = optNum(p.currency.amount);
        ce.precision = optNum(p.currency.precision);
        r.currencyExchange = ce;
    }
    return r;
}

ReversalResult mapReversal(const PaymentResponse& p) {
    ReversalResult r;
    r.outcome = mapOutcome(p.outcome);
    r.resultCode = p.resultCode;
    r.pan = optStr(p.pan);
    r.entryMode = mapEntryMode(p.transactionType);
    r.hostDateTime = optStr(p.hostDateTime);
    r.cardType = mapCardType(p.cardType);
    r.acquirerId = optStr(p.acquirerId);
    r.stan = optStr(p.stan);
    r.onlineId = optStr(p.onlineId);
    r.errorDescription = optStr(p.errorDescription);
    return r;
}

CardVerificationResult mapCardVerify(const PaymentResponse& p) {
    CardVerificationResult r;
    r.outcome = mapOutcome(p.outcome);
    r.resultCode = p.resultCode;
    r.pan = optStr(p.pan);
    r.entryMode = mapEntryMode(p.transactionType);
    r.authCode = optStr(p.authCode);
    r.hostDateTime = optStr(p.hostDateTime);
    r.cardType = mapCardType(p.cardType);
    r.acquirerId = optStr(p.acquirerId);
    r.stan = optStr(p.stan);
    r.onlineId = optStr(p.onlineId);
    r.errorDescription = optStr(p.errorDescription);
    return r;
}

PreAuthResult mapPreAuth(const PreAuthResponse& p) {
    PreAuthResult r;
    r.outcome = mapOutcome(p.outcome);
    r.resultCode = p.resultCode;
    r.pan = optStr(p.pan);
    r.entryMode = mapEntryMode(p.transactionType);
    r.authCode = optStr(p.authCode);
    r.preAuthorizedAmountCents = optNum(p.preAuthorizedAmount);
    r.preAuthCode = optStr(p.preAuthCode);
    r.actionCode = optStr(p.actionCode);
    r.hostDateTime = optStr(p.hostDateTime);
    r.cardType = mapCardType(p.cardType);
    r.acquirerId = optStr(p.acquirerId);
    r.stan = optStr(p.stan);
    r.onlineId = optStr(p.onlineId);
    r.errorDescription = optStr(p.errorDescription);
    return r;
}

PosStatusResponse mapStatus(const StatusResponse& s) {
    PosStatusResponse r;
    r.terminalId = s.terminalId;
    r.status = static_cast<double>(s.status);
    r.softwareRelease = s.softwareRelease;
    // Parse "DDMMYYhhmm" into a time_point; fall back to epoch on bad input.
    std::chrono::system_clock::time_point tp{};
    if (s.dateTimeRaw.size() >= 10) {
        try {
            std::tm tm{};
            tm.tm_mday = std::stoi(s.dateTimeRaw.substr(0, 2));
            tm.tm_mon = std::stoi(s.dateTimeRaw.substr(2, 2)) - 1;
            tm.tm_year = 100 + std::stoi(s.dateTimeRaw.substr(4, 2));  // 20YY
            tm.tm_hour = std::stoi(s.dateTimeRaw.substr(6, 2));
            tm.tm_min = std::stoi(s.dateTimeRaw.substr(8, 2));
            tm.tm_isdst = -1;
            std::time_t t = std::mktime(&tm);
            if (t != -1) {
                tp = std::chrono::system_clock::from_time_t(t);
            }
        } catch (...) {
            // keep epoch
        }
    }
    r.terminalDateTime = tp;
    return r;
}

TotalsResult mapTotals(const TotalsResponse& t) {
    TotalsResult r;
    r.outcome = mapOutcome(t.outcome);
    r.resultCode = t.resultCode;
    r.posTotalCents = optNum(t.posTotal).value_or(0.0);
    return r;
}

CloseSessionResult mapClose(const CloseResponse& c) {
    CloseSessionResult r;
    r.outcome = mapOutcome(c.outcome);
    r.resultCode = c.resultCode;
    r.posTotalCents = optNum(c.posTotal);
    r.hostTotalCents = optNum(c.hostTotal);
    r.actionCode = optStr(c.actionCode);
    r.errorDescription = optStr(c.errorDescription);
    return r;
}

VasResult mapVas(const VasResponse& v) {
    VasResult r;
    r.responseId = v.responseId;
    r.responseMessage = v.responseMessage;
    r.orderId = optStr(v.orderId);
    r.rawXml = v.rawXml;
    return r;
}

}  // namespace

void HybridEcr17Client::configure(const Ecr17Config& config) {
    config_ = config;
    // Close any open socket before tearing down the old transport, otherwise the
    // prior native connection leaks until the HybridObject is collected.
    if (transport_) {
        transport_->disconnect();
    }
    // Force re-init so a new configuration rebuilds the session/timeouts.
    session_.reset();
    adapter_.reset();
    transport_.reset();
    // Create the transport HybridObject NOW, on this (JS) thread. createHybridObject
    // does a JNI FindClass for the Kotlin transport, which resolves only against the
    // app class loader — and the JS thread has it. Doing it lazily on a Nitro worker
    // thread (attached via ThreadScope) would use the system class loader and throw
    // ClassNotFoundException. fbjni caches the resolved jclass globally, so later
    // method calls from worker threads work (they only need a JNIEnv, see the guards).
    ensureInit();
}

Ecr17Config HybridEcr17Client::configuration() { return config_; }

void HybridEcr17Client::ensureInit() {
    if (session_) {
        return;
    }
    auto obj = HybridObjectRegistry::createHybridObject("Ecr17Transport");
    // HybridObject is a *virtual* base, so static_pointer_cast can't downcast
    // from it — must use dynamic_pointer_cast.
    transport_ = std::dynamic_pointer_cast<HybridEcr17TransportSpec>(obj);
    if (!transport_) {
        throw std::runtime_error("ECR17: registry returned an incompatible Ecr17Transport object");
    }
    adapter_ = std::make_shared<NativeTransportAdapter>(transport_);

    SessionConfig sc;
    sc.lrcMode = config_.lrcMode.value_or(LrcMode::STD);
    sc.ackTimeoutMs = static_cast<int>(config_.ackTimeoutMs.value_or(2000));
    sc.responseTimeoutMs = static_cast<int>(config_.responseTimeoutMs.value_or(60000));
    sc.retryCount = static_cast<int>(config_.retryCount.value_or(3));
    sc.retryDelayMs = static_cast<int>(config_.retryDelayMs.value_or(200));
    sc.receiptDrainMs = static_cast<int>(config_.receiptDrainMs.value_or(0));
    session_ = std::make_unique<Ecr17Session>(*adapter_, sc);

    session_->setOnProgress([this](const std::string& message) {
        if (onProgress_) onProgress_(ProgressEvent{message});
    });
    session_->setOnReceiptLine([this](const std::string& line) {
        if (onReceiptLine_) onReceiptLine_(ReceiptLine{line});
    });
}

void HybridEcr17Client::ensureConnected() {
    // Run the transport JNI work under the app class loader (Android); inline on iOS.
    runOnJvmThread([&]() {
        ensureInit();
        if (transport_->isConnected()) {
            return;  // returns from this lambda only — no further JNI needed
        }
        // Auto-connect: block this worker thread until the native transport connects
        // (or throw on failure). keepAlive leaves the socket open for reuse.
        if (onConnectionStateChange_) onConnectionStateChange_(ConnectionState::CONNECTING);
        const double port = config_.port.value_or(1024);
        const double timeout = config_.connectionTimeoutMs.value_or(5000);
        try {
            transport_->connect(config_.host, port, timeout)->await().get();
        } catch (...) {
            // Don't leave listeners stuck on CONNECTING when the connection fails.
            if (onConnectionStateChange_) onConnectionStateChange_(ConnectionState::DISCONNECTED);
            throw;
        }
        if (onConnectionStateChange_) onConnectionStateChange_(ConnectionState::CONNECTED);
    });
}

std::string HybridEcr17Client::cashRegisterIdOr(const std::optional<std::string>& override) const {
    return override.value_or(config_.cashRegisterId);
}

DecodedPacket HybridEcr17Client::runTransaction(
    const std::string& mainPayload, const std::optional<TokenizationRequest>& tokenization,
    bool safeToRetry) {
    std::lock_guard<std::mutex> txLock(txMutex_);  // serialize exchanges on the shared session
    auto doExchange = [&]() -> DecodedPacket {
        if (tokenization.has_value()) {
            const bool recurring = tokenization->service == TokenizationService::RECURRING;
            const std::string tag =
                Ecr17Protocol::formatTokenizationTag(recurring, tokenization->contractCode);
            const std::string additional =
                Ecr17Protocol::buildAdditionalTagsMessage(config_.terminalId, tag);
            return session_->exchangeWithAdditionalData(mainPayload, additional);
        }
        return session_->exchange(mainPayload);
    };

    // All exchange JNI (session_ -> adapter_ -> Kotlin transport, incl. the
    // ArrayBuffer lookup in send()) must run under the app class loader on Android.
    return runOnJvmThread([&]() -> DecodedPacket {
        try {
            return doExchange();
        } catch (const std::exception&) {
            const auto originalError = std::current_exception();
            const bool autoReconnect = config_.autoReconnect.value_or(false);
            const bool dropped = !transport_ || !transport_->isConnected();
            if (autoReconnect && dropped) {
                try {
                    ensureConnected();  // restore the socket for subsequent commands
                } catch (...) {
                    // Reconnect failed: surface the original exchange error, not the
                    // reconnect failure (the former is what the caller needs to see).
                    std::rethrow_exception(originalError);
                }
            }
            if (shouldRetryAfterReconnect(autoReconnect, dropped, safeToRetry)) {
                return doExchange();  // only read-only/idempotent ops may be replayed
            }
            throw;  // financial op: surface the error (recover via sendLastResult / 'G')
        }
    });
}

void HybridEcr17Client::runAckOnly(const std::string& payload, bool safeToRetry) {
    std::lock_guard<std::mutex> txLock(txMutex_);  // serialize exchanges on the shared session
    // Send under the app class loader on Android (ArrayBuffer lookup in send()).
    runOnJvmThread([&]() {
        try {
            session_->sendAckOnly(payload);
        } catch (const std::exception&) {
            const auto originalError = std::current_exception();
            const bool autoReconnect = config_.autoReconnect.value_or(false);
            const bool dropped = !transport_ || !transport_->isConnected();
            if (autoReconnect && dropped) {
                try {
                    ensureConnected();
                } catch (...) {
                    std::rethrow_exception(originalError);  // surface the original error
                }
            }
            if (!shouldRetryAfterReconnect(autoReconnect, dropped, safeToRetry)) {
                throw;  // not retryable: surface the original error
            }
            session_->sendAckOnly(payload);  // read-only/idempotent op: safe to replay
        }
    });
}

std::shared_ptr<Promise<void>> HybridEcr17Client::connect() {
    // Delegate to ensureConnected so the explicit Connect path emits CONNECTING
    // and then CONNECTED on success (consistent with command auto-connect);
    // returning the raw transport promise would leave listeners stuck on
    // CONNECTING. Runs on a worker thread (ensureConnected blocks until ready).
    return Promise<void>::async([this]() { ensureConnected(); });
}

void HybridEcr17Client::disconnect() {
    if (transport_) {
        transport_->disconnect();
    }
    if (onConnectionStateChange_) onConnectionStateChange_(ConnectionState::DISCONNECTED);
}

bool HybridEcr17Client::isConnected() { return transport_ && transport_->isConnected(); }

std::shared_ptr<Promise<PosStatusResponse>> HybridEcr17Client::status() {
    return Promise<PosStatusResponse>::async([this]() -> PosStatusResponse {
        ensureConnected();
        auto pkt = runTransaction(Ecr17Protocol::buildStatusMessage(config_.terminalId),
                                  std::nullopt, /*safeToRetry=*/true);
        return mapStatus(Ecr17Response::parseStatus(pkt.payload));
    });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::pay(const PaymentRequest& request) {
    return Promise<PaymentResult>::async([this, request]() -> PaymentResult {
        ensureConnected();
        const bool tok = request.tokenization.has_value();
        auto payload = Ecr17Protocol::buildPaymentMessage(
            config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
            static_cast<int>(request.amountCents), mapPaymentType(request.paymentType),
            request.cardAlreadyPresent.value_or(false), tok, request.receiptText.value_or(""));
        auto pkt = runTransaction(payload, request.tokenization, false);
        return mapPayment(Ecr17Response::parsePayment(pkt.payload));
    });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::payExtended(const PaymentRequest& request) {
    return Promise<PaymentResult>::async([this, request]() -> PaymentResult {
        ensureConnected();
        const bool tok = request.tokenization.has_value();
        auto payload = Ecr17Protocol::buildExtendedPaymentMessage(
            config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
            static_cast<int>(request.amountCents), mapPaymentType(request.paymentType),
            request.cardAlreadyPresent.value_or(false), tok, request.receiptText.value_or(""));
        auto pkt = runTransaction(payload, request.tokenization, false);
        return mapPayment(Ecr17Response::parsePayment(pkt.payload));
    });
}

std::shared_ptr<Promise<ReversalResult>> HybridEcr17Client::reverse(const ReversalRequest& request) {
    return Promise<ReversalResult>::async([this, request]() -> ReversalResult {
        ensureConnected();
        auto payload = Ecr17Protocol::buildReversalMessage(
            config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
            request.stan.value_or("000000"));
        auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
        return mapReversal(Ecr17Response::parsePayment(pkt.payload));
    });
}

std::shared_ptr<Promise<PreAuthResult>> HybridEcr17Client::preAuth(const PreAuthRequest& request) {
    return Promise<PreAuthResult>::async([this, request]() -> PreAuthResult {
        ensureConnected();
        const bool tok = request.tokenization.has_value();
        auto payload = Ecr17Protocol::buildPreAuthMessage(
            config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
            static_cast<int>(request.amountCents), mapPaymentType(request.paymentType),
            request.cardAlreadyPresent.value_or(false), tok, request.receiptText.value_or(""));
        auto pkt = runTransaction(payload, request.tokenization, false);
        return mapPreAuth(Ecr17Response::parsePreAuth(pkt.payload));
    });
}

std::shared_ptr<Promise<PreAuthResult>> HybridEcr17Client::incrementalAuth(
    const IncrementalAuthRequest& request) {
    return Promise<PreAuthResult>::async([this, request]() -> PreAuthResult {
        ensureConnected();
        auto payload = Ecr17Protocol::buildIncrementalMessage(
            config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
            static_cast<int>(request.amountCents), request.originalPreAuthCode, false,
            request.receiptText.value_or(""));
        auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
        return mapPreAuth(Ecr17Response::parsePreAuth(pkt.payload));
    });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::preAuthClosure(
    const PreAuthClosureRequest& request) {
    return Promise<PaymentResult>::async([this, request]() -> PaymentResult {
        ensureConnected();
        auto payload = Ecr17Protocol::buildPreAuthClosureMessage(
            config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
            static_cast<int>(request.amountCents), request.originalPreAuthCode, false,
            request.receiptText.value_or(""));
        auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
        return mapPayment(Ecr17Response::parsePayment(pkt.payload));
    });
}

std::shared_ptr<Promise<CardVerificationResult>> HybridEcr17Client::verifyCard(
    const CardVerificationRequest& request) {
    return Promise<CardVerificationResult>::async([this, request]() -> CardVerificationResult {
        ensureConnected();
        const bool tok = request.tokenization.has_value();
        auto payload = Ecr17Protocol::buildCardVerificationMessage(
            config_.terminalId, cashRegisterIdOr(request.cashRegisterId),
            mapPaymentType(request.paymentType), tok);
        auto pkt = runTransaction(payload, request.tokenization, false);
        return mapCardVerify(Ecr17Response::parsePayment(pkt.payload));
    });
}

std::shared_ptr<Promise<CloseSessionResult>> HybridEcr17Client::closeSession() {
    return Promise<CloseSessionResult>::async([this]() -> CloseSessionResult {
        ensureConnected();
        auto payload = Ecr17Protocol::buildCloseSessionMessage(config_.terminalId, config_.cashRegisterId);
        auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
        return mapClose(Ecr17Response::parseClose(pkt.payload));
    });
}

std::shared_ptr<Promise<TotalsResult>> HybridEcr17Client::totals() {
    return Promise<TotalsResult>::async([this]() -> TotalsResult {
        ensureConnected();
        auto payload = Ecr17Protocol::buildTotalsMessage(config_.terminalId, config_.cashRegisterId);
        auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/true);
        return mapTotals(Ecr17Response::parseTotals(pkt.payload));
    });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::sendLastResult() {
    return Promise<PaymentResult>::async([this]() -> PaymentResult {
        ensureConnected();
        auto payload = Ecr17Protocol::buildSendLastResultMessage(config_.terminalId, config_.cashRegisterId);
        auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/true);
        return mapPayment(Ecr17Response::parsePayment(pkt.payload));
    });
}

std::shared_ptr<Promise<void>> HybridEcr17Client::enableEcrPrinting(bool enabled) {
    return Promise<void>::async([this, enabled]() {
        ensureConnected();
        runAckOnly(Ecr17Protocol::buildEnableEcrPrintMessage(config_.terminalId, enabled),
                   /*safeToRetry=*/true);
    });
}

std::shared_ptr<Promise<void>> HybridEcr17Client::reprint(bool toEcr) {
    return Promise<void>::async([this, toEcr]() {
        ensureConnected();
        runAckOnly(Ecr17Protocol::buildReprintMessage(config_.terminalId, toEcr),
                   /*safeToRetry=*/false);
    });
}

std::shared_ptr<Promise<VasResult>> HybridEcr17Client::vas(const std::string& xmlRequest) {
    return Promise<VasResult>::async([this, xmlRequest]() -> VasResult {
        ensureConnected();
        auto payload = Ecr17Protocol::buildVasMessage(config_.terminalId, config_.cashRegisterId, xmlRequest);
        auto pkt = runTransaction(payload, std::nullopt, /*safeToRetry=*/false);
        return mapVas(Ecr17Response::parseVas(pkt.payload));
    });
}

void HybridEcr17Client::setOnProgress(const std::function<void(const ProgressEvent&)>& callback) {
    onProgress_ = callback;
}

void HybridEcr17Client::setOnReceiptLine(const std::function<void(const ReceiptLine&)>& callback) {
    onReceiptLine_ = callback;
}

void HybridEcr17Client::setOnConnectionStateChange(const std::function<void(ConnectionState)>& callback) {
    onConnectionStateChange_ = callback;
}

}  // namespace margelo::nitro::ecr17
