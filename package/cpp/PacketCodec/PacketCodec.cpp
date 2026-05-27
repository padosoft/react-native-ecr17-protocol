#include "PacketCodec.hpp"

namespace margelo::nitro::ecr17 {

PacketCodec::PacketCodec(LrcMode mode) : lrcMode_(mode) {}

std::vector<uint8_t> PacketCodec::encodeApplication(const std::string& payload) {
    std::vector<uint8_t> frame;

    frame.push_back(STX);

    frame.insert(frame.end(), payload.begin(), payload.end());

    frame.push_back(ETX);

    uint8_t lrc = Lrc::compute(payload, lrcMode_);

    frame.push_back(lrc);

    return frame;
}

std::vector<uint8_t> PacketCodec::encodeControl(uint8_t ctrl) {
    std::vector<uint8_t> frame;

    frame.push_back(ctrl);
    frame.push_back(ETX);

    uint8_t lrc = Lrc::compute(std::vector<uint8_t>{ctrl}, lrcMode_);

    frame.push_back(lrc);

    return frame;
}

DecodedPacket PacketCodec::decode(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return {
            PacketType::UNKNOWN,
            "",
            false,
        };
    }

    uint8_t first = data[0];

    if (first == ACK) {
        return {
            PacketType::ACK,
            "",
            true,
        };
    }

    if (first == NAK) {
        return {
            PacketType::NAK,
            "",
            true,
        };
    }

    if (first == SOH) {
        std::string payload(data.begin() + 1, data.end() - 1);

        return {
            PacketType::STATUS,
            payload,
            true,
        };
    }

    if (first == STX) {
        auto etxIt = std::find(data.begin(), data.end(), ETX);

        if (etxIt == data.end()) {
            return {
                PacketType::UNKNOWN,
                "",
                false,
            };
        }

        size_t etxIndex = std::distance(data.begin(), etxIt);

        std::string payload(data.begin() + 1, data.begin() + etxIndex);

        uint8_t rxLrc = data.back();

        uint8_t calcLrc = Lrc::compute(payload, lrcMode_);

        return {
            PacketType::APPLICATION,
            payload,
            rxLrc == calcLrc,
        };
    }

    return {
        PacketType::UNKNOWN,
        "",
        false,
    };
}

}  // namespace margelo::nitro::ecr17