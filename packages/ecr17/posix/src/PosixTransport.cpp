#include "ecr17/PosixTransport.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace padosoft::ecr17 {

namespace {

// Reader-loop wait: how often the reader re-checks `running` when idle
// (closeCurrent() also wakes it immediately via shutdown()).
constexpr int kReadTimeoutMs = 100;

// isConnected() polls without waiting: a FIN or pending data is already visible to
// poll(), so a healthy idle socket is reported "alive" in microseconds.
constexpr int kProbeTimeoutMs = 0;

// Upper bound for a single blocking send(), so a stalled peer cannot hold the send
// lock (and therefore disconnect()) forever.
constexpr int kSendTimeoutMs = 15000;

constexpr size_t kReadBufferSize = 4096;

#if defined(MSG_NOSIGNAL)
constexpr int kSendFlags = MSG_NOSIGNAL;  // Linux: no SIGPIPE on a closed peer
#else
constexpr int kSendFlags = 0;  // macOS / BSD: SO_NOSIGPIPE on the socket instead
#endif

std::string errnoMessage(const std::string& what, int code) {
    return "ECR17 transport: " + what + " (" + std::strerror(code) + ")";
}

// Waits until `fd` is readable (data, EOF or error). Returns 1 if readable, 0 on
// timeout, -1 on error.
int waitReadable(int fd, int timeoutMs) {
    pollfd p{fd, POLLIN, 0};
    int ready = 0;
    do {
        ready = ::poll(&p, 1, timeoutMs);
    } while (ready < 0 && errno == EINTR);
    if (ready < 0) {
        return -1;
    }
    if (ready == 0) {
        return 0;
    }
    // POLLHUP / POLLERR also mean "a recv() will not block": it returns 0 or fails.
    return (p.revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) != 0 ? 1 : 0;
}

void setNonBlocking(int fd, bool enabled) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}

// Connects to one resolved address with a timeout (timeoutMs <= 0 waits
// indefinitely). Returns -1 and sets `lastError` on failure.
int connectWithTimeout(const addrinfo* address, int timeoutMs, int& lastError) {
    int fd = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (fd < 0) {
        lastError = errno;
        return -1;
    }
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
#if defined(SO_NOSIGPIPE)
    int noSigPipe = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe));
#endif

    setNonBlocking(fd, true);
    if (::connect(fd, address->ai_addr, address->ai_addrlen) != 0) {
        if (errno != EINPROGRESS && errno != EINTR) {
            lastError = errno;
            ::close(fd);
            return -1;
        }
        pollfd p{fd, POLLOUT, 0};
        int ready = 0;
        do {
            ready = ::poll(&p, 1, timeoutMs > 0 ? timeoutMs : -1);
        } while (ready < 0 && errno == EINTR);
        if (ready == 0) {
            lastError = ETIMEDOUT;
            ::close(fd);
            return -1;
        }
        if (ready < 0) {
            lastError = errno;
            ::close(fd);
            return -1;
        }
        int socketError = 0;
        socklen_t length = sizeof(socketError);
        ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &length);
        if (socketError != 0) {
            lastError = socketError;
            ::close(fd);
            return -1;
        }
    }
    setNonBlocking(fd, false);

    int noDelay = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
    timeval sendTimeout{kSendTimeoutMs / 1000, (kSendTimeoutMs % 1000) * 1000};
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof(sendTimeout));
    return fd;
}

}  // namespace

PosixTransport::~PosixTransport() {
    try {
        closeCurrent();
    } catch (...) {
        // Destructors must not throw; the socket is gone either way.
    }
}

void PosixTransport::connect(const Endpoint& endpoint) {
    if (endpoint.port < 1 || endpoint.port > 65535) {
        throw std::invalid_argument("ECR17 transport: port must be between 1 and 65535");
    }
    const std::string& host = endpoint.host;
    const int port = endpoint.port;
    const int timeoutMs = std::max(endpoint.timeoutMs, 0);  // 0: wait indefinitely
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
        throw std::runtime_error("ECR17 transport: cannot resolve host '" + host + "' (" +
                                 ::gai_strerror(resolveError) + ")");
    }

    int fd = -1;
    int lastError = 0;
    for (const addrinfo* address = addresses; address != nullptr && fd < 0; address = address->ai_next) {
        fd = connectWithTimeout(address, timeoutMs, lastError);
    }
    ::freeaddrinfo(addresses);
    if (fd < 0) {
        throw std::runtime_error(errnoMessage("connect to " + host + ":" + std::to_string(port) + " failed", lastError));
    }

    auto conn = std::make_shared<Connection>();
    conn->fd = fd;
    std::lock_guard<std::mutex> lock(connMutex_);
    conn_ = conn;
    conn->reader = std::thread(&PosixTransport::readerLoop, conn, callbacks_);
}

