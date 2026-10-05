#include "HybridEcr17Client.hpp"

#include <NitroModules/HybridObjectRegistry.hpp>

#include <chrono>
#include <climits>
#include <cmath>
#include <ctime>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "Transport/NativeTransportAdapter.hpp"

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

namespace core = padosoft::ecr17;

namespace {

// Runs `fn` on Android under fbjni's ThreadScope::WithClassLoader, which attaches
// the current thread to the JVM AND installs fbjni's cached app class loader for
// the duration — so every JNI FindClass inside (including NitroModules' lazy
// ArrayBuffer lookup) resolves app classes, not the system loader. Elsewhere `fn`
// is just called. Returns whatever `fn` returns (incl. void) and propagates
// exceptions: WithClassLoader takes a `std::function<void()>`, so on Android the
// result is captured in a local and any exception via std::exception_ptr, then
// rethrown after the scope.
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

// ---------- JS -> core ----------

// JS numbers are doubles. Out-of-range or NaN would be undefined behaviour in a
// plain cast: those throw instead. Fractions are truncated, as before.
int toInt(double value, const char* field) {
    if (!std::isfinite(value) || value < static_cast<double>(INT_MIN) || value > static_cast<double>(INT_MAX)) {
        throw std::invalid_argument(std::string("ECR17: ") + field + " is not a valid number");
    }
    return static_cast<int>(value);
}

core::LrcMode toCore(LrcMode mode) {
    switch (mode) {
        case LrcMode::STX: return core::LrcMode::STX;
        case LrcMode::NOEXT: return core::LrcMode::NOEXT;
        case LrcMode::STX_NOEXT: return core::LrcMode::STX_NOEXT;
        case LrcMode::STD: break;
    }
    return core::LrcMode::STD;
}

core::ClientConfig toCore(const Ecr17Config& c) {
    core::ClientConfig k;
    k.host = c.host;
    k.port = toInt(c.port.value_or(10000), "port");
    k.connectionTimeoutMs = toInt(c.connectionTimeoutMs.value_or(5000), "connectionTimeoutMs");
    k.terminalId = c.terminalId;
    k.cashRegisterId = c.cashRegisterId;
    k.lrcMode = toCore(c.lrcMode.value_or(LrcMode::STD));
    k.ackTimeoutMs = toInt(c.ackTimeoutMs.value_or(2000), "ackTimeoutMs");
    k.responseTimeoutMs = toInt(c.responseTimeoutMs.value_or(60000), "responseTimeoutMs");
    k.retryCount = toInt(c.retryCount.value_or(3), "retryCount");
    k.retryDelayMs = toInt(c.retryDelayMs.value_or(200), "retryDelayMs");
    k.receiptDrainMs = toInt(c.receiptDrainMs.value_or(0), "receiptDrainMs");
    k.autoReconnect = c.autoReconnect.value_or(false);
    return k;
}

core::PaymentCardType toCore(const std::optional<PaymentCardType>& type) {
    if (!type.has_value()) return core::PaymentCardType::Auto;
    switch (*type) {
        case PaymentCardType::DEBIT: return core::PaymentCardType::Debit;
        case PaymentCardType::CREDIT: return core::PaymentCardType::Credit;
        case PaymentCardType::OTHER: return core::PaymentCardType::Other;
        default: return core::PaymentCardType::Auto;
    }
}

std::optional<core::TokenizationRequest> toCore(const std::optional<TokenizationRequest>& t) {
    if (!t.has_value()) return std::nullopt;
    return core::TokenizationRequest{t->service == TokenizationService::RECURRING
                                        ? core::TokenizationService::Recurring
                                        : core::TokenizationService::UnscheduledOrOneClick,
                                    t->contractCode};
}

template <typename Request>
core::PaymentRequest toCorePayment(const Request& r) {
    core::PaymentRequest k;
    k.amountCents = toInt(r.amountCents, "amountCents");
    k.cashRegisterId = r.cashRegisterId;
    k.paymentType = toCore(r.paymentType);
    k.cardAlreadyPresent = r.cardAlreadyPresent.value_or(false);
    k.receiptText = r.receiptText.value_or("");
    k.tokenization = toCore(r.tokenization);
    return k;
}

template <typename Request>
core::PreAuthFollowUpRequest toCoreFollowUp(const Request& r) {
    core::PreAuthFollowUpRequest k;
    k.amountCents = toInt(r.amountCents, "amountCents");
    k.originalPreAuthCode = r.originalPreAuthCode;
    k.cashRegisterId = r.cashRegisterId;
    k.receiptText = r.receiptText.value_or("");
    return k;
}

ConnectionState toNitro(core::ConnectionState state) {
    switch (state) {
        case core::ConnectionState::Connecting: return ConnectionState::CONNECTING;
        case core::ConnectionState::Connected: return ConnectionState::CONNECTED;
        case core::ConnectionState::Disconnected: break;
    }
    return ConnectionState::DISCONNECTED;
}

// ---------- core -> JS ----------

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

TransactionOutcome mapOutcome(core::Outcome o) {
    switch (o) {
        case core::Outcome::Ok: return TransactionOutcome::OK;
        case core::Outcome::Ko: return TransactionOutcome::KO;
        case core::Outcome::CardNotPresent: return TransactionOutcome::CARDNOTPRESENT;
        case core::Outcome::UnknownTag: return TransactionOutcome::UNKNOWNTAG;
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

PaymentResult mapPayment(const core::PaymentResponse& p) {
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

ReversalResult mapReversal(const core::PaymentResponse& p) {
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

CardVerificationResult mapCardVerify(const core::PaymentResponse& p) {
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

PreAuthResult mapPreAuth(const core::PreAuthResponse& p) {
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

PosStatusResponse mapStatus(const core::StatusResponse& s) {
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

TotalsResult mapTotals(const core::TotalsResponse& t) {
    TotalsResult r;
    r.outcome = mapOutcome(t.outcome);
    r.resultCode = t.resultCode;
    r.posTotalCents = optNum(t.posTotal).value_or(0.0);
    return r;
}

CloseSessionResult mapClose(const core::CloseResponse& c) {
    CloseSessionResult r;
    r.outcome = mapOutcome(c.outcome);
    r.resultCode = c.resultCode;
    r.posTotalCents = optNum(c.posTotal);
    r.hostTotalCents = optNum(c.hostTotal);
    r.actionCode = optStr(c.actionCode);
    r.errorDescription = optStr(c.errorDescription);
    return r;
}

VasResult mapVas(const core::VasResponse& v) {
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
    std::shared_ptr<core::Ecr17Client> previous;
    {
        std::lock_guard<std::mutex> lock(clientMutex_);
        previous = std::move(client_);
    }
    // Close the old socket now, otherwise the native connection leaks until the
    // HybridObject is collected. Silently, as before: reconfiguring is not a drop.
    if (previous) {
        previous->setOnConnectionStateChange(nullptr);
        previous->disconnect();
    }
    // Create the transport NOW, on this (JS) thread. createHybridObject does a JNI
    // FindClass for the Kotlin transport, which resolves only against the app class
    // loader — and the JS thread has it. On a Nitro worker thread it would use the
    // system class loader and throw ClassNotFoundException. fbjni caches the resolved
    // jclass globally, so later calls from worker threads work.
    client();
}

Ecr17Config HybridEcr17Client::configuration() { return config_; }

std::shared_ptr<core::Ecr17Client> HybridEcr17Client::client() {
    std::lock_guard<std::mutex> lock(clientMutex_);
    if (client_) {
        return client_;
    }
    auto obj = HybridObjectRegistry::createHybridObject("Ecr17Transport");
    // HybridObject is a *virtual* base, so static_pointer_cast can't downcast
    // from it — must use dynamic_pointer_cast.
    auto transport = std::dynamic_pointer_cast<HybridEcr17TransportSpec>(obj);
    if (!transport) {
        throw std::runtime_error("ECR17: registry returned an incompatible Ecr17Transport object");
    }
    auto client = std::make_shared<core::Ecr17Client>(std::make_shared<NativeTransportAdapter>(transport),
                                                     toCore(config_));
    client->setOnProgress([this](const std::string& message) {
        if (onProgress_) onProgress_(ProgressEvent{message});
    });
    client->setOnReceiptLine([this](const std::string& line) {
        if (onReceiptLine_) onReceiptLine_(ReceiptLine{line});
    });
    client->setOnConnectionStateChange([this](core::ConnectionState state) {
        if (onConnectionStateChange_) onConnectionStateChange_(toNitro(state));
    });
    client_ = client;
    return client;
}

template <typename T, typename Command>
std::shared_ptr<Promise<T>> HybridEcr17Client::run(Command command) {
    return Promise<T>::async([this, command = std::move(command)]() -> T {
        // All transport JNI (connect, the probe, send incl. the ArrayBuffer lookup)
        // runs under the app class loader on Android; inline elsewhere.
        return runOnJvmThread([&]() -> T {
            auto c = client();
            return command(*c);
        });
    });
}

std::shared_ptr<Promise<void>> HybridEcr17Client::connect() {
    return run<void>([](core::Ecr17Client& c) { c.connect(); });
}

void HybridEcr17Client::disconnect() {
    std::shared_ptr<core::Ecr17Client> current;
    {
        std::lock_guard<std::mutex> lock(clientMutex_);
        current = client_;
    }
    if (current) {
        current->disconnect();  // emits DISCONNECTED
    } else if (onConnectionStateChange_) {
        onConnectionStateChange_(ConnectionState::DISCONNECTED);
    }
}

bool HybridEcr17Client::isConnected() {
    std::shared_ptr<core::Ecr17Client> current;
    {
        std::lock_guard<std::mutex> lock(clientMutex_);
        current = client_;
    }
    return current && current->isConnected();
}

std::shared_ptr<Promise<PosStatusResponse>> HybridEcr17Client::status() {
    return run<PosStatusResponse>([](core::Ecr17Client& c) { return mapStatus(c.status()); });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::pay(const PaymentRequest& request) {
    return run<PaymentResult>([request](core::Ecr17Client& c) { return mapPayment(c.pay(toCorePayment(request))); });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::payExtended(const PaymentRequest& request) {
    return run<PaymentResult>(
        [request](core::Ecr17Client& c) { return mapPayment(c.payExtended(toCorePayment(request))); });
}

std::shared_ptr<Promise<ReversalResult>> HybridEcr17Client::reverse(const ReversalRequest& request) {
    return run<ReversalResult>([request](core::Ecr17Client& c) {
        core::ReversalRequest k;
        k.cashRegisterId = request.cashRegisterId;
        k.stan = request.stan.value_or("000000");
        return mapReversal(c.reverse(k));
    });
}

std::shared_ptr<Promise<PreAuthResult>> HybridEcr17Client::preAuth(const PreAuthRequest& request) {
    return run<PreAuthResult>([request](core::Ecr17Client& c) { return mapPreAuth(c.preAuth(toCorePayment(request))); });
}

std::shared_ptr<Promise<PreAuthResult>> HybridEcr17Client::incrementalAuth(const IncrementalAuthRequest& request) {
    return run<PreAuthResult>(
        [request](core::Ecr17Client& c) { return mapPreAuth(c.incrementalAuth(toCoreFollowUp(request))); });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::preAuthClosure(const PreAuthClosureRequest& request) {
    return run<PaymentResult>(
        [request](core::Ecr17Client& c) { return mapPayment(c.preAuthClosure(toCoreFollowUp(request))); });
}

std::shared_ptr<Promise<CardVerificationResult>> HybridEcr17Client::verifyCard(const CardVerificationRequest& request) {
    return run<CardVerificationResult>([request](core::Ecr17Client& c) {
        core::CardVerificationRequest k;
        k.cashRegisterId = request.cashRegisterId;
        k.paymentType = toCore(request.paymentType);
        k.tokenization = toCore(request.tokenization);
        return mapCardVerify(c.verifyCard(k));
    });
}

std::shared_ptr<Promise<CloseSessionResult>> HybridEcr17Client::closeSession() {
    return run<CloseSessionResult>([](core::Ecr17Client& c) { return mapClose(c.closeSession()); });
}

std::shared_ptr<Promise<TotalsResult>> HybridEcr17Client::totals() {
    return run<TotalsResult>([](core::Ecr17Client& c) { return mapTotals(c.totals()); });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::sendLastResult() {
    return run<PaymentResult>([](core::Ecr17Client& c) { return mapPayment(c.sendLastResult()); });
}

std::shared_ptr<Promise<void>> HybridEcr17Client::enableEcrPrinting(bool enabled) {
    return run<void>([enabled](core::Ecr17Client& c) { c.enableEcrPrinting(enabled); });
}

std::shared_ptr<Promise<void>> HybridEcr17Client::reprint(bool toEcr) {
    return run<void>([toEcr](core::Ecr17Client& c) { c.reprint(toEcr); });
}

std::shared_ptr<Promise<VasResult>> HybridEcr17Client::vas(const std::string& xmlRequest) {
    return run<VasResult>([xmlRequest](core::Ecr17Client& c) { return mapVas(c.vas(xmlRequest)); });
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
