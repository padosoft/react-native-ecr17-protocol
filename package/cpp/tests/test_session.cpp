// Orchestration tests for Ecr17Session driven by the in-memory FakeTransport:
// ACK/NAK handshake, retransmission, timeouts, LRC-failure NAK, and progress /
// receipt forwarding. Timeouts are tiny so the suite stays fast.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "PacketCodec/PacketCodec.hpp"
#include "Session/Ecr17Session.hpp"
#include "Transport/FakeTransport.hpp"

using namespace margelo::nitro::ecr17;

namespace {

SessionConfig fastConfig() {
    SessionConfig c;
    c.lrcMode = LrcMode::STD;
    c.ackTimeoutMs = 40;
    c.responseTimeoutMs = 40;
    c.retryCount = 2;
    c.retryDelayMs = 1;
    return c;
}

void append(std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    a.insert(a.end(), b.begin(), b.end());
}

std::vector<uint8_t> progressFrame(const std::string& msg20) {
    std::vector<uint8_t> f{0x01};  // SOH
    f.insert(f.end(), msg20.begin(), msg20.end());
    f.push_back(0x04);  // EOT
    return f;
}

const std::string kResultPayload = "123456780E0000DATA";   // code 'E' at pos 10 -> result
const std::string kReceiptPayload = "123456780SLINE 1";    // code 'S' at pos 10 -> receipt

}  // namespace

TEST(Session, HappyPathReturnsResultAndAcks) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    std::vector<uint8_t> response = codec.encodeControl(PacketCodec::ACK);
    append(response, codec.encodeApplication(kResultPayload));
    t.enqueueResponse(response);

    Ecr17Session session(t, fastConfig());
    DecodedPacket result = session.exchange("123456780P...");
    EXPECT_EQ(result.type, PacketType::APPLICATION);
    EXPECT_TRUE(result.validLrc);
    EXPECT_EQ(result.payload, kResultPayload);
    EXPECT_EQ(t.applicationRequestCount(), 1u);

    // The session must have ACKed the received result frame.
    bool sentAck = false;
    for (const auto& f : t.sentFrames()) {
        if (!f.empty() && f.front() == PacketCodec::ACK) sentAck = true;
    }
    EXPECT_TRUE(sentAck);
}

TEST(Session, NakTriggersRetransmitThenSucceeds) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    t.enqueueResponse(codec.encodeControl(PacketCodec::NAK));  // reply to attempt 1
    std::vector<uint8_t> ok = codec.encodeControl(PacketCodec::ACK);
    append(ok, codec.encodeApplication(kResultPayload));
    t.enqueueResponse(ok);  // reply to retransmit

    Ecr17Session session(t, fastConfig());
    DecodedPacket result = session.exchange("123456780P...");
    EXPECT_EQ(result.payload, kResultPayload);
    EXPECT_EQ(t.applicationRequestCount(), 2u);  // initial + 1 retransmit
}

TEST(Session, AckTimeoutExhaustsRetriesThenThrows) {
    FakeTransport t;  // no responses queued
    Ecr17Session session(t, fastConfig());
    EXPECT_THROW(session.exchange("123456780P..."), std::runtime_error);
    // initial + retryCount retransmissions
    EXPECT_EQ(t.applicationRequestCount(), 3u);
}

TEST(Session, BadLrcResponseSendsNak) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    std::vector<uint8_t> response = codec.encodeControl(PacketCodec::ACK);
    auto bad = codec.encodeApplication(kResultPayload);
    bad.back() ^= 0xFF;  // corrupt LRC
    append(response, bad);
    t.enqueueResponse(response);

    Ecr17Session session(t, fastConfig());
    EXPECT_THROW(session.exchange("123456780P..."), std::runtime_error);  // no valid result

    bool sentNak = false;
    for (const auto& f : t.sentFrames()) {
        if (!f.empty() && f.front() == PacketCodec::NAK) sentNak = true;
    }
    EXPECT_TRUE(sentNak);
}

TEST(Session, ProgressMessagesForwarded) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    std::vector<uint8_t> response = codec.encodeControl(PacketCodec::ACK);
    append(response, progressFrame("ATTENDERE PREGO     "));
    append(response, codec.encodeApplication(kResultPayload));
    t.enqueueResponse(response);

    std::vector<std::string> progress;
    Ecr17Session session(t, fastConfig());
    session.setOnProgress([&](const std::string& m) { progress.push_back(m); });

    DecodedPacket result = session.exchange("123456780P...");
    EXPECT_EQ(result.payload, kResultPayload);
    ASSERT_EQ(progress.size(), 1u);
    EXPECT_EQ(progress[0], "ATTENDERE PREGO     ");
}

