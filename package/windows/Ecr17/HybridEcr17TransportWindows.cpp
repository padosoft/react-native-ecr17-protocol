// No pch.h: this file is plain Winsock + Nitro (no WinRT), so the standalone
// transport test (windows/tests) can compile it without React Native Windows.
#include "HybridEcr17TransportWindows.hpp"

#include <ws2tcpip.h>

#include <NitroModules/ArrayBuffer.hpp>

#include <algorithm>
#include <climits>
#include <stdexcept>
#include <utility>

namespace margelo::nitro::ecr17 {

namespace {

// Reader-loop wait: how often the reader re-checks `running` when idle
// (closeCurrent() also wakes it immediately via shutdown()).
constexpr int kReadTimeoutMs = 100;

// isConnected() polls without waiting: a FIN or pending data is already visible
// to select(), so a healthy idle socket is reported "alive" in microseconds.
// (The Kotlin transport needs a 1 ms read timeout only because Java cannot poll.)
constexpr int kProbeTimeoutMs = 0;

// Upper bound for a single blocking send(), so a stalled peer cannot hold the
// send lock (and therefore disconnect()) forever.
constexpr DWORD kSendTimeoutMs = 15000;

constexpr int kReadBufferSize = 4096;

void ensureWinsock() {
    static std::once_flag once;
    static int startupError = 0;
    std::call_once(once, []() {
        WSADATA data{};
        startupError = ::WSAStartup(MAKEWORD(2, 2), &data);
    });
    if (startupError != 0) {
        throw std::runtime_error("ECR17 transport: WSAStartup failed (error " + std::to_string(startupError) + ")");
    }
}

std::string wsaMessage(const std::string& what, int code) {
    return "ECR17 transport: " + what + " (WSA error " + std::to_string(code) + ")";
}

// Waits until `s` is readable. Returns 1 if readable, 0 on timeout, -1 on error.
int waitReadable(SOCKET s, int timeoutMs) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(s, &readSet);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    const int ready = ::select(0, &readSet, nullptr, nullptr, &tv);
    if (ready == SOCKET_ERROR) {
        return -1;
    }
    return ready > 0 ? 1 : 0;
}

// Connects to one resolved address with a timeout (timeoutMs <= 0 waits
// indefinitely, like java.net.Socket.connect(addr, 0)). Returns INVALID_SOCKET
// and sets `lastError` on failure.
SOCKET connectWithTimeout(const addrinfo* address, int timeoutMs, int& lastError) {
    SOCKET s = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (s == INVALID_SOCKET) {
        lastError = ::WSAGetLastError();
        return INVALID_SOCKET;
    }

    u_long nonBlocking = 1;
    ::ioctlsocket(s, FIONBIO, &nonBlocking);

    if (::connect(s, address->ai_addr, static_cast<int>(address->ai_addrlen)) == SOCKET_ERROR) {
        const int error = ::WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            lastError = error;
            ::closesocket(s);
            return INVALID_SOCKET;
        }

        fd_set writeSet;
        fd_set errorSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        FD_SET(s, &writeSet);
        FD_SET(s, &errorSet);
        timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        const int ready = ::select(0, nullptr, &writeSet, &errorSet, timeoutMs > 0 ? &tv : nullptr);
        if (ready == 0) {
            lastError = WSAETIMEDOUT;
            ::closesocket(s);
            return INVALID_SOCKET;
        }
        if (ready == SOCKET_ERROR) {
            lastError = ::WSAGetLastError();
            ::closesocket(s);
            return INVALID_SOCKET;
        }

        int socketError = 0;
        int length = sizeof(socketError);
        ::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &length);
        if (FD_ISSET(s, &errorSet) || socketError != 0) {
            lastError = socketError != 0 ? socketError : WSAECONNREFUSED;
            ::closesocket(s);
            return INVALID_SOCKET;
        }
    }

    u_long blocking = 0;
    ::ioctlsocket(s, FIONBIO, &blocking);

    BOOL noDelay = TRUE;
    ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
    DWORD sendTimeout = kSendTimeoutMs;
    ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sendTimeout), sizeof(sendTimeout));
    return s;
}

}  // namespace

HybridEcr17TransportWindows::~HybridEcr17TransportWindows() {
    try {
        closeCurrent();
    } catch (...) {
        // Destructors must not throw; the socket is gone either way.
    }
}

