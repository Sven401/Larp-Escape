#pragma once

#include "GameDomain/GridPosition.h"

#include <vector>

namespace larp
{
namespace ports
{
class ICrystalInput
{
public:
    virtual ~ICrystalInput() {}

    virtual std::vector<domain::GridPosition> readPressedPositions() = 0;
};
} // namespace ports
} // namespace larp
