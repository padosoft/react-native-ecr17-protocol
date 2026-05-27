#pragma once

#include "HybridEcr17ClientSpec.hpp"

namespace margelo::nitro::ecr17 {

class HybridEcr17Client : public HybridEcr17ClientSpec {
   public:
    HybridEcr17Client() : HybridObject(TAG) {}

    void configure(const Ecr17Config& config) override;
    Ecr17Config configuration() override;

    PosStatusResponse status() override;

   protected:
    Ecr17Config config_;
};

}  // namespace margelo::nitro::ecr17