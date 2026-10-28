#pragma once

#include <array>

namespace larp {
namespace ports {
// A synchronous group observation. Invalid samples must not trigger wake or
// per-input effects, even if some individual levels were successfully read.
struct WakeInputSample {
    std::array<bool, 4> signals = {};
    bool valid = false;
};
class IWakeInput {
  public:
    virtual ~IWakeInput() {}
    virtual WakeInputSample readWakeSignals() = 0;
};
} // namespace ports
} // namespace larp
