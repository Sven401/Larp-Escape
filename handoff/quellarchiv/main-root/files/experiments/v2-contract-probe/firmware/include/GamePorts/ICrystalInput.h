#pragma once

#include "GameDomain/GridPosition.h"

#include "GamePorts/CrystalInputBatch.h"

namespace larp {
namespace ports {
class ICrystalInput {
  public:
    virtual ~ICrystalInput() {}

    virtual CrystalInputBatch readPressedPositions() = 0;
};
} // namespace ports
} // namespace larp
