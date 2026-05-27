#pragma once
#include <vector>
#include "HybridEcr17Spec.hpp"

namespace margelo::nitro::ecr17 {
class HybridEcr17 : public HybridEcr17Spec {
    public:
        HybridEcr17() : HybridObject(TAG), HybridEcr17Spec() {}
       
        double sum(double a, double b) override;
    };
} // namespace margelo::nitro::ecr17
