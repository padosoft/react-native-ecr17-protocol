#pragma once

// Test-only POSIX TCP transport, used by the opt-in real-terminal integration
// test. Lets a developer run the C++ protocol core (builders + session + parsers)
// against an actual terminal over the LAN, without the native (Kotlin/Swift)
// transport. POSIX-only (Linux/macOS); excluded on Windows.
#if !defined(_WIN32)

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Transport/Transport.hpp"

namespace margelo::nitro::ecr17 {

class PosixTcpTransport : public Transport {
   public:
    PosixTcpTransport(std::string host, int port) : host_(std::move(host)), port_(port) {}
    ~PosixTcpTransport() override { disconnect(); }

    void connect() override {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &res) != 0) {
            throw std::runtime_error("PosixTcpTransport: host resolution failed");
        }
        fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd_ < 0) {
            freeaddrinfo(res);
            throw std::runtime_error("PosixTcpTransport: socket() failed");
        }
        if (::connect(fd_, res->ai_addr, res->ai_addrlen) != 0) {
            freeaddrinfo(res);
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("PosixTcpTransport: connect() failed");
        }
        freeaddrinfo(res);
        running_ = true;
        reader_ = std::thread([this]() { readLoop(); });
    }

    void disconnect() override {
        running_ = false;
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
            ::close(fd_);
            fd_ = -1;
        }
        if (reader_.joinable()) {
            reader_.join();
        }
    }

    bool isConnected() const override { return fd_ >= 0; }

    void send(const std::vector<uint8_t>& bytes) override {
        if (fd_ >= 0) {
            ::send(fd_, bytes.data(), bytes.size(), 0);
        }
    }

    void setDataCallback(DataCallback cb) override { onData_ = std::move(cb); }
    void setDisconnectCallback(DisconnectCallback cb) override { onDisconnect_ = std::move(cb); }

   private:
    void readLoop() {
        uint8_t buffer[4096];
        while (running_) {
            ssize_t n = ::recv(fd_, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                break;
            }
            if (onData_) {
                onData_(std::vector<uint8_t>(buffer, buffer + n));
            }
        }
        if (onDisconnect_) {
            onDisconnect_();
        }
    }

    std::string host_;
    int port_;
    int fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread reader_;
    DataCallback onData_{};
    DisconnectCallback onDisconnect_{};
};

}  // namespace margelo::nitro::ecr17

#endif  // !_WIN32
