// Loopback tests for the POSIX transport (macOS, Linux): the same contract as the
// Winsock transport's tests (windows/tests), against a real TCP socket:
//  - isConnected() detects a peer-closed socket BEFORE a send (proactive reconnect);
//  - the probe never writes to the peer and never consumes a protocol byte;
//  - onDisconnect fires exactly once per unexpected drop, never on disconnect();
//  - a send to a closed peer throws instead of raising SIGPIPE.
#include "ecr17/PosixTransport.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

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

using padosoft::ecr17::Endpoint;
using padosoft::ecr17::PosixTransport;
using namespace std::chrono_literals;

namespace {

// A one-connection TCP server on 127.0.0.1 with an ephemeral port.
class LoopbackServer {
public:
  LoopbackServer() {
    listener_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener_ < 0) {
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
    socklen_t length = sizeof(address);
    ::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length);
    port_ = ntohs(address.sin_port);
  }

  ~LoopbackServer() {
    closePeer();
    ::close(listener_);
  }

  int port() const { return port_; }

  void accept() {
    peer_ = ::accept(listener_, nullptr, nullptr);
    ASSERT_GE(peer_, 0);
  }

  void sendBytes(const std::vector<uint8_t>& bytes) {
    ASSERT_EQ(::send(peer_, bytes.data(), bytes.size(), 0), static_cast<ssize_t>(bytes.size()));
  }

  // Reads whatever arrives within `timeout` (empty if nothing).
  std::vector<uint8_t> receive(std::chrono::milliseconds timeout) {
    timeval tv{static_cast<time_t>(timeout.count() / 1000), static_cast<suseconds_t>((timeout.count() % 1000) * 1000)};
    ::setsockopt(peer_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    std::vector<uint8_t> buffer(4096);
    const ssize_t read = ::recv(peer_, buffer.data(), buffer.size(), 0);
    buffer.resize(read > 0 ? static_cast<size_t>(read) : 0);
    return buffer;
  }

  // Graceful close from the terminal side (FIN), like an ECR17 terminal between transactions.
  void closePeer() {
    if (peer_ >= 0) {
      ::shutdown(peer_, SHUT_RDWR);
      ::close(peer_);
      peer_ = -1;
    }
  }

private:
  int listener_ = -1;
  int peer_ = -1;
  int port_ = 0;
};

// Collects transport callbacks.
struct Recorder {
  std::mutex mutex;
  std::condition_variable changed;
  std::vector<uint8_t> received;
  int disconnects = 0;

  void attach(PosixTransport& transport) {
    transport.setDataCallback([this](const std::vector<uint8_t>& bytes) {
      std::lock_guard<std::mutex> lock(mutex);
      received.insert(received.end(), bytes.begin(), bytes.end());
      changed.notify_all();
    });
    transport.setDisconnectCallback([this]() {
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

std::shared_ptr<PosixTransport> connectTo(LoopbackServer& server, Recorder& recorder) {
  auto transport = std::make_shared<PosixTransport>();
  recorder.attach(*transport);
  transport->connect(Endpoint{"127.0.0.1", server.port(), 2000});
  server.accept();
  return transport;
}

// Polls isConnected() until it reports a drop (the probe should see the FIN quickly).
bool probeUntilDropped(PosixTransport& transport, std::chrono::milliseconds timeout = 3s) {
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

TEST(PosixTransport, ConnectsAndDeliversReceivedBytes) {
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

TEST(PosixTransport, SendWritesBytesToThePeer) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  const std::vector<uint8_t> frame{0x02, 'P', '1', '2', 0x03, 0x55};
  transport->send(frame);
  EXPECT_EQ(server.receive(2s), frame);
}

TEST(PosixTransport, IdleSocketIsReportedAliveQuickly) {
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

TEST(PosixTransport, ProbeNeverWritesToThePeer) {
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

TEST(PosixTransport, ProbeNeverConsumesProtocolBytes) {
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

TEST(PosixTransport, PeerCloseIsDetectedBeforeSendAndSignalledOnce) {
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
  EXPECT_THROW(transport->send(std::vector<uint8_t>{0x02}), std::runtime_error);
}

// The money-critical path: the reader is busy (here: stuck in a slow onData
// consumer), so only the pre-send probe can notice the terminal's FIN. It must
// report the drop, and the reader seeing EOF later must not signal it again.
TEST(PosixTransport, ProbeDetectsPeerCloseWhileTheReaderIsBusy) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  std::mutex gateMutex;
  std::condition_variable gateChanged;
  bool readerInCallback = false;
  bool releaseReader = false;
  transport->setDataCallback([&](const std::vector<uint8_t>&) {
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

TEST(PosixTransport, CallerDisconnectDoesNotSignalADrop) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  transport->disconnect();

  EXPECT_FALSE(transport->isConnected());
  std::this_thread::sleep_for(300ms);
  EXPECT_EQ(recorder.disconnectCount(), 0);
}

TEST(PosixTransport, SendWithoutConnectionThrows) {
  PosixTransport transport;
  EXPECT_FALSE(transport.isConnected());
  EXPECT_THROW(transport.send(std::vector<uint8_t>{0x02}), std::runtime_error);
}

TEST(PosixTransport, ConnectToClosedPortRejects) {
  int closedPort = 0;
  {
    LoopbackServer server;
    closedPort = server.port();
  }
  auto transport = std::make_shared<PosixTransport>();
  EXPECT_ANY_THROW(transport->connect(Endpoint{"127.0.0.1", closedPort, 2000}));
  EXPECT_FALSE(transport->isConnected());
}

TEST(PosixTransport, InvalidPortRejects) {
  auto transport = std::make_shared<PosixTransport>();
  EXPECT_THROW(transport->connect(Endpoint{"127.0.0.1", 0, 1000}), std::invalid_argument);
  EXPECT_THROW(transport->connect(Endpoint{"127.0.0.1", 70000, 1000}), std::invalid_argument);
}

TEST(PosixTransport, ReconnectAfterDropGivesAFreshConnection) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  server.closePeer();
  ASSERT_TRUE(probeUntilDropped(*transport));
  ASSERT_TRUE(recorder.waitForDisconnects(1));

  transport->connect(Endpoint{"127.0.0.1", server.port(), 2000});
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

TEST(PosixTransport, DisconnectFromInsideTheDataCallbackDoesNotDeadlock) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  std::atomic<bool> called{false};
  transport->setDataCallback([&](const std::vector<uint8_t>&) {
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

TEST(PosixTransport, SendToAClosedPeerThrowsInsteadOfRaisingSigpipe) {
  LoopbackServer server;
  Recorder recorder;
  auto transport = connectTo(server, recorder);

  server.closePeer();
  // The first write after the peer's close may still succeed (it triggers the RST);
  // a later one fails. Without MSG_NOSIGNAL / SO_NOSIGPIPE that failure is a SIGPIPE,
  // which would kill the test process (and a Node.js host) instead of throwing.
  bool threw = false;
  for (int i = 0; i < 50 && !threw; ++i) {
    try {
      transport->send(std::vector<uint8_t>(1024, 0x02));
    } catch (const std::runtime_error&) {
      threw = true;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_TRUE(threw);
}
