// Tests for the ECR17 response parsers. Payloads are synthesized field-by-field
// at the exact 1-based offsets from the spec response tables in docs/. Helpers
// guarantee each field's width so offsets can't drift from a miscounted space.

#include <gtest/gtest.h>

#include <string>

#include "Ecr17Response/Ecr17Response.hpp"

using namespace margelo::nitro::ecr17;

namespace {

// Left-justified field, right-padded with spaces to `width` (alpha fields).
std::string a(const std::string& value, size_t width) {
    std::string s = value;
    s.resize(width, ' ');
    return s;
}

// Right-justified numeric field, left-padded with '0' to `width`.
std::string n(const std::string& value, size_t width) {
    return std::string(width - value.size(), '0') + value;
}

}  // namespace

TEST(Response, PaymentPositive) {
    std::string p = a("12345678", 8) + "0" + "E" + "00" +  // header + result
                    n("4111111111", 19) +                  // PAN(19)
                    a("ICC", 3) + a("ABC123", 6) + "2111520" +  // txType authCode dateTime
                    "2" +                                   // cardType
                    a("ACQ", 11) + n("42", 6) + n("99", 6); // acquirer STAN idOnline
    PaymentResponse r = Ecr17Response::parsePayment(p);
    EXPECT_EQ(r.outcome, Outcome::Ok);
    EXPECT_EQ(r.resultCode, "00");
    EXPECT_EQ(r.pan, n("4111111111", 19));
    EXPECT_EQ(r.transactionType, "ICC");
    EXPECT_EQ(r.authCode, "ABC123");
    EXPECT_EQ(r.hostDateTime, "2111520");
    EXPECT_EQ(r.cardType, "2");
    EXPECT_EQ(r.acquirerId, "ACQ");
    EXPECT_EQ(r.stan, "000042");
    EXPECT_EQ(r.onlineId, "000099");
    EXPECT_FALSE(r.currency.applied);
}

TEST(Response, PaymentNegative) {
    std::string p = a("12345678", 8) + "0" + "E" + "01" + a("CARTA RIFIUTATA", 24) +
                    n("", 11) +           // reserved 37-47
                    "3" + a("AC2", 11) + n("7", 6) + n("3", 6);
    PaymentResponse r = Ecr17Response::parsePayment(p);
    EXPECT_EQ(r.outcome, Outcome::Ko);
    EXPECT_EQ(r.resultCode, "01");
    EXPECT_EQ(r.errorDescription, "CARTA RIFIUTATA");
    EXPECT_EQ(r.cardType, "3");
    EXPECT_EQ(r.stan, "000007");
}

TEST(Response, PaymentWithCurrencyExchange) {
    std::string base = a("12345678", 8) + "0" + "V" + "00" + n("4111111111", 19) + a("ICC", 3) +
                       a("ABC123", 6) + "2111520" + "2" + a("ACQ", 11) + n("42", 6) + n("99", 6);
    // actionCode(3) origAmount(8) flag(1) rate(8) ccy(3) amount(12) precision(1)
    std::string p = base + "000" + n("650", 8) + "1" + n("12345", 8) + "USD" + n("650", 12) + "2";
    PaymentResponse r = Ecr17Response::parsePayment(p);
    EXPECT_EQ(r.outcome, Outcome::Ok);
    EXPECT_TRUE(r.currency.applied);
    EXPECT_EQ(r.currency.rate, "00012345");
    EXPECT_EQ(r.currency.currencyCode, "USD");
    EXPECT_EQ(r.currency.amount, "000000000650");
    EXPECT_EQ(r.currency.precision, "2");
}

TEST(Response, Status) {
    std::string p = a("12345678", 8) + "0" + "s" + n("", 10) +  // reserved 11-20
                    "0102251530" + "2" + "V1.2.3";              // dateTime status sw
    StatusResponse r = Ecr17Response::parseStatus(p);
    EXPECT_EQ(r.terminalId, "12345678");
    EXPECT_EQ(r.dateTimeRaw, "0102251530");
    EXPECT_EQ(r.status, 2);
    EXPECT_EQ(r.softwareRelease, "V1.2.3");
}

