// Ecr17Client against FakeTransport: auto-connect, the proactive reconnect before a
// send, connection-state events and — above all — the money-safety rule: after a drop
// a financial command is NEVER re-sent, a read-only one may be.
//
// Until the core was split out, this logic lived in the Nitro client and only RetryPolicy was tested.

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ecr17/Ecr17Client.hpp"
#include "ecr17/PacketCodec.hpp"
#include "FakeTransport.hpp"

using namespace padosoft::ecr17;

namespace {

const std::string kTerminal = "12345678";

ClientConfig fastConfig(bool autoReconnect = false) {
    ClientConfig c;
    c.host = "192.0.2.10";
    c.port = 10001;
    c.connectionTimeoutMs = 1234;
    c.terminalId = kTerminal;
    c.cashRegisterId = "00000001";
    c.ackTimeoutMs = 40;
    c.responseTimeoutMs = 40;
    c.retryCount = 1;
    c.retryDelayMs = 1;
    c.autoReconnect = autoReconnect;
    return c;
}

// ACK + an application result with message code `code` and result "00".
std::vector<uint8_t> ackAndResult(char code, const std::string& data = std::string(60, '0')) {
    PacketCodec codec(LrcMode::STD);
    std::vector<uint8_t> bytes = codec.encodeControl(PacketCodec::ACK);
    const auto result = codec.encodeApplication(kTerminal + "0" + code + "00" + data);
    bytes.insert(bytes.end(), result.begin(), result.end());
    return bytes;
}

std::vector<uint8_t> ackOnly() { return PacketCodec(LrcMode::STD).encodeControl(PacketCodec::ACK); }

std::vector<uint8_t> progressFrame(const std::string& msg20) {
    std::vector<uint8_t> f{0x01};
    f.insert(f.end(), msg20.begin(), msg20.end());
    f.push_back(0x04);
    return f;
}

struct Fixture {
    explicit Fixture(bool autoReconnect = false)
        : transport(std::make_shared<FakeTransport>()), client(transport, fastConfig(autoReconnect)) {
        client.setOnConnectionStateChange([this](ConnectionState s) { states.push_back(s); });
    }
    std::shared_ptr<FakeTransport> transport;
    Ecr17Client client;
    std::vector<ConnectionState> states;
};

// The application command code (byte 9 of the payload) of every request sent.
std::vector<char> sentCommands(const FakeTransport& t) {
    std::vector<char> codes;
    for (const auto& frame : t.sentFrames()) {
        if (frame.size() > 10 && frame.front() == FakeTransport::STX) codes.push_back(static_cast<char>(frame[10]));
    }
    return codes;
}

}  // namespace

TEST(Client, ConnectsWithTheConfiguredEndpointBeforeTheFirstCommand) {
    Fixture f;
    f.transport->enqueueResponse(ackAndResult('E'));

    f.client.pay(PaymentRequest{.amountCents = 650});

    ASSERT_EQ(f.transport->connectAttempts().size(), 1u);
    EXPECT_EQ(f.transport->connectAttempts()[0].host, "192.0.2.10");
    EXPECT_EQ(f.transport->connectAttempts()[0].port, 10001);
    EXPECT_EQ(f.transport->connectAttempts()[0].timeoutMs, 1234);
    EXPECT_EQ(f.states, (std::vector<ConnectionState>{ConnectionState::Connecting, ConnectionState::Connected}));
}

TEST(Client, ReusesALiveConnection) {
    Fixture f;
    f.transport->enqueueResponse(ackAndResult('E'));
    f.transport->enqueueResponse(ackAndResult('E'));

    f.client.pay(PaymentRequest{.amountCents = 100});
    f.client.pay(PaymentRequest{.amountCents = 200});

    EXPECT_EQ(f.transport->connectAttempts().size(), 1u);
}

// ECR17/Nexi terminals close TCP between transactions: the probe must catch it
// and reconnect BEFORE the payment is sent, so the payment goes out exactly once.
TEST(Client, ReconnectsBeforeSendingWhenTheTerminalClosedTheIdleSocket) {
    Fixture f;
    f.client.connect();
    f.transport->dropSilently();
    f.transport->enqueueResponse(ackAndResult('E'));

    const auto result = f.client.pay(PaymentRequest{.amountCents = 650});

    EXPECT_EQ(result.outcome, Outcome::Ok);
    EXPECT_EQ(f.transport->connectAttempts().size(), 2u);
    EXPECT_EQ(f.transport->applicationRequestCount(), 1u);
}

