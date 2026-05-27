#pragma once

#include <string>

namespace margelo::nitro::ecr17 {

class Ecr17Protocol {
   public:
    static std::string buildPaymentMessage(const std::string& terminalId,
                                           const std::string& cashRegisterId, int amountCents);

    static std::string buildStatusMessage(const std::string& terminalId);

    // Reversal ("annullamento") of the last transaction (message code 'S').
    // `stan` is the 6-digit STAN of the transaction to reverse; "000000"
    // (the default) tells the terminal to skip the STAN check and reverse the
    // last payment.
    static std::string buildReversalMessage(const std::string& terminalId,
                                            const std::string& cashRegisterId,
                                            const std::string& stan = "000000");
};

}  // namespace margelo::nitro::ecr17