#include "Lcr.hpp"

namespace margelo::nitro::ecr17 {

static constexpr uint8_t STX = 0x02;
static constexpr uint8_t ETX = 0x03;

uint8_t Lrc::compute(const std::vector<uint8_t>& payload, LrcMode mode) {
    uint8_t lrc = BASE;

    switch (mode) {
        case LrcMode::STX:
        case LrcMode::STX_NOEXT:
            lrc ^= STX;
            break;

        default:
            break;
    }

    for (auto b : payload) {
        lrc ^= b;
    }

    switch (mode) {
        case LrcMode::STX:
        case LrcMode::NOEXT:
            lrc ^= ETX;
            break;

        default:
            break;
    }

    return lrc;
}

uint8_t Lrc::compute(const std::string& payload, LrcMode mode) {
    return compute(std::vector<uint8_t>(payload.begin(), payload.end()), mode);
}

}  // namespace margelo::nitro::ecr17