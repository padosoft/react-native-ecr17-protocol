#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "Ecr17Protocol/Ecr17Protocol.hpp"

using margelo::nitro::ecr17::Ecr17Protocol;

// Field layout reference (1-based positions from the Nexi ECR17 spec, Payment
// request "P"):
//   1   8  Terminal ID
//   9   1  Reserved '0'
//   10  1  Message code 'P'
//   11  8  Cash register ID
//   19  1  Presence of additional data for GT
//   20  2  Reserved "00"
//   22  1  Start transaction when card already present
//   23  1  Payment type
//   24  8  Transaction amount (right aligned, '0' filled)
//   32  128 Text to print (' ' filled)
//   160 8  Reserved '0'  => total 167 bytes

TEST(Protocol, StatusMessageLayout) {
    std::string m = Ecr17Protocol::buildStatusMessage("42");
    ASSERT_EQ(m.size(), 10u);
    EXPECT_EQ(m.substr(0, 8), "00000042");  // terminal id, left-padded
    EXPECT_EQ(m[8], '0');                    // reserved
    EXPECT_EQ(m[9], 's');                    // lowercase message code per spec
}

TEST(Protocol, StatusMessageKeepsFullWidthId) {
    std::string m = Ecr17Protocol::buildStatusMessage("12345678");
    EXPECT_EQ(m, "123456780s");
}

TEST(Protocol, PaymentMessageIs167Bytes) {
    std::string m = Ecr17Protocol::buildPaymentMessage("1", "2", 650);
    EXPECT_EQ(m.size(), 167u);
}

TEST(Protocol, PaymentMessageFieldLayout) {
    std::string m = Ecr17Protocol::buildPaymentMessage("12345678", "87654321", 650);
    ASSERT_EQ(m.size(), 167u);
    EXPECT_EQ(m.substr(0, 8), "12345678");           // terminal id
    EXPECT_EQ(m[8], '0');                              // reserved
    EXPECT_EQ(m[9], 'P');                              // message code
    EXPECT_EQ(m.substr(10, 8), "87654321");           // cash register id
    EXPECT_EQ(m[18], '0');                             // presence of additional data
    EXPECT_EQ(m.substr(19, 2), "00");                 // reserved
    EXPECT_EQ(m[21], '0');                             // start-with-card
    EXPECT_EQ(m[22], '0');                             // payment type
    EXPECT_EQ(m.substr(23, 8), "00000650");           // amount, right aligned
    EXPECT_EQ(m.substr(31, 128), std::string(128, ' '));  // text field
    EXPECT_EQ(m.substr(159, 8), "00000000");          // trailing reserved
}

TEST(Protocol, PaymentMessageAmountMaxFits) {
    std::string m = Ecr17Protocol::buildPaymentMessage("1", "2", 99999999);
    EXPECT_EQ(m.substr(23, 8), "99999999");
}

TEST(Protocol, PaymentRejectsNegativeAmount) {
    EXPECT_THROW(Ecr17Protocol::buildPaymentMessage("1", "2", -1), std::invalid_argument);
}

TEST(Protocol, PaymentRejectsAmountOverflowingField) {
    // 9 digits does not fit the 8-byte amount field.
    EXPECT_THROW(Ecr17Protocol::buildPaymentMessage("1", "2", 100000000), std::invalid_argument);
}

TEST(Protocol, PaymentRejectsOversizedTerminalId) {
    EXPECT_THROW(Ecr17Protocol::buildPaymentMessage("123456789", "2", 650), std::invalid_argument);
}

TEST(Protocol, StatusRejectsOversizedTerminalId) {
    EXPECT_THROW(Ecr17Protocol::buildStatusMessage("123456789"), std::invalid_argument);
}

// Reversal request "S" layout (1-based spec positions):
//   1 8 Terminal ID · 9 1 Reserved · 10 1 'S' · 11 8 Cash register ID
//   19 6 STAN · 25 1 Presence of GT data · 26 1 Reserved  => 26 bytes
TEST(Protocol, ReversalMessageLayout) {
    std::string m = Ecr17Protocol::buildReversalMessage("12345678", "87654321", "000123");
    ASSERT_EQ(m.size(), 26u);
    EXPECT_EQ(m.substr(0, 8), "12345678");
    EXPECT_EQ(m[8], '0');
    EXPECT_EQ(m[9], 'S');
    EXPECT_EQ(m.substr(10, 8), "87654321");
    EXPECT_EQ(m.substr(18, 6), "000123");
    EXPECT_EQ(m[24], '0');
    EXPECT_EQ(m[25], '0');
}

TEST(Protocol, ReversalDefaultStanIsNoCheck) {
    std::string m = Ecr17Protocol::buildReversalMessage("12345678", "87654321");
    EXPECT_EQ(m.substr(18, 6), "000000");
}

TEST(Protocol, ReversalRejectsOversizedStan) {
    EXPECT_THROW(Ecr17Protocol::buildReversalMessage("1", "2", "1234567"), std::invalid_argument);
}
