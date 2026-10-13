#pragma once
#include "Companion/PeerProof.h"
namespace larp {
namespace companion {
// Running BLE/controller entropy only. Never enables boot entropy over active
// ADC/peripheral ownership. Caller also checks the original live ready service.
class Esp32PeerProofNonce : public IPeerProofNonce {
  public:
    bool make(PeerNonce &) noexcept override;
};
} // namespace companion
} // namespace larp
