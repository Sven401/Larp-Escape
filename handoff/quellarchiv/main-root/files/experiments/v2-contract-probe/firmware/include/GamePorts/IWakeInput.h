#pragma once

#include <array>

namespace larp {
namespace ports {
class IWakeInput {
  public:
    virtual ~IWakeInput() {}

    virtual std::array<bool, 4> readWakeSignals() = 0;
};
} // namespace ports
} // namespace larp
