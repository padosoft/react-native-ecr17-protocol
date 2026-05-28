#include "Ecr17Response.hpp"

#include <algorithm>
#include <cctype>

namespace margelo::nitro::ecr17 {

namespace {

// 1-based field extractor. Returns "" if the field starts beyond the payload;
// clamps the length to whatever bytes are actually present (defensive parsing).
std::string at(const std::string& p, size_t pos1, size_t len) {
    if (pos1 == 0 || pos1 > p.size()) {
        return "";
    }
    const size_t i = pos1 - 1;
    return p.substr(i, std::min(len, p.size() - i));
}

std::string trimRight(const std::string& s) {
    const size_t end = s.find_last_not_of(' ');
    return end == std::string::npos ? std::string() : s.substr(0, end + 1);
}

// Extracts the value of a Nexi VAS XML param: <p k="KEY">value</p>.
std::string xmlValue(const std::string& xml, const std::string& key) {
    const std::string needle = "\"" + key + "\">";
    const size_t start = xml.find(needle);
    if (start == std::string::npos) {
        return "";
    }
    const size_t from = start + needle.size();
    const size_t end = xml.find('<', from);
    std::string value = xml.substr(from, end == std::string::npos ? std::string::npos : end - from);
    // trim surrounding whitespace
    const size_t a = value.find_first_not_of(" \t\r\n");
    const size_t b = value.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : value.substr(a, b - a + 1);
}

}  // namespace

Outcome outcomeFromCode(const std::string& code) {
    if (code == "00") return Outcome::Ok;
    if (code == "01") return Outcome::Ko;
    if (code == "05") return Outcome::CardNotPresent;
    if (code == "09") return Outcome::UnknownTag;
    return Outcome::Unknown;
}

PaymentResponse Ecr17Response::parsePayment(const std::string& p) {
    PaymentResponse r;
    const std::string code = at(p, 10, 1);  // message code 'E' (plain) or 'V' (DCC)
    const bool dcc = code == "V";

    r.resultCode = at(p, 11, 2);
    r.outcome = outcomeFromCode(r.resultCode);

    if (r.outcome == Outcome::Ko) {
        r.errorDescription = trimRight(at(p, 13, 24));
    } else {
        r.pan = at(p, 13, 19);
        r.transactionType = trimRight(at(p, 32, 3));
        r.authCode = trimRight(at(p, 35, 6));
        r.hostDateTime = at(p, 41, 7);
    }

    // Common to any response.
    r.cardType = at(p, 48, 1);
    r.acquirerId = trimRight(at(p, 49, 11));
    r.stan = at(p, 60, 6);
    r.onlineId = at(p, 66, 6);

    if (dcc) {
        r.currency.applied = at(p, 83, 1) == "1";
        r.currency.rate = at(p, 84, 8);
        r.currency.currencyCode = trimRight(at(p, 92, 3));
        r.currency.amount = at(p, 95, 12);
        r.currency.precision = at(p, 107, 1);
    }
    return r;
}

StatusResponse Ecr17Response::parseStatus(const std::string& p) {
    StatusResponse r;
    r.terminalId = at(p, 1, 8);
    r.dateTimeRaw = at(p, 21, 10);
    const std::string s = at(p, 31, 1);
    r.status = (!s.empty() && std::isdigit(static_cast<unsigned char>(s[0]))) ? s[0] - '0' : -1;
    r.softwareRelease = trimRight(at(p, 32, p.size()));
    return r;
}

TotalsResponse Ecr17Response::parseTotals(const std::string& p) {
    TotalsResponse r;
    r.resultCode = at(p, 11, 2);
    r.outcome = outcomeFromCode(r.resultCode);
    r.posTotal = at(p, 13, 16);
    return r;
}

CloseResponse Ecr17Response::parseClose(const std::string& p) {
    CloseResponse r;
    r.resultCode = at(p, 11, 2);
    r.outcome = outcomeFromCode(r.resultCode);
    if (r.outcome == Outcome::Ok) {
        r.posTotal = at(p, 13, 16);
        r.hostTotal = at(p, 29, 16);
    } else {
        r.errorDescription = trimRight(at(p, 13, 19));
        r.actionCode = at(p, 32, 3);
    }
    return r;
}

PreAuthResponse Ecr17Response::parsePreAuth(const std::string& p) {
    PreAuthResponse r;
    r.resultCode = at(p, 11, 2);
    r.outcome = outcomeFromCode(r.resultCode);
    if (r.outcome == Outcome::Ko) {
        r.errorDescription = trimRight(at(p, 13, 24));
        r.actionCode = at(p, 37, 3);
    } else {
        r.pan = at(p, 13, 19);
        r.transactionType = trimRight(at(p, 32, 3));
        r.authCode = trimRight(at(p, 35, 6));
        r.preAuthorizedAmount = at(p, 41, 8);
        r.preAuthCode = at(p, 49, 9);
        r.actionCode = at(p, 58, 3);
        r.hostDateTime = at(p, 61, 7);
    }
    // Common fields (per spec offsets; note pos 48 overlaps the positive amount
    // field in the spec table, so cardType is only meaningful for KO responses).
    r.cardType = at(p, 48, 1);
    r.acquirerId = trimRight(at(p, 72, 11));
    r.stan = at(p, 83, 6);
    r.onlineId = at(p, 89, 6);
    return r;
}

VasResponse Ecr17Response::parseVas(const std::string& p) {
    VasResponse r;
    r.moreMessages = at(p, 15, 1) == "1";
    r.idMessage = at(p, 16, 3);
    r.rawXml = at(p, 27, p.size());
    r.responseId = xmlValue(r.rawXml, "RESPID");
    r.responseMessage = xmlValue(r.rawXml, "RESPMSG");
    r.orderId = xmlValue(r.rawXml, "ORDER_ID");
    return r;
}

}  // namespace margelo::nitro::ecr17
