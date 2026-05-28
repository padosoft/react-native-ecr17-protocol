// OPT-IN real-terminal integration test.
//
// Skipped unless ECR17_TERMINAL_HOST is set, so it never affects CI. To run it
// against a real Nexi terminal on your LAN:
//
//   ECR17_TERMINAL_HOST=192.168.1.50 \
//   ECR17_TERMINAL_PORT=10000 \
//   ECR17_TERMINAL_ID=00000000 \
//   ECR17_LRC_MODE=std \
//   ctest --test-dir build --output-on-failure -R Integration
//
// It connects over real TCP, sends a Terminal Status ('s') request through the
// full C++ core (builder -> session/ACK-NAK -> parser) and prints the result.

#if !defined(_WIN32)

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "Ecr17Protocol/Ecr17Protocol.hpp"
#include "Ecr17Response/Ecr17Response.hpp"
#include "PosixTcpTransport.hpp"
#include "Session/Ecr17Session.hpp"

using namespace margelo::nitro::ecr17;

namespace {
LrcMode lrcModeFromEnv() {
    const char* m = std::getenv("ECR17_LRC_MODE");
    const std::string mode = m ? m : "std";
    if (mode == "stx") return LrcMode::STX;
    if (mode == "noext") return LrcMode::NOEXT;
    if (mode == "stx_noext") return LrcMode::STX_NOEXT;
    return LrcMode::STD;
}
}  // namespace

TEST(Integration, RealTerminalStatus) {
    const char* host = std::getenv("ECR17_TERMINAL_HOST");
    if (host == nullptr) {
        GTEST_SKIP() << "set ECR17_TERMINAL_HOST to run against a real terminal";
    }
    const char* portEnv = std::getenv("ECR17_TERMINAL_PORT");
    const char* idEnv = std::getenv("ECR17_TERMINAL_ID");
    const int port = portEnv ? std::atoi(portEnv) : 10000;
    const std::string terminalId = idEnv ? idEnv : "00000000";

    PosixTcpTransport transport(host, port);
    transport.connect();

    SessionConfig cfg;
    cfg.lrcMode = lrcModeFromEnv();
    cfg.ackTimeoutMs = 3000;
    cfg.responseTimeoutMs = 15000;
    Ecr17Session session(transport, cfg);

    DecodedPacket pkt = session.exchange(Ecr17Protocol::buildStatusMessage(terminalId));
    StatusResponse status = Ecr17Response::parseStatus(pkt.payload);

    std::cout << "[ECR17] terminalId=" << status.terminalId << " status=" << status.status
              << " dateTime=" << status.dateTimeRaw << " sw=" << status.softwareRelease << std::endl;

    EXPECT_TRUE(pkt.validLrc);
    EXPECT_FALSE(status.terminalId.empty());

    transport.disconnect();
}

#endif  // !_WIN32
