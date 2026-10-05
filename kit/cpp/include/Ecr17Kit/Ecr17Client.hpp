#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "Ecr17Kit/ClientTypes.hpp"
#include "Ecr17Kit/Ecr17Response.hpp"
#include "Ecr17Kit/Ecr17Session.hpp"
#include "Ecr17Kit/Transport.hpp"

namespace padosoft::ecr17 {

/// An ECR17 terminal client: the public API of the Kit.
///
/// Every command auto-connects (probing the socket first, so a terminal that closed
/// the connection between transactions is reconnected BEFORE anything is sent), runs
/// one exchange and returns the parsed response. Commands are synchronous and
/// serialized: call them from a worker thread, never from a UI thread. Errors
/// (connection, timeout, NAK exhaustion, drop) throw std::runtime_error, invalid
/// fields std::invalid_argument.
///
/// ⚠️ Money safety: with `autoReconnect`, a command interrupted by a drop is re-sent
/// only if it is read-only. A financial command is never re-sent (the terminal may
/// already have charged the card): recover its outcome with sendLastResult().
class Ecr17Client {
   public:
    Ecr17Client(std::shared_ptr<Transport> transport, ClientConfig config);
    ~Ecr17Client();

    Ecr17Client(const Ecr17Client&) = delete;
    Ecr17Client& operator=(const Ecr17Client&) = delete;

    const ClientConfig& config() const { return config_; }

    // --- Connection ---
    void connect();
    void disconnect();
    bool isConnected();

    // --- Commands ---
    StatusResponse status();
    PaymentResponse pay(const PaymentRequest& request);
    PaymentResponse payExtended(const PaymentRequest& request);
    PaymentResponse reverse(const ReversalRequest& request);
    PreAuthResponse preAuth(const PreAuthRequest& request);
    PreAuthResponse incrementalAuth(const PreAuthFollowUpRequest& request);
    PaymentResponse preAuthClosure(const PreAuthFollowUpRequest& request);
    PaymentResponse verifyCard(const CardVerificationRequest& request);
    CloseResponse closeSession();
    TotalsResponse totals();
    PaymentResponse sendLastResult();
    void enableEcrPrinting(bool enabled);
    void reprint(bool toEcr);
    VasResponse vas(const std::string& xmlRequest);

    // --- Events (called on the thread that runs the command, or the transport's) ---
    void setOnProgress(std::function<void(const std::string&)> callback);
    void setOnReceiptLine(std::function<void(const std::string&)> callback);
    void setOnConnectionStateChange(std::function<void(ConnectionState)> callback);

   private:
    void ensureConnected();
    void emit(ConnectionState state);
    std::string cashRegisterIdOr(const std::optional<std::string>& override) const;
    DecodedPacket runTransaction(const std::string& mainPayload,
                                 const std::optional<TokenizationRequest>& tokenization,
                                 bool safeToRetry);
    void runAckOnly(const std::string& payload, bool safeToRetry);

    std::shared_ptr<Transport> transport_;
    ClientConfig config_;
    std::unique_ptr<Ecr17Session> session_;

    // Serializes protocol exchanges: commands share one session and RX buffer, so
    // two of them must not interleave on the wire (or ACK each other's frames).
    std::mutex txMutex_;

    std::mutex callbacksMutex_;
    std::function<void(const std::string&)> onProgress_;
    std::function<void(const std::string&)> onReceiptLine_;
    std::function<void(ConnectionState)> onConnectionStateChange_;
};

}  // namespace padosoft::ecr17
