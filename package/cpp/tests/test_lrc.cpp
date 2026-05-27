#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Lcr/Lcr.hpp"

using margelo::nitro::ecr17::Lrc;
using margelo::nitro::ecr17::LrcMode;

namespace {

constexpr uint8_t kBase = 0x7F;
constexpr uint8_t kStx = 0x02;
constexpr uint8_t kEtx = 0x03;

// Reference implementation kept intentionally independent from the production
// code, so the tests assert against first principles rather than a copy.
uint8_t reference(const std::vector<uint8_t>& payload, LrcMode mode) {
    uint8_t lrc = kBase;
    if (mode == LrcMode::STX || mode == LrcMode::STX_NOEXT) {
        lrc ^= kStx;
    }
    for (uint8_t b : payload) {
        lrc ^= b;
    }
    if (mode == LrcMode::STX || mode == LrcMode::NOEXT) {
        lrc ^= kEtx;
    }
    return lrc;
}

}  // namespace

TEST(Lrc, EmptyPayloadStdIsBase) {
    EXPECT_EQ(Lrc::compute(std::vector<uint8_t>{}, LrcMode::STD), kBase);
}

TEST(Lrc, EmptyPayloadStxFoldsStxAndEtx) {
    // 0x7F ^ 0x02 ^ 0x03 == 0x7E
    EXPECT_EQ(Lrc::compute(std::vector<uint8_t>{}, LrcMode::STX), 0x7E);
}

TEST(Lrc, KnownVectorAllModes) {
    const std::vector<uint8_t> payload{'A'};  // 0x41
    EXPECT_EQ(Lrc::compute(payload, LrcMode::STD), 0x3E);
    EXPECT_EQ(Lrc::compute(payload, LrcMode::STX), 0x3F);
    EXPECT_EQ(Lrc::compute(payload, LrcMode::NOEXT), 0x3D);
    EXPECT_EQ(Lrc::compute(payload, LrcMode::STX_NOEXT), 0x3C);
}

TEST(Lrc, MatchesReferenceForEveryMode) {
    const std::vector<uint8_t> payload{0x00, 0x7F, 0x55, 0xAA, 'Z', 0x10};
    for (LrcMode mode : {LrcMode::STX, LrcMode::STD, LrcMode::NOEXT, LrcMode::STX_NOEXT}) {
        EXPECT_EQ(Lrc::compute(payload, mode), reference(payload, mode));
    }
}

TEST(Lrc, StringAndVectorOverloadsAgree) {
    const std::string payload = "12345678P0";
    const std::vector<uint8_t> bytes(payload.begin(), payload.end());
    for (LrcMode mode : {LrcMode::STX, LrcMode::STD, LrcMode::NOEXT, LrcMode::STX_NOEXT}) {
        EXPECT_EQ(Lrc::compute(payload, mode), Lrc::compute(bytes, mode));
    }
}