TEST(Client, AFailedConnectThrowsAndReportsDisconnected) {
    Fixture f;
    f.transport->failNextConnects(1);

    EXPECT_THROW(f.client.status(), std::runtime_error);
    EXPECT_EQ(f.transport->applicationRequestCount(), 0u);
    EXPECT_EQ(f.states, (std::vector<ConnectionState>{ConnectionState::Connecting, ConnectionState::Disconnected}));
}

TEST(Client, DisconnectReportsDisconnected) {
    Fixture f;
    f.client.connect();
    f.client.disconnect();

    EXPECT_FALSE(f.client.isConnected());
    EXPECT_EQ(f.states.back(), ConnectionState::Disconnected);
}

// ⚠️ MONEY-CRITICAL: every command that can move money or change terminal state is
// sent once, even with autoReconnect, when the connection drops during it.
TEST(Client, AFinancialCommandIsNeverResentAfterADrop) {
    const std::vector<std::pair<const char*, std::function<void(Ecr17Client&)>>> financial = {
        {"pay", [](Ecr17Client& c) { c.pay(PaymentRequest{.amountCents = 650}); }},
        {"payExtended", [](Ecr17Client& c) { c.payExtended(PaymentRequest{.amountCents = 650}); }},
        {"reverse", [](Ecr17Client& c) { c.reverse(ReversalRequest{}); }},
        {"preAuth", [](Ecr17Client& c) { c.preAuth(PreAuthRequest{.amountCents = 650}); }},
        {"incrementalAuth",
         [](Ecr17Client& c) { c.incrementalAuth(PreAuthFollowUpRequest{.amountCents = 100, .originalPreAuthCode = "123456789"}); }},
        {"preAuthClosure",
         [](Ecr17Client& c) { c.preAuthClosure(PreAuthFollowUpRequest{.amountCents = 100, .originalPreAuthCode = "123456789"}); }},
        {"verifyCard", [](Ecr17Client& c) { c.verifyCard(CardVerificationRequest{}); }},
        {"closeSession", [](Ecr17Client& c) { c.closeSession(); }},
        {"vas", [](Ecr17Client& c) { c.vas("<xml/>"); }},
        {"reprint", [](Ecr17Client& c) { c.reprint(true); }},
    };
    for (const auto& [name, command] : financial) {
        SCOPED_TRACE(name);
        Fixture f(/*autoReconnect=*/true);
        f.client.connect();
        f.transport->disconnectOnNextRequest();
        // A reply is ready: if the command were replayed, it would succeed.
        f.transport->enqueueResponse(ackAndResult('E'));

        EXPECT_THROW(command(f.client), std::runtime_error);
        EXPECT_EQ(f.transport->applicationRequestCount(), 1u);
        // The socket itself is restored for the next command (and for sendLastResult).
        EXPECT_EQ(f.transport->connectAttempts().size(), 2u);
        EXPECT_TRUE(f.client.isConnected());
    }
}

TEST(Client, AReadOnlyCommandIsResentAfterADropWithAutoReconnect) {
    const std::vector<std::pair<const char*, std::function<void(Ecr17Client&)>>> readOnly = {
        {"status", [](Ecr17Client& c) { c.status(); }},
        {"totals", [](Ecr17Client& c) { c.totals(); }},
        {"sendLastResult", [](Ecr17Client& c) { c.sendLastResult(); }},
    };
    for (const auto& [name, command] : readOnly) {
        SCOPED_TRACE(name);
        Fixture f(/*autoReconnect=*/true);
        f.client.connect();
        f.transport->disconnectOnNextRequest();
        f.transport->enqueueResponse(ackAndResult('E'));

        EXPECT_NO_THROW(command(f.client));
        EXPECT_EQ(f.transport->applicationRequestCount(), 2u);
    }
}

