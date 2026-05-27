#include "PacketCodec.hpp"

#include <algorithm>  // std::find
#include <iterator>   // std::distance

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
        // Progress update packet: SOH + message + EOT (no LRC). We need at least
        // SOH and the trailing EOT before stripping the first/last byte,
        // otherwise the iterator range below would be invalid (last < first).
        if (data.size() < 2) {
            return {
                PacketType::UNKNOWN,
                "",
                false,
            };
        }

        std::string payload(data.begin() + 1, data.end() - 1);

        return {
            PacketType::PROGRESS,
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

        // The LRC is the byte immediately after ETX (STX + payload + ETX + LRC).
        // If nothing follows ETX the frame is truncated and cannot be validated.
        if (etxIndex + 1 >= data.size()) {
            return {
                PacketType::UNKNOWN,
                "",
                false,
            };
        }

        std::string payload(data.begin() + 1, data.begin() + etxIndex);

        uint8_t rxLrc = data[etxIndex + 1];

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