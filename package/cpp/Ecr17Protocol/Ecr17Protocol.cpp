#include "Ecr17Protocol.hpp"

#include <stdexcept>

namespace margelo::nitro::ecr17 {

// Right-aligns `value` into a fixed-width field of `size` bytes, padding on the
// left with `ch`. ECR17 fields have a fixed length, so a value longer than the
// field would shift every following field and corrupt the frame: reject it
// instead of emitting a malformed message.
static std::string leftPad(const std::string& value, size_t size, char ch = '0') {
    if (value.size() > size) {
        throw std::invalid_argument("ECR17: value '" + value + "' exceeds fixed field width of " +
                                    std::to_string(size) + " bytes");
    }

    if (value.size() == size) {
        return value;
    }

    return std::string(size - value.size(), ch) + value;
}

std::string Ecr17Protocol::buildPaymentMessage(const std::string& terminalId,
                                               const std::string& cashRegisterId, int amountCents) {
    if (amountCents < 0) {
        throw std::invalid_argument("ECR17: amount must be non-negative");
    }

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