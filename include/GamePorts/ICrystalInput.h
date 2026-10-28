#pragma once

#include "GameDomain/GridPosition.h"

#include "GamePorts/CrystalInputBatch.h"

namespace larp {
namespace ports {
// A complete synchronous scan. Partial or failed scans are not game input.
struct CrystalInputSample {
    CrystalInputBatch positions;
    bool valid = false;
};
class ICrystalInput {
  public:
    virtual ~ICrystalInput() {}

    virtual CrystalInputSample readPressedPositions() = 0;
};
} // namespace ports
} // namespace larp
