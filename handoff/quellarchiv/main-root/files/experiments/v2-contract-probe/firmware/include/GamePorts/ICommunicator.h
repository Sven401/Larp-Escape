#pragma once

#include <cstdint>

namespace larp {
namespace ports {
enum class GameMessage : std::uint8_t { Activate };

class ICommunicator {
  public:
    virtual ~ICommunicator() {}

    virtual bool send(GameMessage message) = 0;
};
} // namespace ports
} // namespace larp