TEST(Client, EnablingEcrPrintingIsResentAfterADrop) {
    Fixture f(/*autoReconnect=*/true);
    f.client.connect();
    f.transport->disconnectOnNextRequest();
    f.transport->enqueueResponse(ackOnly());

    EXPECT_NO_THROW(f.client.enableEcrPrinting(true));
    EXPECT_EQ(f.transport->applicationRequestCount(), 2u);
}

TEST(Client, WithoutAutoReconnectNothingIsResentOrReconnected) {
    Fixture f(/*autoReconnect=*/false);
    f.client.connect();
    f.transport->disconnectOnNextRequest();
    f.transport->enqueueResponse(ackAndResult('E'));

    EXPECT_THROW(f.client.totals(), std::runtime_error);
    EXPECT_EQ(f.transport->applicationRequestCount(), 1u);
    EXPECT_EQ(f.transport->connectAttempts().size(), 1u);
}

TEST(Client, AFailedReconnectSurfacesTheOriginalError) {
    Fixture f(/*autoReconnect=*/true);
    f.client.connect();
    f.transport->disconnectOnNextRequest();
    f.transport->failNextConnects(1);

    try {
        f.client.status();
        FAIL() << "expected the exchange error";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("disconnected"), std::string::npos) << error.what();
    }
    EXPECT_EQ(f.transport->applicationRequestCount(), 1u);
}

TEST(Client, TokenizationSendsTheAdditionalDataMessage) {
    Fixture f;
    PacketCodec codec(LrcMode::STD);
    f.transport->enqueueResponse(ackOnly());           // ACK of the payment request
    f.transport->enqueueResponse(ackAndResult('E'));   // ACK of 'U' + the result

    PaymentRequest request{.amountCents = 650};
    request.tokenization = TokenizationRequest{TokenizationService::Recurring, "CONTRACT1"};
    EXPECT_EQ(f.client.pay(request).outcome, Outcome::Ok);

    EXPECT_EQ(sentCommands(*f.transport), (std::vector<char>{'P', 'U'}));
}

TEST(Client, UsesTheRequestCashRegisterIdOverTheConfiguredOne) {
    Fixture f;
    f.transport->enqueueResponse(ackAndResult('E'));

    PaymentRequest request{.amountCents = 650};
    request.cashRegisterId = "99999999";
    f.client.pay(request);

    const auto& frame = f.transport->sentFrames().front();
    const std::string sent(frame.begin(), frame.end());
    EXPECT_NE(sent.find("99999999"), std::string::npos);
    EXPECT_EQ(sent.find("00000001"), std::string::npos);
}

TEST(Client, ForwardsProgressAndReceiptLines) {
    Fixture f;
    PacketCodec codec(LrcMode::STD);
    std::vector<uint8_t> reply = ackOnly();
    const auto progress = progressFrame("ATTENDERE PREGO     ");
    reply.insert(reply.end(), progress.begin(), progress.end());
    const auto receipt = codec.encodeApplication(kTerminal + "0SLINE 1");
    reply.insert(reply.end(), receipt.begin(), receipt.end());
    const auto result = ackAndResult('E');
    reply.insert(reply.end(), result.begin() + 1, result.end());  // the result without a second ACK
    f.transport->enqueueResponse(reply);

    std::vector<std::string> progressMessages;
    std::vector<std::string> receiptLines;
    f.client.setOnProgress([&](const std::string& m) { progressMessages.push_back(m); });
    f.client.setOnReceiptLine([&](const std::string& l) { receiptLines.push_back(l); });

    f.client.pay(PaymentRequest{.amountCents = 650});

    EXPECT_EQ(progressMessages, (std::vector<std::string>{"ATTENDERE PREGO     "}));
    ASSERT_EQ(receiptLines.size(), 1u);
    EXPECT_NE(receiptLines[0].find("LINE 1"), std::string::npos);
}

TEST(Client, PaymentTypeCodes) {
    EXPECT_EQ(paymentTypeCode(PaymentCardType::Auto), '0');
    EXPECT_EQ(paymentTypeCode(PaymentCardType::Debit), '1');
    EXPECT_EQ(paymentTypeCode(PaymentCardType::Credit), '2');
    EXPECT_EQ(paymentTypeCode(PaymentCardType::Other), '3');
}

TEST(Client, RequiresATransport) {
    EXPECT_THROW(Ecr17Client(nullptr, ClientConfig{}), std::invalid_argument);
}
