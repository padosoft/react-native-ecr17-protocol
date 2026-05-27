// cpp/core/Lrc.hpp

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "LrcMode.hpp"

namespace margelo::nitro::ecr17 {

class Lrc {
   public:
    static constexpr uint8_t BASE = 0x7F;

    static uint8_t compute(const std::vector<uint8_t>& payload, LrcMode mode);

    static uint8_t compute(const std::string& payload, LrcMode mode);
};

}  // namespace margelo::nitro::ecr17