#pragma once

#include <NitroModules/Promise.hpp>

#include <memory>

#include "HybridEcr17ClientSpec.hpp"
#include "HybridEcr17TransportSpec.hpp"
#include "Session/Ecr17Session.hpp"
#include "Transport/NativeTransportAdapter.hpp"

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
    // Lazily creates the native transport (via the Nitro registry), the adapter
    // and the session, and wires session events to the JS callbacks.
    void ensureInit();
    // Throws if not connected (commands require an open connection).
    void requireConnected();
    std::string cashRegisterIdOr(const std::optional<std::string>& override) const;

    Ecr17Config config_;

    std::shared_ptr<HybridEcr17TransportSpec> transport_;
    std::shared_ptr<NativeTransportAdapter> adapter_;
    std::unique_ptr<Ecr17Session> session_;

    std::function<void(const ProgressEvent&)> onProgress_{};
    std::function<void(const ReceiptLine&)> onReceiptLine_{};
    std::function<void(ConnectionState)> onConnectionStateChange_{};
};

}  // namespace margelo::nitro::ecr17
