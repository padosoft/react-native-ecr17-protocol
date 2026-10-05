#pragma once

// winsock2.h must come before any <windows.h> that is not WIN32_LEAN_AND_MEAN
// (pch.h includes it first for the DLL).
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <winsock2.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "HybridEcr17TransportSpec.hpp"

namespace margelo::nitro::ecr17 {

// Windows (Winsock) implementation of the ECR17 LAN transport.
//
// iOS/Android implement the `Ecr17Transport` spec in Swift/Kotlin. React Native
// Windows has no Nitro Swift/Kotlin bridge, so on Windows the spec is
// implemented directly in C++ and registered in the HybridObjectRegistry by the
// NitroModules TurboModule in this DLL (see Ecr17Module.cpp).
//
// Behavior mirrors HybridEcr17Transport.kt:
//  - plain TCP socket (TCP_NODELAY) with a background reader thread that
//    forwards received bytes via onData;
//  - isConnected() is a synchronous, NON-DESTRUCTIVE, WRITE-FREE liveness probe
//    (non-blocking poll + MSG_PEEK of one byte). ECR17/Nexi terminals close the
//    socket between transactions, so the drop must be detected BEFORE a
//    financial command is sent, never by writing bytes to the peer;
//  - onDisconnect fires exactly once per unexpected drop, never for a
//    caller-initiated disconnect().
class HybridEcr17TransportWindows : public HybridEcr17TransportSpec {
   public:
    HybridEcr17TransportWindows() : HybridObject(TAG) {}
    ~HybridEcr17TransportWindows() override;

    std::shared_ptr<Promise<void>> connect(const std::string& host, double port, double timeoutMs) override;
    void disconnect() override;
    bool isConnected() override;
    void send(const std::shared_ptr<ArrayBuffer>& bytes) override;
    void setOnData(const std::function<void(const std::shared_ptr<ArrayBuffer>&)>& callback) override;
    void setOnDisconnect(const std::function<void()>& callback) override;

   private:
    // Callbacks live in their own shared block so the reader thread never touches
    // `this` (it may outlive a disconnect() issued from inside a callback).
    struct Callbacks {
        std::mutex mutex;
        std::function<void(const std::shared_ptr<ArrayBuffer>&)> onData;
        std::function<void()> onDisconnect;
    };

    // One TCP connection. A new connect() creates a fresh Connection, so a stale
    // reader thread from a previous socket can never flip the state of the new one.
    struct Connection {
        SOCKET socket = INVALID_SOCKET;
        std::atomic<bool> running{true};
        // True while a caller-initiated disconnect is in progress: no onDisconnect.
        std::atomic<bool> intentional{false};
        // Single-shot guard: a drop fires onDisconnect once, whether the reader
        // thread or the liveness probe observes it first.
        std::atomic<bool> disconnectEmitted{false};
        // Keeps the handle valid for the isConnected() probe: closeCurrent() takes it
        // before closesocket(). The reader needs no lock (it is joined before the close).
        std::mutex ioMutex;
        // Serializes send() against the final close.
        std::mutex sendMutex;
        std::thread reader;
    };

    static void readerLoop(std::shared_ptr<Connection> conn, std::shared_ptr<Callbacks> callbacks);
    // Marks the connection dropped. Returns true if the caller must fire onDisconnect.
    static bool markDropped(Connection& conn);
    static void fireDisconnect(Callbacks& callbacks);

    void connectBlocking(const std::string& host, int port, int timeoutMs);
    std::shared_ptr<Connection> current();
    // Closes the current socket and joins its reader thread (intentional close).
    void closeCurrent();

    // Serializes connect() calls so two concurrent connects cannot leak a socket.
    std::mutex connectMutex_;
    std::mutex connMutex_;
    std::shared_ptr<Connection> conn_;
    std::shared_ptr<Callbacks> callbacks_ = std::make_shared<Callbacks>();
};

}  // namespace margelo::nitro::ecr17
