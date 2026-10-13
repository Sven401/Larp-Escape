#pragma once

namespace larp {
namespace ports {
// Nonblocking progress, called only by the owning application loop.
class IRuntimeService {
  public:
    virtual ~IRuntimeService() {}
    virtual void service() = 0;
};
} // namespace ports
} // namespace larp
