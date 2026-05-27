#include "Ecr17Client.hpp"

namespace margelo::nitro::ecr17 {
void HybridEcr17Client::configure(const Ecr17Config& config) { config_ = config; }

Ecr17Config HybridEcr17Client::configuration() { return config_; }

PosStatusResponse HybridEcr17Client::status() {};

}  // namespace margelo::nitro::ecr17