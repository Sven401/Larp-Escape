#pragma once

#include "GameDomain/GameDefinitions.h"

#include <cstdint>

namespace larp {
namespace ports {
class ICardReader {
  public:
    virtual ~ICardReader() {}

    virtual domain::CardUid readCard(std::uint16_t timeoutMilliseconds) = 0;
};
} // namespace ports
} // namespace larp
