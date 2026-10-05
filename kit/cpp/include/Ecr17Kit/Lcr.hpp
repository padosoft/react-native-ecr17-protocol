#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Ecr17Kit/LrcMode.hpp"

namespace padosoft::ecr17 {

class Lrc {
   public:
    static constexpr uint8_t BASE = 0x7F;

    static uint8_t compute(const std::vector<uint8_t>& payload, LrcMode mode);

    static uint8_t compute(const std::string& payload, LrcMode mode);
};

}  // namespace padosoft::ecr17