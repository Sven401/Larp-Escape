#pragma once
#include "Companion/DeviceIdentityStore.h"
// Trusted startup only: no RF/ADC/I2S/device owners may use that resource yet.
// Enables the pinned boot entropy source for one fill and releases it afterward.
// Public random label only; no verifier/key or measured entropy-quality claim.
class Esp32StartupIdentityEntropy : public larp::companion::IDeviceIdentityEntropy {
  public:
    bool make(larp::companion::Identity &destination) noexcept override;
};
