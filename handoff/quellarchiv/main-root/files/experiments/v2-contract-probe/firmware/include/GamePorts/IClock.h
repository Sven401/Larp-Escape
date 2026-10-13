#pragma once

#include <cstdint>

namespace larp {
namespace ports {
class IClock {
  public:
    virtual ~IClock() {}

    virtual std::uint32_t nowMilliseconds() const = 0;
    virtual void delayMilliseconds(std::uint32_t duration) = 0;
};
} // namespace ports
} // namespace larp