TEST(Response, Totals) {
    std::string p = a("12345678", 8) + "0" + "T" + "00" + n("123456", 16) + n("", 6);
    TotalsResponse r = Ecr17Response::parseTotals(p);
    EXPECT_EQ(r.outcome, Outcome::Ok);
    EXPECT_EQ(r.posTotal, n("123456", 16));
}

TEST(Response, ClosePositive) {
    std::string p = a("12345678", 8) + "0" + "C" + "00" + n("1000", 16) + n("1000", 16);
    CloseResponse r = Ecr17Response::parseClose(p);
    EXPECT_EQ(r.outcome, Outcome::Ok);
    EXPECT_EQ(r.posTotal, n("1000", 16));
    EXPECT_EQ(r.hostTotal, n("1000", 16));
}

TEST(Response, CloseNegative) {
    std::string p = a("12345678", 8) + "0" + "C" + "01" + a("SBILANCIO", 19) + "100";
    CloseResponse r = Ecr17Response::parseClose(p);
    EXPECT_EQ(r.outcome, Outcome::Ko);
    EXPECT_EQ(r.errorDescription, "SBILANCIO");
    EXPECT_EQ(r.actionCode, "100");
}

TEST(Response, PreAuthPositive) {
    std::string p = a("12345678", 8) + "0" + "e" + "00" + n("4111111111", 19) + a("CLI", 3) +
                    a("AUTH01", 6) + n("50000", 8) + n("123", 9) + "000" + "2111520";
    PreAuthResponse r = Ecr17Response::parsePreAuth(p);
    EXPECT_EQ(r.outcome, Outcome::Ok);
    EXPECT_EQ(r.transactionType, "CLI");
    EXPECT_EQ(r.authCode, "AUTH01");
    EXPECT_EQ(r.preAuthorizedAmount, "00050000");
    EXPECT_EQ(r.preAuthCode, "000000123");
    EXPECT_EQ(r.hostDateTime, "2111520");
}

TEST(Response, Vas) {
    std::string xml =
        "<ecrres><p k=\"RESPID\">0</p><p k=\"RESPMSG\">OK-APPROVED</p>"
        "<p k=\"ORDER_ID\">ABC123</p></ecrres>";
    // header(10) reserved(4) concatFlag(1) idMessage(3) filler-to-pos27(8) xml
    std::string p = a("12345678", 8) + "0" + "K" + n("", 4) + "0" + "001" + n("", 8) + xml;
    VasResponse r = Ecr17Response::parseVas(p);
    EXPECT_FALSE(r.moreMessages);
    EXPECT_EQ(r.idMessage, "001");
    EXPECT_EQ(r.responseId, "0");
    EXPECT_EQ(r.responseMessage, "OK-APPROVED");
    EXPECT_EQ(r.orderId, "ABC123");
    EXPECT_EQ(r.rawXml, xml);
}

TEST(Response, DefensiveOnShortOrEmptyPayload) {
    PaymentResponse r = Ecr17Response::parsePayment("");
    EXPECT_EQ(r.outcome, Outcome::Unknown);
    EXPECT_EQ(r.resultCode, "");
    EXPECT_EQ(r.pan, "");

    StatusResponse s = Ecr17Response::parseStatus("123");  // truncated, must not crash
    EXPECT_EQ(s.status, -1);
}

TEST(Response, OutcomeMapping) {
    EXPECT_EQ(outcomeFromCode("00"), Outcome::Ok);
    EXPECT_EQ(outcomeFromCode("01"), Outcome::Ko);
    EXPECT_EQ(outcomeFromCode("05"), Outcome::CardNotPresent);
    EXPECT_EQ(outcomeFromCode("09"), Outcome::UnknownTag);
    EXPECT_EQ(outcomeFromCode("zz"), Outcome::Unknown);
}