TEST(Session, ReceiptLinesForwardedBeforeResult) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    std::vector<uint8_t> response = codec.encodeControl(PacketCodec::ACK);
    append(response, codec.encodeApplication(kReceiptPayload));
    append(response, codec.encodeApplication(kResultPayload));
    t.enqueueResponse(response);

    std::vector<std::string> receipts;
    Ecr17Session session(t, fastConfig());
    session.setOnReceiptLine([&](const std::string& l) { receipts.push_back(l); });

    DecodedPacket result = session.exchange("123456780P...");
    EXPECT_EQ(result.payload, kResultPayload);
    ASSERT_EQ(receipts.size(), 1u);
    EXPECT_EQ(receipts[0], kReceiptPayload);
}

TEST(Session, ResponseTimeoutAfterAckThrows) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    t.enqueueResponse(codec.encodeControl(PacketCodec::ACK));  // ACK only, no result

    Ecr17Session session(t, fastConfig());
    EXPECT_THROW(session.exchange("123456780P..."), std::runtime_error);
}

TEST(Session, DisconnectDuringExchangeThrows) {
    FakeTransport t;
    t.disconnectOnNextRequest();
    Ecr17Session session(t, fastConfig());
    EXPECT_THROW(session.exchange("123456780P..."), std::runtime_error);
}

TEST(Session, SendAckOnlyReturnsOnAck) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    t.enqueueResponse(codec.encodeControl(PacketCodec::ACK));  // ACK, no app result
    Ecr17Session session(t, fastConfig());
    EXPECT_NO_THROW(session.sendAckOnly("123456780E1"));
    EXPECT_EQ(t.applicationRequestCount(), 1u);
}

TEST(Session, SendAckOnlyRetransmitsOnNak) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    t.enqueueResponse(codec.encodeControl(PacketCodec::NAK));
    t.enqueueResponse(codec.encodeControl(PacketCodec::ACK));
    Ecr17Session session(t, fastConfig());
    EXPECT_NO_THROW(session.sendAckOnly("123456780E0"));
    EXPECT_EQ(t.applicationRequestCount(), 2u);
}

TEST(Session, SendAckOnlyTimesOut) {
    FakeTransport t;  // no ACK
    Ecr17Session session(t, fastConfig());
    EXPECT_THROW(session.sendAckOnly("123456780E1"), std::runtime_error);
}

TEST(Session, ExchangeWithAdditionalDataSendsTwoRequests) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    t.enqueueResponse(codec.encodeControl(PacketCodec::ACK));  // ACK for the main 'P'
    std::vector<uint8_t> ok = codec.encodeControl(PacketCodec::ACK);
    append(ok, codec.encodeApplication(kResultPayload));
    t.enqueueResponse(ok);  // ACK for 'U' + the result

    Ecr17Session session(t, fastConfig());
    DecodedPacket result = session.exchangeWithAdditionalData("123456780P...", "123456780U...");
    EXPECT_EQ(result.payload, kResultPayload);
    EXPECT_EQ(t.applicationRequestCount(), 2u);  // P + U
}

TEST(Session, ReceiptDrainForwardsReceiptsAfterResult) {
    FakeTransport t;
    PacketCodec codec(LrcMode::STD);
    std::vector<uint8_t> response = codec.encodeControl(PacketCodec::ACK);
    append(response, codec.encodeApplication(kResultPayload));   // result first
    append(response, codec.encodeApplication(kReceiptPayload));  // then a receipt line
    t.enqueueResponse(response);

    SessionConfig cfg = fastConfig();
    cfg.receiptDrainMs = 30;  // drain receipts that follow the result
    std::vector<std::string> receipts;
    Ecr17Session session(t, cfg);
    session.setOnReceiptLine([&](const std::string& l) { receipts.push_back(l); });

    DecodedPacket result = session.exchange("123456780P...");
    EXPECT_EQ(result.payload, kResultPayload);
    ASSERT_EQ(receipts.size(), 1u);
    EXPECT_EQ(receipts[0], kReceiptPayload);
}