void PosixTransport::readerLoop(std::shared_ptr<Connection> conn, std::shared_ptr<Callbacks> callbacks) {
    // The descriptor is stable for the reader's whole life: closeCurrent() joins this
    // thread before close(). (If disconnect() runs inside a callback on this thread,
    // `running` is already false when the callback returns, so the loop exits without
    // touching the socket again.)
    //
    // No lock is held while waiting, so the isConnected() probe is never stuck behind a
    // 100 ms poll(). ioMutex is taken only around recv(): the probe holds it for poll +
    // peek, so the reader can never consume the bytes between the probe's poll and its
    // peek. recv() itself does not block here: poll reported data, EOF or an error, and
    // only the reader consumes bytes.
    const int fd = conn->fd;
    uint8_t buffer[kReadBufferSize];
    try {
        while (conn->running.load()) {
            const int ready = waitReadable(fd, kReadTimeoutMs);
            if (ready == 0) {
                continue;  // no data within kReadTimeoutMs; re-check `running`
            }
            if (ready < 0 || !conn->running.load()) {
                break;
            }
            ssize_t read = 0;
            {
                std::lock_guard<std::mutex> io(conn->ioMutex);
                do {
                    read = ::recv(fd, buffer, sizeof(buffer), 0);
                } while (read < 0 && errno == EINTR);
            }
            if (read <= 0) {
                break;  // 0: peer closed the connection (EOF); < 0: I/O error or local close
            }

            DataCallback onData;
            {
                std::lock_guard<std::mutex> lock(callbacks->mutex);
                onData = callbacks->onData;
            }
            if (onData) {
                onData(std::vector<uint8_t>(buffer, buffer + read));
            }
        }
    } catch (...) {
        // A throwing consumer ends the reader like an I/O error does: the connection is
        // reported as dropped below.
    }

    if (markDropped(*conn)) {
        fireDisconnect(*callbacks);
    }
}

bool PosixTransport::markDropped(Connection& conn) {
    conn.running.store(false);
    // Only unexpected drops are signalled, and only once per connection.
    if (conn.intentional.load()) {
        return false;
    }
    // Send our FIN too, so the terminal does not keep a half-closed connection. The
    // descriptor itself stays open until closeCurrent(), the only place that close()s it.
    ::shutdown(conn.fd, SHUT_WR);
    return !conn.disconnectEmitted.exchange(true);
}

void PosixTransport::fireDisconnect(Callbacks& callbacks) {
    DisconnectCallback onDisconnect;
    {
        std::lock_guard<std::mutex> lock(callbacks.mutex);
        onDisconnect = callbacks.onDisconnect;
    }
    if (onDisconnect) {
        onDisconnect();
    }
}

std::shared_ptr<PosixTransport::Connection> PosixTransport::current() {
    std::lock_guard<std::mutex> lock(connMutex_);
    return conn_;
}

void PosixTransport::closeCurrent() {
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
    // Wake the reader out of poll()/recv() right away. The descriptor stays valid
    // until close() below, so no other thread can use a recycled descriptor number.
    ::shutdown(conn->fd, SHUT_RDWR);

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
    ::close(conn->fd);
    conn->fd = -1;
}

void PosixTransport::disconnect() {
    closeCurrent();
}

// Non-destructive liveness probe used to detect a peer-closed / half-open socket BEFORE
// a command is sent (see the Kotlin transport, HybridEcr17Transport.kt, for the full
// rationale). recv(MSG_PEEK | MSG_DONTWAIT) looks at one byte without consuming it, so a
// protocol byte is never lost, and nothing is ever written to the peer. A readable socket
// that peeks 0 bytes means the peer closed (FIN); not readable means idle but alive.
// ioMutex (held only for this instant poll + peek) keeps the reader's recv() from
// draining the bytes between poll and peek, and keeps the descriptor alive against
// closeCurrent().
//
// Money-safety is unchanged: this only removes the FALSE drop from a stale pre-send
// socket; a genuine mid-exchange drop still surfaces and is recovered via
// sendLastResult ('G').
bool PosixTransport::isConnected() {
    auto conn = current();
    if (!conn || !conn->running.load()) {
        return false;
    }

    bool alive = false;
    bool fire = false;
    {
        std::lock_guard<std::mutex> io(conn->ioMutex);
        if (!conn->running.load() || conn->fd < 0) {
            return false;
        }
        const int ready = waitReadable(conn->fd, kProbeTimeoutMs);
        if (ready == 0) {
            alive = true;  // idle but alive: no FIN pending
        } else if (ready > 0) {
            uint8_t probe = 0;
            ssize_t peeked = 0;
            do {
                peeked = ::recv(conn->fd, &probe, 1, MSG_PEEK | MSG_DONTWAIT);
            } while (peeked < 0 && errno == EINTR);
            if (peeked > 0) {
                alive = true;  // a pending protocol byte; MSG_PEEK left it in the stream
            } else if (peeked < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
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

void PosixTransport::send(const std::vector<uint8_t>& bytes) {
    auto conn = current();
    if (!conn || !conn->running.load()) {
        throw std::runtime_error("ECR17 transport is not connected");
    }

    std::lock_guard<std::mutex> lock(conn->sendMutex);
    if (conn->fd < 0) {
        throw std::runtime_error("ECR17 transport is not connected");
    }
    const uint8_t* data = bytes.data();
    size_t remaining = bytes.size();
    while (remaining > 0) {
        const ssize_t sent = ::send(conn->fd, data, remaining, kSendFlags);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(errnoMessage("send failed", errno));
        }
        data += sent;
        remaining -= static_cast<size_t>(sent);
    }
}

void PosixTransport::setDataCallback(DataCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_->mutex);
    callbacks_->onData = std::move(callback);
}

void PosixTransport::setDisconnectCallback(DisconnectCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_->mutex);
    callbacks_->onDisconnect = std::move(callback);
}

}  // namespace padosoft::ecr17
