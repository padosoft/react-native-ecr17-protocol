#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace padosoft::ecr17 {

using DataCallback = std::function<void(const std::vector<uint8_t>&)>;

using DisconnectCallback = std::function<void()>;

/// Where and how long to connect. `timeoutMs <= 0` waits indefinitely.
struct Endpoint {
    std::string host;
    int port = 10000;
    int timeoutMs = 5000;
};

/// The byte stream to the terminal (TCP on every platform today).
///
/// Implementations: WinsockTransport (Windows, in this Kit), the Swift/Kotlin
/// transports of the React Native binding (through its adapter), FakeTransport
/// (tests). Contract the session and the client rely on:
///  - connect() blocks until connected or throws; it replaces any previous connection.
///  - isConnected() may PROBE the socket, but must never write to the peer nor consume
///    a protocol byte. ECR17/Nexi terminals close TCP between transactions, and the
///    client calls it before every command so a financial command is never sent on a
///    dead socket.
///  - the disconnect callback fires once per unexpected drop, never for disconnect().
class Transport {
   public:
    virtual ~Transport() = default;

    virtual void connect(const Endpoint& endpoint) = 0;

    virtual void disconnect() = 0;

    virtual bool isConnected() = 0;

    virtual void send(const std::vector<uint8_t>& bytes) = 0;

    virtual void setDataCallback(DataCallback cb) = 0;

    virtual void setDisconnectCallback(DisconnectCallback cb) = 0;
};

}  // namespace padosoft::ecr17
