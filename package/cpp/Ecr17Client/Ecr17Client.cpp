#include "Ecr17Client.hpp"

#include <stdexcept>

namespace margelo::nitro::ecr17 {

void HybridEcr17Client::configure(const Ecr17Config& config) { config_ = config; }

Ecr17Config HybridEcr17Client::configuration() { return config_; }

// Helper macro so every not-yet-implemented method rejects the Promise with the
// same clear message instead of falling off the end of a non-void function.
#define UNIMPLEMENTED(name)                                                           \
    throw std::runtime_error("Ecr17Client::" name " is not implemented yet: "        \
                             "missing transport send/receive and response parsing")

std::shared_ptr<margelo::nitro::Promise<void>> HybridEcr17Client::connect() {
    return margelo::nitro::Promise<void>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("connect"); });
}

void HybridEcr17Client::disconnect() { /* no-op until transport is wired */ }

bool HybridEcr17Client::isConnected() { return false; }

std::shared_ptr<margelo::nitro::Promise<PosStatusResponse>> HybridEcr17Client::status() {
    return margelo::nitro::Promise<PosStatusResponse>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("status"); });
}

std::shared_ptr<margelo::nitro::Promise<PaymentResult>> HybridEcr17Client::pay(
    [[maybe_unused]] const PaymentRequest& request) {
    return margelo::nitro::Promise<PaymentResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("pay"); });
}

std::shared_ptr<margelo::nitro::Promise<PaymentResult>> HybridEcr17Client::payExtended(
    [[maybe_unused]] const PaymentRequest& request) {
    return margelo::nitro::Promise<PaymentResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("payExtended"); });
}

std::shared_ptr<margelo::nitro::Promise<ReversalResult>> HybridEcr17Client::reverse(
    [[maybe_unused]] const ReversalRequest& request) {
    return margelo::nitro::Promise<ReversalResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("reverse"); });
}

std::shared_ptr<margelo::nitro::Promise<PreAuthResult>> HybridEcr17Client::preAuth(
    [[maybe_unused]] const PreAuthRequest& request) {
    return margelo::nitro::Promise<PreAuthResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("preAuth"); });
}

std::shared_ptr<margelo::nitro::Promise<PreAuthResult>> HybridEcr17Client::incrementalAuth(
    [[maybe_unused]] const IncrementalAuthRequest& request) {
    return margelo::nitro::Promise<PreAuthResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("incrementalAuth"); });
}

std::shared_ptr<margelo::nitro::Promise<PaymentResult>> HybridEcr17Client::preAuthClosure(
    [[maybe_unused]] const PreAuthClosureRequest& request) {
    return margelo::nitro::Promise<PaymentResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("preAuthClosure"); });
}

std::shared_ptr<margelo::nitro::Promise<CardVerificationResult>> HybridEcr17Client::verifyCard(
    [[maybe_unused]] const CardVerificationRequest& request) {
    return margelo::nitro::Promise<CardVerificationResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("verifyCard"); });
}

std::shared_ptr<margelo::nitro::Promise<CloseSessionResult>> HybridEcr17Client::closeSession() {
    return margelo::nitro::Promise<CloseSessionResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("closeSession"); });
}

std::shared_ptr<margelo::nitro::Promise<TotalsResult>> HybridEcr17Client::totals() {
    return margelo::nitro::Promise<TotalsResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("totals"); });
}

std::shared_ptr<margelo::nitro::Promise<PaymentResult>> HybridEcr17Client::sendLastResult() {
    return margelo::nitro::Promise<PaymentResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("sendLastResult"); });
}

std::shared_ptr<margelo::nitro::Promise<void>> HybridEcr17Client::enableEcrPrinting(
    [[maybe_unused]] bool enabled) {
    return margelo::nitro::Promise<void>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("enableEcrPrinting"); });
}

std::shared_ptr<margelo::nitro::Promise<void>> HybridEcr17Client::reprint(
    [[maybe_unused]] bool toEcr) {
    return margelo::nitro::Promise<void>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("reprint"); });
}

std::shared_ptr<margelo::nitro::Promise<VasResult>> HybridEcr17Client::vas(
    [[maybe_unused]] const std::string& xmlRequest) {
    return margelo::nitro::Promise<VasResult>::async(
        []([[maybe_unused]] auto& res) { UNIMPLEMENTED("vas"); });
}

void HybridEcr17Client::setOnProgress(
    [[maybe_unused]] const std::function<void(const ProgressEvent&)>& callback) {}

void HybridEcr17Client::setOnReceiptLine(
    [[maybe_unused]] const std::function<void(const ReceiptLine&)>& callback) {}

void HybridEcr17Client::setOnConnectionStateChange(
    [[maybe_unused]] const std::function<void(const ConnectionState&)>& callback) {}

#undef UNIMPLEMENTED

}  // namespace margelo::nitro::ecr17