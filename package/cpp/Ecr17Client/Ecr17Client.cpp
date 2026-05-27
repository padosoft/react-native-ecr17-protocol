#include "Ecr17Client.hpp"

#include <functional>
#include <stdexcept>
#include <string>

namespace margelo::nitro::ecr17 {

using margelo::nitro::Promise;

void HybridEcr17Client::configure(const Ecr17Config& config) { config_ = config; }

Ecr17Config HybridEcr17Client::configuration() { return config_; }

// Phase 0 leaves the operations as rejecting stubs: the spec/API is in place but
// the transport + orchestration + parsing land in later phases. Each command
// returns a Promise that rejects with a clear message (instead of crashing).
//
// Promise<T>::async takes a `std::function<T()>`: the lambda takes no arguments
// and returns T (or throws, which rejects the promise).
namespace {
[[noreturn]] void notImplemented(const char* method) {
    throw std::runtime_error(std::string("Ecr17Client::") + method +
                             " is not implemented yet: transport/orchestration/parsing pending");
}
}  // namespace

std::shared_ptr<Promise<void>> HybridEcr17Client::connect() {
    return Promise<void>::async([]() { notImplemented("connect"); });
}

void HybridEcr17Client::disconnect() { /* no-op until transport is wired */ }

bool HybridEcr17Client::isConnected() { return false; }

std::shared_ptr<Promise<PosStatusResponse>> HybridEcr17Client::status() {
    return Promise<PosStatusResponse>::async([]() -> PosStatusResponse { notImplemented("status"); });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::pay(const PaymentRequest&) {
    return Promise<PaymentResult>::async([]() -> PaymentResult { notImplemented("pay"); });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::payExtended(const PaymentRequest&) {
    return Promise<PaymentResult>::async([]() -> PaymentResult { notImplemented("payExtended"); });
}

std::shared_ptr<Promise<ReversalResult>> HybridEcr17Client::reverse(const ReversalRequest&) {
    return Promise<ReversalResult>::async([]() -> ReversalResult { notImplemented("reverse"); });
}

std::shared_ptr<Promise<PreAuthResult>> HybridEcr17Client::preAuth(const PreAuthRequest&) {
    return Promise<PreAuthResult>::async([]() -> PreAuthResult { notImplemented("preAuth"); });
}

std::shared_ptr<Promise<PreAuthResult>> HybridEcr17Client::incrementalAuth(const IncrementalAuthRequest&) {
    return Promise<PreAuthResult>::async([]() -> PreAuthResult { notImplemented("incrementalAuth"); });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::preAuthClosure(const PreAuthClosureRequest&) {
    return Promise<PaymentResult>::async([]() -> PaymentResult { notImplemented("preAuthClosure"); });
}

std::shared_ptr<Promise<CardVerificationResult>> HybridEcr17Client::verifyCard(const CardVerificationRequest&) {
    return Promise<CardVerificationResult>::async(
        []() -> CardVerificationResult { notImplemented("verifyCard"); });
}

std::shared_ptr<Promise<CloseSessionResult>> HybridEcr17Client::closeSession() {
    return Promise<CloseSessionResult>::async([]() -> CloseSessionResult { notImplemented("closeSession"); });
}

std::shared_ptr<Promise<TotalsResult>> HybridEcr17Client::totals() {
    return Promise<TotalsResult>::async([]() -> TotalsResult { notImplemented("totals"); });
}

std::shared_ptr<Promise<PaymentResult>> HybridEcr17Client::sendLastResult() {
    return Promise<PaymentResult>::async([]() -> PaymentResult { notImplemented("sendLastResult"); });
}

std::shared_ptr<Promise<void>> HybridEcr17Client::enableEcrPrinting(bool) {
    return Promise<void>::async([]() { notImplemented("enableEcrPrinting"); });
}

std::shared_ptr<Promise<void>> HybridEcr17Client::reprint(bool) {
    return Promise<void>::async([]() { notImplemented("reprint"); });
}

std::shared_ptr<Promise<VasResult>> HybridEcr17Client::vas(const std::string&) {
    return Promise<VasResult>::async([]() -> VasResult { notImplemented("vas"); });
}

void HybridEcr17Client::setOnProgress(const std::function<void(const ProgressEvent&)>&) {}

void HybridEcr17Client::setOnReceiptLine(const std::function<void(const ReceiptLine&)>&) {}

void HybridEcr17Client::setOnConnectionStateChange(const std::function<void(ConnectionState)>&) {}

}  // namespace margelo::nitro::ecr17
