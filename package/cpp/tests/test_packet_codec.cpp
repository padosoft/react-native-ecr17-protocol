#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "PacketCodec/PacketCodec.hpp"

using margelo::nitro::ecr17::DecodedPacket;
using margelo::nitro::ecr17::LrcMode;
using margelo::nitro::ecr17::PacketCodec;
using margelo::nitro::ecr17::PacketType;

namespace {
constexpr uint8_t kSoh = 0x01;
constexpr uint8_t kStx = 0x02;
constexpr uint8_t kEtx = 0x03;
constexpr uint8_t kEot = 0x04;
constexpr uint8_t kAck = 0x06;
constexpr uint8_t kNak = 0x15;
}  // namespace

TEST(PacketCodec, EncodeApplicationFramesStxPayloadEtxLrc) {
    PacketCodec codec(LrcMode::STD);
    auto frame = codec.encodeApplication("AB");
    ASSERT_EQ(frame.size(), 5u);
    EXPECT_EQ(frame[0], kStx);
    EXPECT_EQ(frame[1], 'A');
    EXPECT_EQ(frame[2], 'B');
    EXPECT_EQ(frame[3], kEtx);
    EXPECT_EQ(frame[4], 0x7C);  // 0x7F ^ 'A' ^ 'B'
}

TEST(PacketCodec, ApplicationRoundTrip) {
    for (LrcMode mode : {LrcMode::STX, LrcMode::STD, LrcMode::NOEXT, LrcMode::STX_NOEXT}) {
        PacketCodec codec(mode);
        const std::string payload = "123456780P0000065000";
        DecodedPacket decoded = codec.decode(codec.encodeApplication(payload));
        EXPECT_EQ(decoded.type, PacketType::APPLICATION);
        EXPECT_EQ(decoded.payload, payload);
        EXPECT_TRUE(decoded.validLrc);
    }
}

TEST(PacketCodec, ApplicationDetectsCorruptedLrc) {
    PacketCodec codec(LrcMode::STD);
    auto frame = codec.encodeApplication("HELLO");
    frame.back() ^= 0xFF;  // corrupt the LRC byte
    DecodedPacket decoded = codec.decode(frame);
    EXPECT_EQ(decoded.type, PacketType::APPLICATION);
    EXPECT_EQ(decoded.payload, "HELLO");
    EXPECT_FALSE(decoded.validLrc);
}

TEST(PacketCodec, EncodeControlFramesCtrlEtxLrc) {
    PacketCodec codec(LrcMode::STD);
    auto frame = codec.encodeControl(kAck);
    ASSERT_EQ(frame.size(), 3u);
    EXPECT_EQ(frame[0], kAck);
    EXPECT_EQ(frame[1], kEtx);
}

TEST(PacketCodec, DecodeAck) {
    PacketCodec codec(LrcMode::STD);
    DecodedPacket decoded = codec.decode({kAck});
    EXPECT_EQ(decoded.type, PacketType::ACK);
    EXPECT_TRUE(decoded.validLrc);
}

TEST(PacketCodec, DecodeNak) {
    PacketCodec codec(LrcMode::STD);
    DecodedPacket decoded = codec.decode({kNak});
    EXPECT_EQ(decoded.type, PacketType::NAK);
    EXPECT_TRUE(decoded.validLrc);
}

TEST(PacketCodec, DecodeEmptyIsUnknown) {
    PacketCodec codec(LrcMode::STD);
    DecodedPacket decoded = codec.decode({});
    EXPECT_EQ(decoded.type, PacketType::UNKNOWN);
    EXPECT_FALSE(decoded.validLrc);
}

// Regression: a lone SOH byte previously built a string from an inverted
// iterator range [begin()+1, end()-1) == [end(), begin()) -> UB/crash.
TEST(PacketCodec, DecodeLoneSohIsUnknownNotCrash) {
    PacketCodec codec(LrcMode::STD);
    DecodedPacket decoded = codec.decode({kSoh});
    EXPECT_EQ(decoded.type, PacketType::UNKNOWN);
    EXPECT_FALSE(decoded.validLrc);
}

TEST(PacketCodec, DecodeProgressUpdate) {
    PacketCodec codec(LrcMode::STD);
    std::vector<uint8_t> frame{kSoh};
    const std::string msg = "ELABORAZIONE...     ";  // 20 chars per spec
    frame.insert(frame.end(), msg.begin(), msg.end());
    frame.push_back(kEot);

    DecodedPacket decoded = codec.decode(frame);
    EXPECT_EQ(decoded.type, PacketType::PROGRESS);
    EXPECT_EQ(decoded.payload, msg);
}

TEST(PacketCodec, DecodeStxWithoutEtxIsUnknown) {
    PacketCodec codec(LrcMode::STD);
    DecodedPacket decoded = codec.decode({kStx, 'A', 'B'});
    EXPECT_EQ(decoded.type, PacketType::UNKNOWN);
    EXPECT_FALSE(decoded.validLrc);
}

// Regression: ETX present but no trailing LRC byte must not read past the end
// nor mistake ETX for the LRC.
TEST(PacketCodec, DecodeStxWithEtxButNoLrcIsUnknown) {
    PacketCodec codec(LrcMode::STD);
    DecodedPacket decoded = codec.decode({kStx, 'A', kEtx});
    EXPECT_EQ(decoded.type, PacketType::UNKNOWN);
    EXPECT_FALSE(decoded.validLrc);
}

TEST(PacketCodec, DecodeUnknownLeadByte) {
    PacketCodec codec(LrcMode::STD);
    DecodedPacket decoded = codec.decode({0x99, 0x00});
    EXPECT_EQ(decoded.type, PacketType::UNKNOWN);
    EXPECT_FALSE(decoded.validLrc);
}
