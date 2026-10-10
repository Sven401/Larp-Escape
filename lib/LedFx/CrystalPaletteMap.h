#pragma once

#include <FastLED.h>

#include "GameDomain/GameDefinitions.h"

namespace larp
{
namespace led
{
const TProgmemRGBPalette16 *paletteForCrystal(domain::RoleId role, domain::GridPosition position);
} // namespace led
} // namespace larp
