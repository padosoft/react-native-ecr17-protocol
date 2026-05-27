#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Lcr.hpp"

namespace margelo::nitro::ecr17 {

enum class PacketType {
    APPLICATION,
    STATUS,
    ACK,
    NAK,
    UNKNOWN,
};

struct DecodedPacket {
    PacketType type;
    std::string payload;
    bool validLrc;
};

class PacketCodec {
   public:
    static constexpr uint8_t STX = 0x02;
    static constexpr uint8_t ETX = 0x03;
    static constexpr uint8_t SOH = 0x01;
    static constexpr uint8_t EOT = 0x04;
    static constexpr uint8_t ACK = 0x06;
    static constexpr uint8_t NAK = 0x15;

    explicit PacketCodec(LrcMode mode);

    std::vector<uint8_t> encodeApplication(const std::string& payload);

    std::vector<uint8_t> encodeControl(uint8_t ctrl);

    DecodedPacket decode(const std::vector<uint8_t>& data);

   private:
    LrcMode lrcMode_;
};

}  // namespace margelo::nitro::ecr17