#pragma once
#include "DeviceRuntime/Lifecycle.h"
namespace larp {
namespace devices {
struct OwnedComponent {
    configuration::Component settings;
    std::uint8_t slot = 0;
};
// Sole-owner storage for COPIED setup values. This checks setup ordering, but
// does not initialize a physical driver. Park/cleanup before begin; the worker
// may acknowledge Commit/Resume only after actual driver setup also succeeds.
class OwnerConfiguration {
  public:
    explicit OwnerConfiguration(Owner owner) : owner_(owner) {}
    OwnerConfiguration(const OwnerConfiguration &) = delete;
    OwnerConfiguration &operator=(const OwnerConfiguration &) = delete;
    bool begin(const Control &control);
    bool append(const Control &control);
    bool commit(const Control &control);
    bool ready(std::uint32_t generation) const;
    std::uint8_t count() const {
        return count_;
    }
    const OwnedComponent *get(std::uint8_t index) const;
    const OwnedComponent *find(ComponentId id) const;
    const std::array<I2cBus, 2> &buses() const {
        return i2c_;
    }
    std::uint8_t peerChannel() const {
        return peerChannel_;
    }

  private:
    Owner owner_;
    std::array<OwnedComponent, Limits::Components> components_ = {};
    std::array<I2cBus, 2> i2c_ = {};
    std::uint32_t generation_ = 0;
    std::uint8_t count_ = 0, expected_ = 0, peerChannel_ = 0;
    bool preparing_ = false, ready_ = false;
};
} // namespace devices
} // namespace larp
