#pragma once

namespace margelo::nitro::ecr17 {

// Decides whether a command may be safely RE-SENT after an auto-reconnect.
//
// ⚠️ MONEY-CRITICAL INVARIANT: a financial command (safeToRetry == false) must
// NEVER be retried. If the connection drops after the terminal has processed the
// payment but before the response arrives, a blind re-send would charge the
// cardholder twice. Such cases are recovered by querying the terminal's last
// result (command 'G' / sendLastResult), NOT by retransmitting the request.
//
// Only read-only / idempotent commands (status, totals, sendLastResult,
// enable-printing) pass safeToRetry == true.
//
// Reconnecting the socket is a separate, always-safe action; this function only
// governs whether the *request* is replayed.
inline bool shouldRetryAfterReconnect(bool autoReconnect, bool transportDropped,
                                      bool safeToRetry) {
    return autoReconnect && transportDropped && safeToRetry;
}

}  // namespace margelo::nitro::ecr17
