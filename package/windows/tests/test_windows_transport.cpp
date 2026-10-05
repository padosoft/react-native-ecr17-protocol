// Loopback tests for the Windows (Winsock) Ecr17Transport.
//
// The money-safety contract the C++ core relies on (see docs/LESSON.md and
// HybridEcr17Transport.kt) is checked here against a real TCP socket:
//  - isConnected() detects a peer-closed socket BEFORE a send (proactive reconnect);
//  - the probe never writes to the peer and never consumes a protocol byte;
//  - onDisconnect fires exactly once per unexpected drop, never on disconnect().
#include "HybridEcr17TransportWindows.hpp"

#include <ws2tcpip.h>

#include <NitroModules/ArrayBuffer.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using margelo::nitro::ArrayBuffer;
using margelo::nitro::ecr17::HybridEcr17TransportWindows;
using namespace std::chrono_literals;

namespace {

class WinsockEnvironment : public ::testing::Environment {
public:
  void SetUp() override {
    WSADATA data{};
    ASSERT_EQ(::WSAStartup(MAKEWORD(2, 2), &data), 0);
  }
  void TearDown() override { ::WSACleanup(); }
};

const auto* const kWinsock = ::testing::AddGlobalTestEnvironment(new WinsockEnvironment());

// A one-connection TCP server on 127.0.0.1 with an ephemeral port.
class LoopbackServer {
public:
  LoopbackServer() {
    listener_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener_ == INVALID_SOCKET) {
      throw std::runtime_error("socket() failed");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(listener_, 4) != 0) {
      throw std::runtime_error("bind/listen failed");
    }
    int length = sizeof(address);
    ::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length);
    port_ = ntohs(address.sin_port);
  }

  ~LoopbackServer() {
    closePeer();
    ::closesocket(listener_);
  }

  int port() const { return port_; }

  void accept() {
    peer_ = ::accept(listener_, nullptr, nullptr);
    ASSERT_NE(peer_, INVALID_SOCKET);
  }

  void sendBytes(const std::vector<uint8_t>& bytes) {
    ASSERT_EQ(::send(peer_, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0),
              static_cast<int>(bytes.size()));
  }

  // Reads whatever arrives within `timeout` (empty if nothing).
  std::vector<uint8_t> receive(std::chrono::milliseconds timeout) {
    DWORD ms = static_cast<DWORD>(timeout.count());
    ::setsockopt(peer_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
    std::vector<uint8_t> buffer(4096);
    const int read = ::recv(peer_, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
    buffer.resize(read > 0 ? static_cast<size_t>(read) : 0);
    return buffer;
  }

  // Graceful close from the terminal side (FIN), like an ECR17 terminal between transactions.
  void closePeer() {
    if (peer_ != INVALID_SOCKET) {
      ::shutdown(peer_, SD_BOTH);
      ::closesocket(peer_);
      peer_ = INVALID_SOCKET;
    }
  }

private:
  SOCKET listener_ = INVALID_SOCKET;
  SOCKET peer_ = INVALID_SOCKET;
  int port_ = 0;
};

// Collects transport callbacks.
struct Recorder {
  std::mutex mutex;
  std::condition_variable changed;
  std::vector<uint8_t> received;
  int disconnects = 0;

  void attach(HybridEcr17TransportWindows& transport) {
    transport.setOnData([this](const std::shared_ptr<ArrayBuffer>& bytes) {
      std::lock_guard<std::mutex> lock(mutex);
      received.insert(received.end(), bytes->data(), bytes->data() + bytes->size());
      changed.notify_all();
    });
    transport.setOnDisconnect([this]() {
      std::lock_guard<std::mutex> lock(mutex);
      ++disconnects;
      changed.notify_all();
    });
  }

  bool waitForBytes(size_t count, std::chrono::milliseconds timeout = 3s) {
    std::unique_lock<std::mutex> lock(mutex);
    return changed.wait_for(lock, timeout, [&]() { return received.size() >= count; });
  }

  bool waitForDisconnects(int count, std::chrono::milliseconds timeout = 3s) {
    std::unique_lock<std::mutex> lock(mutex);
    return changed.wait_for(lock, timeout, [&]() { return disconnects >= count; });
  }

  int disconnectCount() {
    std::lock_guard<std::mutex> lock(mutex);
    return disconnects;
  }
};

std::shared_ptr<HybridEcr17TransportWindows> connectTo(LoopbackServer& server, Recorder& recorder) {
  auto transport = std::make_shared<HybridEcr17TransportWindows>();
  recorder.attach(*transport);
  transport->connect("127.0.0.1", server.port(), 2000)->await().get();
  server.accept();
  return transport;
}

// Polls isConnected() until it reports a drop (the probe should see the FIN quickly).
bool probeUntilDropped(HybridEcr17TransportWindows& transport, std::chrono::milliseconds timeout = 3s) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (!transport.isConnected()) {
      return true;
    }
    std::this_thread::sleep_for(5ms);
  }
  return false;
}

} // namespace

