#pragma once

#include <string>

namespace margelo::nitro::ecr17 {

// Builds ECR17 application-message payloads (the bytes that go between STX and
// ETX). All fields are fixed-width and validated; values that overflow their
// field throw std::invalid_argument so a malformed frame is never produced.
//
// `paymentType` is the single request digit: '0' auto, '1' debit, '2' credit,
// '3' other. `cardAlreadyPresent` maps to field "start with card already
// inserted". `receiptText` is the contract/print text (max 128 chars).
class Ecr17Protocol {
   public:
    // --- Payment family (167 bytes): 'P' payment, 'X' extended, 'p' pre-auth ---
    static std::string buildPaymentMessage(const std::string& terminalId,
                                           const std::string& cashRegisterId, int amountCents,
                                           char paymentType = '0', bool cardAlreadyPresent = false,
                                           bool withAdditionalData = false,
                                           const std::string& receiptText = "");

    static std::string buildExtendedPaymentMessage(const std::string& terminalId,
                                                   const std::string& cashRegisterId, int amountCents,
                                                   char paymentType = '0',
                                                   bool cardAlreadyPresent = false,
                                                   bool withAdditionalData = false,
                                                   const std::string& receiptText = "");

    static std::string buildPreAuthMessage(const std::string& terminalId,
                                           const std::string& cashRegisterId, int amountCents,
                                           char paymentType = '0', bool cardAlreadyPresent = false,
                                           bool withAdditionalData = false,
                                           const std::string& receiptText = "");

    // --- Pre-auth integration/closure (176 bytes): 'i' incremental, 'c' closure ---
    static std::string buildIncrementalMessage(const std::string& terminalId,
                                               const std::string& cashRegisterId, int amountCents,
                                               const std::string& originalPreAuthCode,
                                               bool withAdditionalData = false,
                                               const std::string& receiptText = "");

    static std::string buildPreAuthClosureMessage(const std::string& terminalId,
                                                  const std::string& cashRegisterId, int amountCents,
                                                  const std::string& originalPreAuthCode,
                                                  bool withAdditionalData = false,
                                                  const std::string& receiptText = "");

    // --- Card verification 'H' (39 bytes) ---
    static std::string buildCardVerificationMessage(const std::string& terminalId,
                                                    const std::string& cashRegisterId,
                                                    char paymentType = '0',
                                                    bool withAdditionalData = false);

    // --- Session commands (26 bytes): 'C' close, 'T' totals ---
    static std::string buildCloseSessionMessage(const std::string& terminalId,
                                                const std::string& cashRegisterId,
                                                bool withAdditionalData = false);

    static std::string buildTotalsMessage(const std::string& terminalId,
                                          const std::string& cashRegisterId,
                                          bool withAdditionalData = false);

    // --- Send last result 'G' (22 bytes) ---
    static std::string buildSendLastResultMessage(const std::string& terminalId,
                                                  const std::string& cashRegisterId,
                                                  bool withAdditionalData = false);

    // --- Enable/disable ECR printing 'E' (11 bytes) ---
    static std::string buildEnableEcrPrintMessage(const std::string& terminalId, bool enabled);

    // --- Reprint ticket 'R' (22 bytes) ---
    static std::string buildReprintMessage(const std::string& terminalId, bool toEcr,
                                           char ticketType = '0');

    // --- Status 's' (10 bytes) ---
    static std::string buildStatusMessage(const std::string& terminalId);

    // --- Reversal 'S' (26 bytes); stan "000000" = reverse last, no STAN check ---
    static std::string buildReversalMessage(const std::string& terminalId,
                                            const std::string& cashRegisterId,
                                            const std::string& stan = "000000");

    // --- VAS 'K' (variable, length-prefixed XML, max 1024) ---
    static std::string buildVasMessage(const std::string& terminalId, const std::string& ecrId,
                                       const std::string& xmlRequest);

    // --- Additional data for GT / tokenization 'U' (variable) ---
    // `tagContent` is the privative TAG content (1..100 chars), terminated with
    // 0x1B by this builder. Use formatTokenizationTag() to produce it.
    static std::string buildAdditionalTagsMessage(const std::string& terminalId,
                                                  const std::string& tagContent,
                                                  const std::string& isoField = "62",
                                                  const std::string& tagNumber = "DF8D01");

    // Formats the TAG 5 content for tokenization (Intesa-style mapping):
    //   "0COF0TRK<contract>|0FNZ03" (unscheduled/one-click)
    //   "0REC0TRK<contract>|0FNZ03" (recurring)
    // `recurring` selects 0REC vs 0COF.
    static std::string formatTokenizationTag(bool recurring, const std::string& contractCode);
};

}  // namespace margelo::nitro::ecr17
