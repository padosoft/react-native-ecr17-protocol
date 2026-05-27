#pragma once

#include <string>

namespace margelo::nitro::ecr17 {

class Ecr17Protocol {
   public:
    static std::string buildPaymentMessage(const std::string& terminalId,
                                           const std::string& cashRegisterId, int amountCents);

    static std::string buildStatusMessage(const std::string& terminalId);
};

}  // namespace margelo::nitro::ecr17