TEST(WindowsTransport, ConnectsAndDeliversReceivedBytes) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  EXPECT_TRUE(transport->isConnected());
  const std::vector<uint8_t> frame{0x02, 's', 0x03, 0x7F};
  server.sendBytes(frame);
  ASSERT_TRUE(recorder.waitForBytes(frame.size()));
  std::lock_guard<std::mutex> lock(recorder.mutex);
  EXPECT_EQ(recorder.received, frame);
}

TEST(WindowsTransport, SendWritesBytesToThePeer) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  const std::vector<uint8_t> frame{0x02, 'P', '1', '2', 0x03, 0x55};
  transport->send(ArrayBuffer::copy(frame));
  EXPECT_EQ(server.receive(2s), frame);
}

TEST(WindowsTransport, IdleSocketIsReportedAliveQuickly) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  // isConnected() runs before every command, so it must not wait for the reader
  // (which sits in a 100 ms select()). 200 probes of an idle socket: well under 1 s.
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 200; ++i) {
    EXPECT_TRUE(transport->isConnected());
  }
  EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
  EXPECT_EQ(recorder.disconnectCount(), 0);
}

TEST(WindowsTransport, ProbeNeverWritesToThePeer) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  for (int i = 0; i < 50; ++i) {
    transport->isConnected();
  }
  // A write-based probe (e.g. out-of-band data) could put a byte before the next
  // STX frame and corrupt a financial command. Nothing may reach the terminal.
  EXPECT_TRUE(server.receive(300ms).empty());
}

TEST(WindowsTransport, ProbeNeverConsumesProtocolBytes) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  std::atomic<bool> probing{true};
  std::thread prober([&]() {
    while (probing.load()) {
      transport->isConnected();
    }
  });

  std::vector<uint8_t> expected;
  for (int i = 0; i < 200; ++i) {
    std::vector<uint8_t> chunk{0x02, static_cast<uint8_t>(i & 0x7F), static_cast<uint8_t>((i * 7) & 0xFF), 0x03};
    server.sendBytes(chunk);
    expected.insert(expected.end(), chunk.begin(), chunk.end());
    if (i % 20 == 0) {
      std::this_thread::sleep_for(2ms);
    }
  }

  const bool complete = recorder.waitForBytes(expected.size(), 5s);
  probing.store(false);
  prober.join();

  ASSERT_TRUE(complete);
  std::lock_guard<std::mutex> lock(recorder.mutex);
  EXPECT_EQ(recorder.received, expected);
}

TEST(WindowsTransport, PeerCloseIsDetectedBeforeSendAndSignalledOnce) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  server.closePeer();

  EXPECT_TRUE(probeUntilDropped(*transport));
  ASSERT_TRUE(recorder.waitForDisconnects(1));
  // The reader and the probe may both observe the drop: still exactly one signal.
  std::this_thread::sleep_for(300ms);
  EXPECT_FALSE(transport->isConnected());
  EXPECT_EQ(recorder.disconnectCount(), 1);
  EXPECT_THROW(transport->send(ArrayBuffer::copy(std::vector<uint8_t>{0x02})), std::runtime_error);
}

