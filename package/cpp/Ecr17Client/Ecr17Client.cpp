#include "Ecr17Client.hpp"

#include <stdexcept>

namespace margelo::nitro::ecr17 {
void HybridEcr17Client::configure(const Ecr17Config& config) { config_ = config; }

Ecr17Config HybridEcr17Client::configuration() { return config_; }

PosStatusResponse HybridEcr17Client::status() {
    // NOTE: not implemented yet. Producing a real PosStatusResponse requires a
    // configured Transport, sending the 's' status request and parsing the
    // terminal's status response message. Until that flow exists we throw
    // instead of falling off the end of a non-void function (undefined
    // behaviour, which previously returned a garbage struct to JS).
    throw std::runtime_error(
        "Ecr17Client::status() is not implemented yet: missing transport send/receive and "
        "status response parsing");
}

}  // namespace margelo::nitro::ecr17