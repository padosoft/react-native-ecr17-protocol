// Byte-layout tests for the full ECR17 command builder set (Phase 1).
// Positions are checked against the Nexi ECR17 spec tables in docs/.

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "Ecr17Protocol/Ecr17Protocol.hpp"

using margelo::nitro::ecr17::Ecr17Protocol;

namespace {
constexpr char kFieldSep = 0x1B;
const std::string T = "12345678";   // terminal id
const std::string C = "87654321";   // cash register id
}  // namespace

// --- Payment family (167 bytes) --------------------------------------------

TEST(Commands, ExtendedPaymentLayoutAndFlags) {
    std::string m = Ecr17Protocol::buildExtendedPaymentMessage(T, C, 650, '2', true, true, "ABC");
    ASSERT_EQ(m.size(), 167u);
    EXPECT_EQ(m.substr(0, 8), T);
    EXPECT_EQ(m[8], '0');
    EXPECT_EQ(m[9], 'X');
    EXPECT_EQ(m.substr(10, 8), C);
    EXPECT_EQ(m[18], '1');                       // withAdditionalData
    EXPECT_EQ(m.substr(19, 2), "00");
    EXPECT_EQ(m[21], '1');                       // cardAlreadyPresent
    EXPECT_EQ(m[22], '2');                       // payment type
    EXPECT_EQ(m.substr(23, 8), "00000650");      // amount
    EXPECT_EQ(m.substr(31, 125), std::string(125, ' '));  // text left-padding
    EXPECT_EQ(m.substr(156, 3), "ABC");          // text right-aligned
    EXPECT_EQ(m.substr(159, 8), "00000000");
}

TEST(Commands, PreAuthUsesCodeLowerP) {
    std::string m = Ecr17Protocol::buildPreAuthMessage(T, C, 1000);
    ASSERT_EQ(m.size(), 167u);
    EXPECT_EQ(m[9], 'p');
    EXPECT_EQ(m.substr(23, 8), "00001000");
}

TEST(Commands, PaymentDefaultsMatchBasicLayout) {
    std::string m = Ecr17Protocol::buildPaymentMessage(T, C, 650);
    ASSERT_EQ(m.size(), 167u);
    EXPECT_EQ(m[9], 'P');
    EXPECT_EQ(m[18], '0');  // no additional data by default
    EXPECT_EQ(m[21], '0');  // card not present by default
    EXPECT_EQ(m[22], '0');  // auto payment type by default
    EXPECT_EQ(m.substr(31, 128), std::string(128, ' '));
}

// --- Pre-auth integration / closure (176 bytes) ----------------------------

TEST(Commands, IncrementalLayout) {
    std::string m = Ecr17Protocol::buildIncrementalMessage(T, C, 1000, "123456789");
    ASSERT_EQ(m.size(), 176u);
    EXPECT_EQ(m[9], 'i');
    EXPECT_EQ(m.substr(19, 4), "0000");
    EXPECT_EQ(m.substr(23, 8), "00001000");
    EXPECT_EQ(m.substr(159, 9), "123456789");  // original pre-auth code
    EXPECT_EQ(m.substr(168, 8), "00000000");
}

TEST(Commands, PreAuthClosureLayout) {
    std::string m = Ecr17Protocol::buildPreAuthClosureMessage(T, C, 500, "000000042");
    ASSERT_EQ(m.size(), 176u);
    EXPECT_EQ(m[9], 'c');
    EXPECT_EQ(m.substr(159, 9), "000000042");
}

// --- Card verification (39 bytes) ------------------------------------------

TEST(Commands, CardVerificationLayout) {
    std::string m = Ecr17Protocol::buildCardVerificationMessage(T, C, '1');
    ASSERT_EQ(m.size(), 39u);
    EXPECT_EQ(m[9], 'H');
    EXPECT_EQ(m.substr(10, 8), C);
    EXPECT_EQ(m[18], '0');                       // no additional data
    EXPECT_EQ(m.substr(19, 2), "00");
    EXPECT_EQ(m[21], '0');                       // standard verification
    EXPECT_EQ(m[22], '1');                       // payment type
    EXPECT_EQ(m.substr(23, 16), std::string(16, '0'));
}

// --- Session commands (26 bytes) -------------------------------------------

TEST(Commands, CloseSessionLayout) {
    std::string m = Ecr17Protocol::buildCloseSessionMessage(T, C);
    ASSERT_EQ(m.size(), 26u);
    EXPECT_EQ(m[9], 'C');
    EXPECT_EQ(m.substr(10, 8), C);
    EXPECT_EQ(m[18], '0');
    EXPECT_EQ(m.substr(19, 7), std::string(7, '0'));
}

