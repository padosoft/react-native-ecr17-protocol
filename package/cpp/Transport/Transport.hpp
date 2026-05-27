#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace margelo::nitro::ecr17 {

using DataCallback = std::function<void(const std::vector<uint8_t>&)>;

using DisconnectCallback = std::function<void()>;

class Transport {
   public:
    virtual ~Transport() = default;

    virtual void connect() = 0;

    virtual void disconnect() = 0;

    virtual bool isConnected() const = 0;

    virtual void send(const std::vector<uint8_t>& bytes) = 0;

    virtual void setDataCallback(DataCallback cb) = 0;

    virtual void setDisconnectCallback(DisconnectCallback cb) = 0;
};

}  // namespace margelo::nitro::ecr17