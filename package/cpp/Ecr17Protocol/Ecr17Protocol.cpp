#include "Ecr17Protocol.hpp"

#include <stdexcept>

namespace margelo::nitro::ecr17 {

namespace {

constexpr char kReserved = '0';     // '0' (0x30) filler for reserved numeric fields
constexpr char kFieldSep = 0x1B;    // end-of-field for the privative TAG content

// Right-aligns `value` into a fixed-width field of `size` bytes, padding on the
// left with `ch`. ECR17 fields have a fixed length, so a value longer than the
// field would shift every following field and corrupt the frame: reject it.
std::string leftPad(const std::string& value, size_t size, char ch = kReserved) {
    if (value.size() > size) {
        throw std::invalid_argument("ECR17: value '" + value + "' exceeds fixed field width of " +
                                    std::to_string(size) + " bytes");
    }
    if (value.size() == size) {
        return value;
    }
    return std::string(size - value.size(), ch) + value;
}

// Left-aligns `value` into a fixed-width field, padding on the right with `ch`.
std::string rightPad(const std::string& value, size_t size, char ch = ' ') {
    if (value.size() > size) {
        throw std::invalid_argument("ECR17: value '" + value + "' exceeds fixed field width of " +
                                    std::to_string(size) + " bytes");
    }
    return value + std::string(size - value.size(), ch);
}

std::string amountField(int amountCents) {
    if (amountCents < 0) {
        throw std::invalid_argument("ECR17: amount must be non-negative");
    }
    return leftPad(std::to_string(amountCents), 8);
}

char flag(bool on) { return on ? '1' : '0'; }

// Shared 167-byte payment-family layout (codes 'P', 'X', 'p').
std::string buildPaymentLike(char code, const std::string& terminalId,
                             const std::string& cashRegisterId, int amountCents, char paymentType,
                             bool cardAlreadyPresent, bool withAdditionalData,
                             const std::string& receiptText) {
    std::string m;
    m += leftPad(terminalId, 8);                  // 1  : terminal id
    m += kReserved;                               // 9  : reserved
    m += code;                                    // 10 : message code
    m += leftPad(cashRegisterId, 8);              // 11 : cash register id
    m += flag(withAdditionalData);                // 19 : presence of additional GT data
    m += "00";                                    // 20 : reserved
    m += flag(cardAlreadyPresent);                // 22 : start-with-card-present
    m += paymentType;                             // 23 : payment type
    m += amountField(amountCents);                // 24 : amount (8)
    m += leftPad(receiptText, 128, ' ');          // 32 : receipt text (128)
    m += "00000000";                              // 160: reserved (8)
    return m;                                     // 167
}

// Shared 176-byte pre-auth integration/closure layout (codes 'i', 'c').
std::string buildPreAuthFollowUp(char code, const std::string& terminalId,
                                 const std::string& cashRegisterId, int amountCents,
                                 const std::string& originalPreAuthCode, bool withAdditionalData,
                                 const std::string& receiptText) {
    std::string m;
    m += leftPad(terminalId, 8);            // 1  : terminal id
    m += kReserved;                         // 9  : reserved
    m += code;                              // 10 : message code
    m += leftPad(cashRegisterId, 8);        // 11 : cash register id
    m += flag(withAdditionalData);          // 19 : presence of additional GT data
    m += "0000";                            // 20 : reserved (4)
    m += amountField(amountCents);          // 24 : amount (8)
    m += leftPad(receiptText, 128, ' ');    // 32 : receipt text (128)
    m += leftPad(originalPreAuthCode, 9);   // 160: original pre-auth code (9)
    m += "00000000";                        // 169: reserved (8)
    return m;                               // 176
}

// Shared 26-byte session command layout (codes 'C', 'T').
std::string buildSessionCommand(char code, const std::string& terminalId,
                                const std::string& cashRegisterId, bool withAdditionalData) {
    std::string m;
    m += leftPad(terminalId, 8);      // 1  : terminal id
    m += kReserved;                   // 9  : reserved
    m += code;                        // 10 : message code
    m += leftPad(cashRegisterId, 8);  // 11 : cash register id
    m += flag(withAdditionalData);    // 19 : presence of additional GT data
    m += "0000000";                   // 20 : reserved (7)
    return m;                         // 26
}

}  // namespace

std::string Ecr17Protocol::buildPaymentMessage(const std::string& terminalId,
                                               const std::string& cashRegisterId, int amountCents,
                                               char paymentType, bool cardAlreadyPresent,
                                               bool withAdditionalData,
                                               const std::string& receiptText) {
    return buildPaymentLike('P', terminalId, cashRegisterId, amountCents, paymentType,
                            cardAlreadyPresent, withAdditionalData, receiptText);
}

std::string Ecr17Protocol::buildExtendedPaymentMessage(const std::string& terminalId,
                                                       const std::string& cashRegisterId,
                                                       int amountCents, char paymentType,
                                                       bool cardAlreadyPresent,
                                                       bool withAdditionalData,
                                                       const std::string& receiptText) {
    return buildPaymentLike('X', terminalId, cashRegisterId, amountCents, paymentType,
                            cardAlreadyPresent, withAdditionalData, receiptText);
}

std::string Ecr17Protocol::buildPreAuthMessage(const std::string& terminalId,
                                               const std::string& cashRegisterId, int amountCents,
                                               char paymentType, bool cardAlreadyPresent,
                                               bool withAdditionalData,
                                               const std::string& receiptText) {
    return buildPaymentLike('p', terminalId, cashRegisterId, amountCents, paymentType,
                            cardAlreadyPresent, withAdditionalData, receiptText);
}

std::string Ecr17Protocol::buildIncrementalMessage(const std::string& terminalId,
                                                   const std::string& cashRegisterId,
                                                   int amountCents,
                                                   const std::string& originalPreAuthCode,
                                                   bool withAdditionalData,
                                                   const std::string& receiptText) {
    return buildPreAuthFollowUp('i', terminalId, cashRegisterId, amountCents, originalPreAuthCode,
                                withAdditionalData, receiptText);
}

std::string Ecr17Protocol::buildPreAuthClosureMessage(const std::string& terminalId,
                                                      const std::string& cashRegisterId,
                                                      int amountCents,
                                                      const std::string& originalPreAuthCode,
                                                      bool withAdditionalData,
                                                      const std::string& receiptText) {
    return buildPreAuthFollowUp('c', terminalId, cashRegisterId, amountCents, originalPreAuthCode,
                                withAdditionalData, receiptText);
}

std::string Ecr17Protocol::buildCardVerificationMessage(const std::string& terminalId,
                                                        const std::string& cashRegisterId,
                                                        char paymentType, bool withAdditionalData) {
    std::string m;
    m += leftPad(terminalId, 8);      // 1  : terminal id
    m += kReserved;                   // 9  : reserved
    m += 'H';                         // 10 : message code
    m += leftPad(cashRegisterId, 8);  // 11 : cash register id
    m += flag(withAdditionalData);    // 19 : presence of additional GT data
    m += "00";                        // 20 : reserved (2)
    m += '0';                         // 22 : standard card verification
    m += paymentType;                 // 23 : payment type
    m += "0000000000000000";          // 24 : reserved (16)
    return m;                         // 39
}

std::string Ecr17Protocol::buildCloseSessionMessage(const std::string& terminalId,
                                                    const std::string& cashRegisterId,
                                                    bool withAdditionalData) {
    return buildSessionCommand('C', terminalId, cashRegisterId, withAdditionalData);
}

std::string Ecr17Protocol::buildTotalsMessage(const std::string& terminalId,
                                              const std::string& cashRegisterId,
                                              bool withAdditionalData) {
    return buildSessionCommand('T', terminalId, cashRegisterId, withAdditionalData);
}

std::string Ecr17Protocol::buildSendLastResultMessage(const std::string& terminalId,
                                                      const std::string& cashRegisterId,
                                                      bool withAdditionalData) {
    std::string m;
    m += leftPad(terminalId, 8);      // 1  : terminal id
    m += kReserved;                   // 9  : reserved
    m += 'G';                         // 10 : message code
    m += leftPad(cashRegisterId, 8);  // 11 : cash register id
    m += flag(withAdditionalData);    // 19 : presence of additional GT data
    m += "000";                       // 20 : reserved (3)
    return m;                         // 22
}

std::string Ecr17Protocol::buildEnableEcrPrintMessage(const std::string& terminalId, bool enabled) {
    std::string m;
    m += leftPad(terminalId, 8);  // 1  : terminal id
    m += kReserved;               // 9  : reserved
    m += 'E';                     // 10 : message code
    m += flag(enabled);           // 11 : enable(1)/disable(0) printing on ECR
    return m;                     // 11
}

std::string Ecr17Protocol::buildReprintMessage(const std::string& terminalId, bool toEcr,
                                               char ticketType) {
    std::string m;
    m += leftPad(terminalId, 8);  // 1  : terminal id
    m += kReserved;               // 9  : reserved
    m += 'R';                     // 10 : message code
    m += flag(toEcr);             // 11 : 1 = send receipt to ECR, 0 = print on terminal
    m += ticketType;              // 12 : ticket type flag
    m += "0000000000";            // 13 : reserved (10)
    return m;                     // 22
}

std::string Ecr17Protocol::buildStatusMessage(const std::string& terminalId) {
    std::string m;
    m += leftPad(terminalId, 8);  // 1  : terminal id
    m += kReserved;               // 9  : reserved
    m += 's';                     // 10 : message code (lowercase per spec)
    return m;                     // 10
}

std::string Ecr17Protocol::buildReversalMessage(const std::string& terminalId,
                                                const std::string& cashRegisterId,
                                                const std::string& stan) {
    std::string m;
    m += leftPad(terminalId, 8);      // 1  : terminal id
    m += kReserved;                   // 9  : reserved
    m += 'S';                         // 10 : message code
    m += leftPad(cashRegisterId, 8);  // 11 : cash register id
    m += leftPad(stan, 6);            // 19 : STAN ("000000" = no check)
    m += kReserved;                   // 25 : presence of additional GT data
    m += kReserved;                   // 26 : reserved
    return m;                         // 26
}

std::string Ecr17Protocol::buildVasMessage(const std::string& terminalId, const std::string& ecrId,
                                           const std::string& xmlRequest) {
    if (xmlRequest.size() > 1024) {
        throw std::invalid_argument("ECR17: VAS request exceeds 1024 bytes");
    }
    std::string m;
    m += leftPad(terminalId, 8);                       // 1  : terminal id
    m += kReserved;                                    // 9  : reserved
    m += 'K';                                          // 10 : message code
    m += leftPad(ecrId, 8);                            // 11 : ECR identifier
    m += "000";                                        // 19 : reserved (3)
    m += kReserved;                                    // 22 : reserved (1)
    m += leftPad(std::to_string(xmlRequest.size()), 4);// 23 : VAS request length (4)
    m += xmlRequest;                                   // 27 : VAS request (XML)
    return m;
}

std::string Ecr17Protocol::buildAdditionalTagsMessage(const std::string& terminalId,
                                                      const std::string& tagContent,
                                                      const std::string& isoField,
                                                      const std::string& tagNumber) {
    if (tagContent.empty() || tagContent.size() > 100) {
        throw std::invalid_argument("ECR17: additional TAG content must be 1..100 chars");
    }
    std::string m;
    m += leftPad(terminalId, 8);       // 1  : terminal id
    m += kReserved;                    // 9  : reserved
    m += 'U';                          // 10 : message code
    m += "000000";                     // 11 : payment type (6) -> standard payment
    m += leftPad(isoField, 2);         // 17 : ISO field number (e.g. "62")
    m += rightPad(tagNumber, 8);       // 19 : TAG number, left-justified, blank-filled
    m += kReserved;                    // 27 : reserved (1)
    m += "0000";                       // 28 : exclusive TAG index bytemap (none to send to GT)
    m += "00000";                      // 32 : reserved (5)
    m += tagContent;                   // 37 : privative TAG content (1..100)
    m += kFieldSep;                    //      end-of-field (0x1B)
    return m;
}

std::string Ecr17Protocol::formatTokenizationTag(bool recurring, const std::string& contractCode) {
    if (contractCode.empty() || contractCode.size() > 18) {
        throw std::invalid_argument("ECR17: tokenization contract code must be 1..18 chars");
    }
    const std::string service = recurring ? "0REC" : "0COF";
    return service + "0TRK" + contractCode + "|0FNZ03";
}

}  // namespace margelo::nitro::ecr17
