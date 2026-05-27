// End-to-end protocol *flow* tests, modelled on the documented Nexi ECR17
// sequences (basic payment, reversal/"annullamento", re-payment, status,
// NAK retransmission).
//
// These exercise the layers that exist today: request building
// (Ecr17Protocol) + physical framing (PacketCodec). Response *field* parsing
// is not implemented yet, so terminal responses are synthesized as spec-shaped
// payloads and asserted at the framing/classification level (packet type, LRC
// validity, message code position), not by parsed fields.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Ecr17Protocol/Ecr17Protocol.hpp"
#include "PacketCodec/PacketCodec.hpp"

using namespace margelo::nitro::ecr17;

namespace {

constexpr uint8_t kStx = 0x02;
constexpr uint8_t kEtx = 0x03;
constexpr uint8_t kSoh = 0x01;
constexpr uint8_t kEot = 0x04;
constexpr uint8_t kAck = 0x06;
constexpr uint8_t kNak = 0x15;

const std::string kTerminal = "12345678";
const std::string kCashReg = "00000001";

// Terminal-side framing of a progress-update packet: SOH + 20-char message + EOT.
std::vector<uint8_t> progressFrame(const std::string& msg20) {
    std::vector<uint8_t> f{kSoh};
    f.insert(f.end(), msg20.begin(), msg20.end());
    f.push_back(kEot);
    return f;
}

// Terminal-side framing of a spec-shaped result message ('E', result "00" = OK).
std::vector<uint8_t> okResultFrame(PacketCodec& codec) {
    std::string e = kTerminal + "0" + "E" + "00" + std::string(60, '0');
    return codec.encodeApplication(e);
}

}  // namespace

// Documented "Basic Payment Flow": request -> ACK -> progress -> result -> ACK.
TEST(Flow, BasicPayment) {
    PacketCodec codec(LrcMode::STD);

    // 1. ECR -> POS: payment request on the wire (STX .. ETX .. LRC).
    std::string req = Ecr17Protocol::buildPaymentMessage(kTerminal, kCashReg, 650);
    auto reqFrame = codec.encodeApplication(req);
    EXPECT_EQ(reqFrame.front(), kStx);
    EXPECT_EQ(reqFrame[reqFrame.size() - 2], kEtx);

    // POS validates and recovers the exact request payload.
    DecodedPacket atPos = codec.decode(reqFrame);
    EXPECT_EQ(atPos.type, PacketType::APPLICATION);
    EXPECT_TRUE(atPos.validLrc);
    EXPECT_EQ(atPos.payload, req);
    EXPECT_EQ(atPos.payload[9], 'P');

    // 2. POS -> ECR: physical ACK.
    EXPECT_EQ(codec.decode(codec.encodeControl(kAck)).type, PacketType::ACK);

    // 3. POS -> ECR: progress update while contacting the host (no LRC).
    DecodedPacket prog = codec.decode(progressFrame("ATTENDERE PREGO     "));
    EXPECT_EQ(prog.type, PacketType::PROGRESS);
    EXPECT_EQ(prog.payload, "ATTENDERE PREGO     ");

    // 4. POS -> ECR: positive result.
    DecodedPacket result = codec.decode(okResultFrame(codec));
    EXPECT_EQ(result.type, PacketType::APPLICATION);
    EXPECT_TRUE(result.validLrc);
    EXPECT_EQ(result.payload[9], 'E');
    EXPECT_EQ(result.payload.substr(10, 2), "00");

    // 5. ECR -> POS: ACK confirming receipt of the result.
    EXPECT_EQ(codec.decode(codec.encodeControl(kAck)).type, PacketType::ACK);
}

// Documented NAK handling: a NAK triggers retransmission of the same message
// (up to 3 times). Framing is deterministic, so the retransmitted bytes match.
TEST(Flow, NakTriggersIdenticalRetransmit) {
    PacketCodec codec(LrcMode::STD);
    std::string req = Ecr17Protocol::buildPaymentMessage(kTerminal, kCashReg, 1999);
    auto first = codec.encodeApplication(req);

    EXPECT_EQ(codec.decode(codec.encodeControl(kNak)).type, PacketType::NAK);

    auto retransmit = codec.encodeApplication(req);
    EXPECT_EQ(first, retransmit);
}

// Documented "Reversal Last Transaction" (annullamento), command 'S'.
TEST(Flow, ReversalAnnullamento) {
    PacketCodec codec(LrcMode::STD);

    std::string req = Ecr17Protocol::buildReversalMessage(kTerminal, kCashReg, "000123");
    ASSERT_EQ(req.size(), 26u);
    EXPECT_EQ(req[9], 'S');
    EXPECT_EQ(req.substr(18, 6), "000123");

    DecodedPacket atPos = codec.decode(codec.encodeApplication(req));
    EXPECT_EQ(atPos.type, PacketType::APPLICATION);
    EXPECT_TRUE(atPos.validLrc);
    EXPECT_EQ(atPos.payload, req);

    EXPECT_EQ(codec.decode(codec.encodeControl(kAck)).type, PacketType::ACK);
    DecodedPacket result = codec.decode(okResultFrame(codec));
    EXPECT_TRUE(result.validLrc);
    EXPECT_EQ(result.payload[9], 'E');
}

// Pay -> annullamento -> ripaga (re-payment with the corrected amount).
TEST(Flow, PayReverseRepay) {
    PacketCodec codec(LrcMode::STD);

    auto pay1 = codec.encodeApplication(Ecr17Protocol::buildPaymentMessage(kTerminal, kCashReg, 650));
    auto rev = codec.encodeApplication(Ecr17Protocol::buildReversalMessage(kTerminal, kCashReg));
    auto pay2 = codec.encodeApplication(Ecr17Protocol::buildPaymentMessage(kTerminal, kCashReg, 720));

    for (const auto* frame : {&pay1, &rev, &pay2}) {
        DecodedPacket d = codec.decode(*frame);
        EXPECT_EQ(d.type, PacketType::APPLICATION);
        EXPECT_TRUE(d.validLrc);
    }

    EXPECT_EQ(codec.decode(pay1).payload.substr(23, 8), "00000650");
    EXPECT_EQ(codec.decode(pay2).payload.substr(23, 8), "00000720");
    EXPECT_NE(pay1, pay2);  // the corrected re-payment differs on the wire
}

// Terminal status request/round-trip ('s').
TEST(Flow, StatusRequest) {
    PacketCodec codec(LrcMode::STD);
    std::string req = Ecr17Protocol::buildStatusMessage(kTerminal);
    EXPECT_EQ(req, "123456780s");

    DecodedPacket atPos = codec.decode(codec.encodeApplication(req));
    EXPECT_EQ(atPos.type, PacketType::APPLICATION);
    EXPECT_TRUE(atPos.validLrc);
    EXPECT_EQ(atPos.payload, req);
}