std::shared_ptr<Promise<void>> HybridEcr17TransportWindows::connect(const std::string& host, double port,
                                                                    double timeoutMs) {
    auto self = shared_cast<HybridEcr17TransportWindows>();
    auto promise = Promise<void>::create();
    // A dedicated thread, not Promise::async: HybridEcr17Client::ensureConnected()
    // already runs on Nitro's shared ThreadPool and blocks on this promise. If the
    // connect were queued on the same pool while every worker is busy (e.g. a long
    // payment exchange), it would wait for a free worker and connectionTimeoutMs
    // would no longer bound it. (Kotlin's Promise.parallel uses its own executor.)
    std::thread([self, promise, host, port, timeoutMs]() {
        try {
            if (!(port >= 1 && port <= 65535)) {
                throw std::invalid_argument("ECR17 transport: port must be between 1 and 65535");
            }
            // <= 0 (or NaN) waits indefinitely, like java.net.Socket.connect(addr, 0).
            const int timeout = timeoutMs > 0 ? static_cast<int>(std::min(timeoutMs, static_cast<double>(INT_MAX))) : 0;
            self->connectBlocking(host, static_cast<int>(port), timeout);
            promise->resolve();
        } catch (...) {
            promise->reject(std::current_exception());
        }
    }).detach();
    return promise;
}

void HybridEcr17TransportWindows::connectBlocking(const std::string& host, int port, int timeoutMs) {
    ensureWinsock();
    std::lock_guard<std::mutex> connectLock(connectMutex_);

    // Tear down any previous connection (and join its reader) before reconnecting.
    closeCurrent();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* addresses = nullptr;
    const int resolveError = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addresses);
    if (resolveError != 0) {
        throw std::runtime_error(wsaMessage("cannot resolve host '" + host + "'", resolveError));
    }

    SOCKET s = INVALID_SOCKET;
    int lastError = 0;
    for (const addrinfo* address = addresses; address != nullptr && s == INVALID_SOCKET; address = address->ai_next) {
        s = connectWithTimeout(address, timeoutMs, lastError);
    }
    ::freeaddrinfo(addresses);
    if (s == INVALID_SOCKET) {
        throw std::runtime_error(wsaMessage("connect to " + host + ":" + std::to_string(port) + " failed", lastError));
    }

    auto conn = std::make_shared<Connection>();
    conn->socket = s;
    std::lock_guard<std::mutex> lock(connMutex_);
    conn_ = conn;
    conn->reader = std::thread(&HybridEcr17TransportWindows::readerLoop, conn, callbacks_);
}

void HybridEcr17TransportWindows::readerLoop(std::shared_ptr<Connection> conn, std::shared_ptr<Callbacks> callbacks) {
    ::SetThreadDescription(::GetCurrentThread(), L"ecr17-reader");
    // The handle is stable for the reader's whole life: closeCurrent() joins this
    // thread before closesocket(). (If disconnect() runs inside a callback on this
    // thread, `running` is already false when the callback returns, so the loop
    // exits without touching the socket again.)
    //
    // No lock is held while waiting, so the isConnected() probe is never stuck
    // behind a 100 ms select() (MSVC's std::mutex is not fair: a reader that holds
    // the lock across its wait can starve the probe indefinitely). ioMutex is taken
    // only around recv(): the probe holds it for select + peek, so the reader can
    // never consume the bytes between the probe's select and its (blocking) peek.
    // recv() itself does not block here: select reported data or EOF, and only the
    // reader consumes bytes.
    const SOCKET s = conn->socket;
    char buffer[kReadBufferSize];
    try {
        while (conn->running.load()) {
            const int ready = waitReadable(s, kReadTimeoutMs);
            if (ready == 0) {
                continue;  // no data within kReadTimeoutMs; re-check `running`
            }
            if (ready < 0 || !conn->running.load()) {
                break;
            }
            int read = 0;
            {
                std::lock_guard<std::mutex> io(conn->ioMutex);
                read = ::recv(s, buffer, sizeof(buffer), 0);
            }
            if (read <= 0) {
                break;  // 0: peer closed the connection (EOF); SOCKET_ERROR: I/O error or local close
            }

            std::function<void(const std::shared_ptr<ArrayBuffer>&)> onData;
            {
                std::lock_guard<std::mutex> lock(callbacks->mutex);
                onData = callbacks->onData;
            }
            if (onData) {
                onData(ArrayBuffer::copy(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(read)));
            }
        }
    } catch (...) {
        // A throwing consumer ends the reader like an I/O error does on Android:
        // the connection is reported as dropped below.
    }

    if (markDropped(*conn)) {
        fireDisconnect(*callbacks);
    }
}

bool HybridEcr17TransportWindows::markDropped(Connection& conn) {
    conn.running.store(false);
    // Only unexpected drops are signalled, and only once per connection.
    if (conn.intentional.load()) {
        return false;
    }
    // Send our FIN too (the Kotlin transport closes the socket here), so the
    // terminal does not keep a half-closed connection. The handle itself stays
    // open until closeCurrent(), which is the only place that may closesocket().
    ::shutdown(conn.socket, SD_SEND);
    return !conn.disconnectEmitted.exchange(true);
}

