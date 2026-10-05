#pragma once

#include <NitroModules/Promise.hpp>

#include <ecr17/Ecr17Client.hpp>

#include <functional>
#include <memory>
#include <mutex>

#include "HybridEcr17ClientSpec.hpp"
#include "HybridEcr17TransportSpec.hpp"

namespace margelo::nitro::ecr17 {

// The JS-facing client: maps the Nitro types to @padosoft/ecr17's Ecr17Client (which owns
// the protocol, the auto-connect and the money-safe retry policy) and runs each
// command on a Nitro worker thread. No protocol logic lives here.
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
    // The core client for the current configuration, created on first use. Must run
    // on the JS thread the first time on Android (see configure()).
    std::shared_ptr<padosoft::ecr17::Ecr17Client> client();
    // Runs `command` against the current client on a Nitro worker thread.
    template <typename T, typename Command>
    std::shared_ptr<margelo::nitro::Promise<T>> run(Command command);

    Ecr17Config config_;

    // Guards client_: configure() can replace it while a command still holds the old one.
    std::mutex clientMutex_;
    std::shared_ptr<padosoft::ecr17::Ecr17Client> client_;

    std::function<void(const ProgressEvent&)> onProgress_{};
    std::function<void(const ReceiptLine&)> onReceiptLine_{};
    std::function<void(ConnectionState)> onConnectionStateChange_{};
};

}  // namespace margelo::nitro::ecr17
