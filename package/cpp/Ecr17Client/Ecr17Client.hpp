#pragma once

#include <NitroModules/Promise.hpp>

#include "HybridEcr17ClientSpec.hpp"

namespace margelo::nitro::ecr17 {

class HybridEcr17Client : public HybridEcr17ClientSpec {
   public:
    HybridEcr17Client() : HybridObject(TAG) {}

    // --- Configuration (synchronous) ---
    void configure(const Ecr17Config& config) override;
    Ecr17Config configuration() override;

    // --- Connection ---
    std::shared_ptr<margelo::nitro::Promise<void>> connect() override;
    void disconnect() override;
    bool isConnected() override;

    // --- Commands ---
    std::shared_ptr<margelo::nitro::Promise<PosStatusResponse>> status() override;
    std::shared_ptr<margelo::nitro::Promise<PaymentResult>> pay(const PaymentRequest& request) override;
    std::shared_ptr<margelo::nitro::Promise<PaymentResult>> payExtended(const PaymentRequest& request) override;
    std::shared_ptr<margelo::nitro::Promise<ReversalResult>> reverse(const ReversalRequest& request) override;
    std::shared_ptr<margelo::nitro::Promise<PreAuthResult>> preAuth(const PreAuthRequest& request) override;
    std::shared_ptr<margelo::nitro::Promise<PreAuthResult>> incrementalAuth(const IncrementalAuthRequest& request) override;
    std::shared_ptr<margelo::nitro::Promise<PaymentResult>> preAuthClosure(const PreAuthClosureRequest& request) override;
    std::shared_ptr<margelo::nitro::Promise<CardVerificationResult>> verifyCard(const CardVerificationRequest& request) override;
    std::shared_ptr<margelo::nitro::Promise<CloseSessionResult>> closeSession() override;
    std::shared_ptr<margelo::nitro::Promise<TotalsResult>> totals() override;
    std::shared_ptr<margelo::nitro::Promise<PaymentResult>> sendLastResult() override;
    std::shared_ptr<margelo::nitro::Promise<void>> enableEcrPrinting(bool enabled) override;
    std::shared_ptr<margelo::nitro::Promise<void>> reprint(bool toEcr) override;
    std::shared_ptr<margelo::nitro::Promise<VasResult>> vas(const std::string& xmlRequest) override;

    // --- Events ---
    void setOnProgress(const std::function<void(const ProgressEvent&)>& callback) override;
    void setOnReceiptLine(const std::function<void(const ReceiptLine&)>& callback) override;
    void setOnConnectionStateChange(const std::function<void(ConnectionState)>& callback) override;

   protected:
    Ecr17Config config_;
};

}  // namespace margelo::nitro::ecr17