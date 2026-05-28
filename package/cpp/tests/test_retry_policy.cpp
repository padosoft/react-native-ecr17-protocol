// Money-critical safety tests for the auto-reconnect retry decision.
// The terminal handles real payments, so a financial command must NEVER be
// blindly re-sent after a connection drop (double-charge risk).

#include <gtest/gtest.h>

#include "Session/RetryPolicy.hpp"

using margelo::nitro::ecr17::shouldRetryAfterReconnect;

// A financial command (safeToRetry == false) must never be retried, regardless
// of autoReconnect / drop state. This is the invariant that prevents double
// charging; recovery is via sendLastResult ('G'), not a re-send.
TEST(RetryPolicy, FinancialCommandIsNeverRetried) {
    EXPECT_FALSE(shouldRetryAfterReconnect(/*autoReconnect=*/true, /*dropped=*/true, /*safe=*/false));
    EXPECT_FALSE(shouldRetryAfterReconnect(/*autoReconnect=*/false, /*dropped=*/true, /*safe=*/false));
    EXPECT_FALSE(shouldRetryAfterReconnect(/*autoReconnect=*/true, /*dropped=*/false, /*safe=*/false));
    EXPECT_FALSE(shouldRetryAfterReconnect(/*autoReconnect=*/false, /*dropped=*/false, /*safe=*/false));
}

// A safe/idempotent command is retried ONLY when autoReconnect is on AND the
// transport actually dropped.
TEST(RetryPolicy, SafeCommandRetriedOnlyOnReconnectAfterDrop) {
    EXPECT_TRUE(shouldRetryAfterReconnect(/*autoReconnect=*/true, /*dropped=*/true, /*safe=*/true));
    EXPECT_FALSE(shouldRetryAfterReconnect(/*autoReconnect=*/false, /*dropped=*/true, /*safe=*/true));
    EXPECT_FALSE(shouldRetryAfterReconnect(/*autoReconnect=*/true, /*dropped=*/false, /*safe=*/true));
    EXPECT_FALSE(shouldRetryAfterReconnect(/*autoReconnect=*/false, /*dropped=*/false, /*safe=*/true));
}