TEST(Commands, TotalsLayout) {
    std::string m = Ecr17Protocol::buildTotalsMessage(T, C);
    ASSERT_EQ(m.size(), 26u);
    EXPECT_EQ(m[9], 'T');
}

// --- Send last result (22 bytes) -------------------------------------------

TEST(Commands, SendLastResultLayout) {
    std::string m = Ecr17Protocol::buildSendLastResultMessage(T, C);
    ASSERT_EQ(m.size(), 22u);
    EXPECT_EQ(m[9], 'G');
    EXPECT_EQ(m.substr(19, 3), "000");
}

// --- Enable/disable ECR printing (11 bytes) --------------------------------

TEST(Commands, EnableEcrPrintLayout) {
    EXPECT_EQ(Ecr17Protocol::buildEnableEcrPrintMessage(T, true), "123456780E1");
    EXPECT_EQ(Ecr17Protocol::buildEnableEcrPrintMessage(T, false), "123456780E0");
}

// --- Reprint (22 bytes) -----------------------------------------------------

TEST(Commands, ReprintLayout) {
    std::string m = Ecr17Protocol::buildReprintMessage(T, true);
    ASSERT_EQ(m.size(), 22u);
    EXPECT_EQ(m[9], 'R');
    EXPECT_EQ(m[10], '1');  // send to ECR
    EXPECT_EQ(m[11], '0');  // ticket type default
    EXPECT_EQ(m.substr(12, 10), std::string(10, '0'));
}

// --- VAS (variable, length-prefixed) ---------------------------------------

TEST(Commands, VasLayoutAndLengthPrefix) {
    std::string m = Ecr17Protocol::buildVasMessage(T, C, "<x/>");
    ASSERT_EQ(m.size(), 30u);
    EXPECT_EQ(m[9], 'K');
    EXPECT_EQ(m.substr(10, 8), C);
    EXPECT_EQ(m.substr(18, 3), "000");
    EXPECT_EQ(m[21], '0');
    EXPECT_EQ(m.substr(22, 4), "0004");  // length of "<x/>"
    EXPECT_EQ(m.substr(26), "<x/>");
}

TEST(Commands, VasRejectsOversizedRequest) {
    EXPECT_THROW(Ecr17Protocol::buildVasMessage(T, C, std::string(1025, 'x')), std::invalid_argument);
}

// --- Additional data / tokenization 'U' ------------------------------------

TEST(Commands, AdditionalTagsLayout) {
    const std::string content = "0COF0TRK123|0FNZ03";  // 18 chars
    std::string m = Ecr17Protocol::buildAdditionalTagsMessage(T, content);
    ASSERT_EQ(m.size(), 36u + content.size() + 1u);
    EXPECT_EQ(m[9], 'U');
    EXPECT_EQ(m.substr(10, 6), "000000");
    EXPECT_EQ(m.substr(16, 2), "62");
    EXPECT_EQ(m.substr(18, 8), "DF8D01  ");  // left-justified, blank-filled
    EXPECT_EQ(m[26], '0');
    EXPECT_EQ(m.substr(27, 4), "0000");
    EXPECT_EQ(m.substr(31, 5), "00000");
    EXPECT_EQ(m.substr(36, content.size()), content);
    EXPECT_EQ(m.back(), kFieldSep);
}

TEST(Commands, AdditionalTagsRejectsBadContent) {
    EXPECT_THROW(Ecr17Protocol::buildAdditionalTagsMessage(T, ""), std::invalid_argument);
    EXPECT_THROW(Ecr17Protocol::buildAdditionalTagsMessage(T, std::string(101, 'x')),
                 std::invalid_argument);
}

TEST(Commands, TokenizationTagFormat) {
    EXPECT_EQ(Ecr17Protocol::formatTokenizationTag(false, "1666354841608"),
              "0COF0TRK1666354841608|0FNZ03");
    EXPECT_EQ(Ecr17Protocol::formatTokenizationTag(true, "ABC"), "0REC0TRKABC|0FNZ03");
    EXPECT_THROW(Ecr17Protocol::formatTokenizationTag(false, ""), std::invalid_argument);
    EXPECT_THROW(Ecr17Protocol::formatTokenizationTag(false, std::string(19, 'x')),
                 std::invalid_argument);
}

// --- Validation shared via leftPad -----------------------------------------

TEST(Commands, IncrementalRejectsOversizedPreAuthCode) {
    EXPECT_THROW(Ecr17Protocol::buildIncrementalMessage(T, C, 100, "1234567890"),
                 std::invalid_argument);  // 10 digits > 9-byte field
}

TEST(Commands, PreAuthRejectsNegativeAmount) {
    EXPECT_THROW(Ecr17Protocol::buildPreAuthMessage(T, C, -1), std::invalid_argument);
}