// The money-critical path: the reader is busy (here: stuck in a slow onData
// consumer), so only the pre-send probe can notice the terminal's FIN. It must
// report the drop, and the reader seeing EOF later must not signal it again.
TEST(WindowsTransport, ProbeDetectsPeerCloseWhileTheReaderIsBusy) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  std::mutex gateMutex;
  std::condition_variable gateChanged;
  bool readerInCallback = false;
  bool releaseReader = false;
  transport->setOnData([&](const std::shared_ptr<ArrayBuffer>&) {
    std::unique_lock<std::mutex> lock(gateMutex);
    readerInCallback = true;
    gateChanged.notify_all();
    gateChanged.wait(lock, [&]() { return releaseReader; });
  });

  server.sendBytes({0x02, 0x03});
  {
    std::unique_lock<std::mutex> lock(gateMutex);
    ASSERT_TRUE(gateChanged.wait_for(lock, 3s, [&]() { return readerInCallback; }));
  }
  server.closePeer();

  EXPECT_TRUE(probeUntilDropped(*transport));
  EXPECT_EQ(recorder.disconnectCount(), 1);

  {
    std::lock_guard<std::mutex> lock(gateMutex);
    releaseReader = true;
  }
  gateChanged.notify_all();
  std::this_thread::sleep_for(300ms);
  EXPECT_FALSE(transport->isConnected());
  EXPECT_EQ(recorder.disconnectCount(), 1);
}

TEST(WindowsTransport, CallerDisconnectDoesNotSignalADrop) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  transport->disconnect();

  EXPECT_FALSE(transport->isConnected());
  std::this_thread::sleep_for(300ms);
  EXPECT_EQ(recorder.disconnectCount(), 0);
}

TEST(WindowsTransport, SendWithoutConnectionThrows) {
  HybridEcr17TransportWindows transport;
  EXPECT_FALSE(transport.isConnected());
  EXPECT_THROW(transport.send(ArrayBuffer::copy(std::vector<uint8_t>{0x02})), std::runtime_error);
}

TEST(WindowsTransport, ConnectToClosedPortRejects) {
  int closedPort = 0;
  {
    LoopbackServer server;
    closedPort = server.port();
  }
  auto transport = std::make_shared<HybridEcr17TransportWindows>();
  EXPECT_ANY_THROW(transport->connect("127.0.0.1", closedPort, 2000)->await().get());
  EXPECT_FALSE(transport->isConnected());
}

TEST(WindowsTransport, InvalidPortRejects) {
  auto transport = std::make_shared<HybridEcr17TransportWindows>();
  EXPECT_ANY_THROW(transport->connect("127.0.0.1", 0, 1000)->await().get());
  EXPECT_ANY_THROW(transport->connect("127.0.0.1", 70000, 1000)->await().get());
}

TEST(WindowsTransport, ReconnectAfterDropGivesAFreshConnection) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  server.closePeer();
  ASSERT_TRUE(probeUntilDropped(*transport));
  ASSERT_TRUE(recorder.waitForDisconnects(1));

  transport->connect("127.0.0.1", server.port(), 2000)->await().get();
  server.accept();

  EXPECT_TRUE(transport->isConnected());
  const std::vector<uint8_t> frame{0x06};
  server.sendBytes(frame);
  ASSERT_TRUE(recorder.waitForBytes(frame.size()));
  // The old connection's reader must not flip the new connection to dropped.
  std::this_thread::sleep_for(300ms);
  EXPECT_TRUE(transport->isConnected());
  EXPECT_EQ(recorder.disconnectCount(), 1);
}

TEST(WindowsTransport, DisconnectFromInsideTheDataCallbackDoesNotDeadlock) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  std::atomic<bool> called{false};
  transport->setOnData([&](const std::shared_ptr<ArrayBuffer>&) {
    transport->disconnect();
    called.store(true);
  });
  server.sendBytes({0x02, 0x03});

  const auto deadline = std::chrono::steady_clock::now() + 3s;
  while (!called.load() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(5ms);
  }
  EXPECT_TRUE(called.load());
  EXPECT_FALSE(transport->isConnected());
  EXPECT_EQ(recorder.disconnectCount(), 0);
}
