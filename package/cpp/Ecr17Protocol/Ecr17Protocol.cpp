#include "Ecr17Protocol.hpp"

namespace margelo::nitro::ecr17 {

static std::string leftPad(const std::string& value, size_t size, char ch = '0') {
    if (value.size() >= size) {
        return value;
    }

    return std::string(size - value.size(), ch) + value;
}

std::string Ecr17Protocol::buildPaymentMessage(const std::string& terminalId,
                                               const std::string& cashRegisterId, int amountCents) {
    std::string m;

    m += leftPad(terminalId, 8);
    m += "0";
    m += "P";
    m += leftPad(cashRegisterId, 8);
    m += "0";
    m += "00";
    m += "0";
    m += "0";

    m += leftPad(std::to_string(amountCents), 8);

    m += std::string(128, ' ');

    m += "00000000";

    return m;
}

std::string Ecr17Protocol::buildStatusMessage(const std::string& terminalId) {
    std::string m;

    m += leftPad(terminalId, 8);
    m += "0";
    m += "s";

    return m;
}

}  // namespace margelo::nitro::ecr17