void HybridEcr17TransportWindows::fireDisconnect(Callbacks& callbacks) {
    std::function<void()> onDisconnect;
    {
        std::lock_guard<std::mutex> lock(callbacks.mutex);
        onDisconnect = callbacks.onDisconnect;
    }
    if (onDisconnect) {
        onDisconnect();
    }
}

std::shared_ptr<HybridEcr17TransportWindows::Connection> HybridEcr17TransportWindows::current() {
    std::lock_guard<std::mutex> lock(connMutex_);
    return conn_;
}

void HybridEcr17TransportWindows::closeCurrent() {
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        conn = std::move(conn_);
    }
    if (!conn) {
        return;
    }

    conn->intentional.store(true);
    conn->running.store(false);
    // Wake the reader out of select()/recv() right away. The handle itself stays
    // valid until closesocket() below, so no other thread can use a recycled handle.
    ::shutdown(conn->socket, SD_BOTH);

    if (conn->reader.joinable()) {
        if (conn->reader.get_id() == std::this_thread::get_id()) {
            // disconnect() called from inside an onData/onDisconnect callback: the
            // reader is this thread. It exits on its own once the callback returns.
            conn->reader.detach();
        } else {
            conn->reader.join();
        }
    }

    std::scoped_lock lock(conn->ioMutex, conn->sendMutex);
    ::closesocket(conn->socket);
    conn->socket = INVALID_SOCKET;
}

void HybridEcr17TransportWindows::disconnect() {
    closeCurrent();
}

// Non-destructive liveness probe used to detect a peer-closed / half-open socket
// BEFORE a command is sent (see HybridEcr17Transport.kt for the full rationale).
// recv(MSG_PEEK) looks at one byte without consuming it, so a protocol byte is
// never lost, and nothing is ever written to the peer. A readable socket that
// peeks 0 bytes means the peer closed (FIN); not readable means idle but alive.
// ioMutex (held only for this instant poll + peek) keeps the reader's recv() from
// draining the bytes between select and peek, which would make the blocking peek
// wait for the next frame, and keeps the handle alive against closeCurrent().
//
// Money-safety is unchanged: this only removes the FALSE drop from a stale
// pre-send socket; a genuine mid-exchange drop still surfaces and is recovered
// via sendLastResult ('G').
bool HybridEcr17TransportWindows::isConnected() {
    auto conn = current();
    if (!conn || !conn->running.load()) {
        return false;
    }

    bool alive = false;
    bool fire = false;
    {
        std::lock_guard<std::mutex> io(conn->ioMutex);
        if (!conn->running.load() || conn->socket == INVALID_SOCKET) {
            return false;
        }
        const int ready = waitReadable(conn->socket, kProbeTimeoutMs);
        if (ready == 0) {
            alive = true;  // idle but alive: no FIN pending
        } else if (ready > 0) {
            char probe = 0;
            const int peeked = ::recv(conn->socket, &probe, 1, MSG_PEEK);
            if (peeked > 0) {
                alive = true;  // a pending protocol byte; MSG_PEEK left it in the stream
            } else if (peeked == SOCKET_ERROR && ::WSAGetLastError() == WSAEWOULDBLOCK) {
                alive = true;
            }
        }
        if (!alive) {
            fire = markDropped(*conn);
        }
    }
    if (fire) {
        fireDisconnect(*callbacks_);
    }
    return alive;
}

void HybridEcr17TransportWindows::send(const std::shared_ptr<ArrayBuffer>& bytes) {
    auto conn = current();
    if (!conn || !conn->running.load()) {
        throw std::runtime_error("ECR17 transport is not connected");
    }
    if (bytes == nullptr) {
        throw std::invalid_argument("ECR17 transport: send() needs a buffer");
    }

    std::lock_guard<std::mutex> lock(conn->sendMutex);
    if (conn->socket == INVALID_SOCKET) {
        throw std::runtime_error("ECR17 transport is not connected");
    }
    const char* data = reinterpret_cast<const char*>(bytes->data());
    size_t remaining = bytes->size();
    while (remaining > 0) {
        const int chunk = static_cast<int>(std::min<size_t>(remaining, INT_MAX));
        const int sent = ::send(conn->socket, data, chunk, 0);
        if (sent == SOCKET_ERROR) {
            throw std::runtime_error(wsaMessage("send failed", ::WSAGetLastError()));
        }
        data += sent;
        remaining -= static_cast<size_t>(sent);
    }
}

void HybridEcr17TransportWindows::setOnData(const std::function<void(const std::shared_ptr<ArrayBuffer>&)>& callback) {
    std::lock_guard<std::mutex> lock(callbacks_->mutex);
    callbacks_->onData = callback;
}

void HybridEcr17TransportWindows::setOnDisconnect(const std::function<void()>& callback) {
    std::lock_guard<std::mutex> lock(callbacks_->mutex);
    callbacks_->onDisconnect = callback;
}

}  // namespace margelo::nitro::ecr17
