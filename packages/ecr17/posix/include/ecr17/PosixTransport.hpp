#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "ecr17/Transport.hpp"

namespace padosoft::ecr17 {

// POSIX (macOS, Linux) implementation of the ECR17 LAN transport: the transport of the
// Node.js addon, and of any native macOS / Linux app (desktop, server, command line).
//
// Same contract and design as WinsockTransport (windows/), which mirrors the Kotlin
// transport of the React Native binding:
//  - plain TCP socket (TCP_NODELAY) with a background reader thread that forwards
//    received bytes via onData;
//  - isConnected() is a synchronous, NON-DESTRUCTIVE, WRITE-FREE liveness probe (an
//    instant poll() + recv(MSG_PEEK) of one byte). ECR17/Nexi terminals close the socket
//    between transactions, so the drop must be detected BEFORE a financial command is
//    sent, never by writing bytes to the peer;
//  - onDisconnect fires exactly once per unexpected drop, never for a caller-initiated
//    disconnect();
//  - writing to a socket the peer closed never raises SIGPIPE (MSG_NOSIGNAL /
//    SO_NOSIGPIPE): it throws, so a host process such as Node.js is not killed.
class PosixTransport final : public Transport {
   public:
    PosixTransport() = default;
    ~PosixTransport() override;

    PosixTransport(const PosixTransport&) = delete;
    PosixTransport& operator=(const PosixTransport&) = delete;

    /// Blocks until connected (or throws). Ports outside 1..65535 throw
    /// std::invalid_argument; `timeoutMs <= 0` waits indefinitely.
    void connect(const Endpoint& endpoint) override;
    void disconnect() override;
    bool isConnected() override;
    void send(const std::vector<uint8_t>& bytes) override;
    void setDataCallback(DataCallback callback) override;
    void setDisconnectCallback(DisconnectCallback callback) override;

   private:
    // Callbacks live in their own shared block so the reader thread never touches
    // `this` (it may outlive a disconnect() issued from inside a callback).
    struct Callbacks {
        std::mutex mutex;
        DataCallback onData;
        DisconnectCallback onDisconnect;
    };

    // One TCP connection. A new connect() creates a fresh Connection, so a stale
    // reader thread from a previous socket can never flip the state of the new one.
    struct Connection {
        int fd = -1;
        std::atomic<bool> running{true};
        // True while a caller-initiated disconnect is in progress: no onDisconnect.
        std::atomic<bool> intentional{false};
        // Single-shot guard: a drop fires onDisconnect once, whether the reader
        // thread or the liveness probe observes it first.
        std::atomic<bool> disconnectEmitted{false};
        // Keeps the descriptor valid for the isConnected() probe: closeCurrent() takes
        // it before close(). Held by the reader only around recv().
        std::mutex ioMutex;
        // Serializes send() against the final close.
        std::mutex sendMutex;
        std::thread reader;
    };

    static void readerLoop(std::shared_ptr<Connection> conn, std::shared_ptr<Callbacks> callbacks);
    // Marks the connection dropped. Returns true if the caller must fire onDisconnect.
    static bool markDropped(Connection& conn);
    static void fireDisconnect(Callbacks& callbacks);

    std::shared_ptr<Connection> current();
    // Closes the current socket and joins its reader thread (intentional close).
    void closeCurrent();

    // Serializes connect() calls so two concurrent connects cannot leak a socket.
    std::mutex connectMutex_;
    std::mutex connMutex_;
    std::shared_ptr<Connection> conn_;
    std::shared_ptr<Callbacks> callbacks_ = std::make_shared<Callbacks>();
};

}  // namespace padosoft::ecr17
