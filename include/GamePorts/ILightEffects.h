#pragma once

#include "GameDomain/GameDefinitions.h"
#include "GameDomain/GridPosition.h"

#include <cstdint>

namespace larp
{
namespace ports
{
struct RgbColor
{
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

enum class BrightnessRamp : std::uint8_t
{
    Idle,
    WaitingForCrystals,
    FirstCrystalPlaced,
    SecondCrystalPlaced,
    ThirdCrystalPlaced,
    GameCompleted,
    Count
};

class ILightEffects
{
public:
    virtual ~ILightEffects() {}

    virtual void setTwinkleDensity(std::uint8_t density) = 0;
    virtual void setTwinkleSpeed(std::uint8_t speed) = 0;
    virtual void setMutedPalette() = 0;
    virtual void setCrystalPalette(domain::RoleId role, const domain::GridPosition &position) = 0;
    virtual void fireBurst(std::uint8_t frames) = 0;
    virtual void fireBurst(std::uint8_t frames, RgbColor color) = 0;
    virtual void fireRipple(std::uint8_t effectIndex, std::uint8_t frames, std::uint8_t origin) = 0;
    virtual void stepBrightnessToward(BrightnessRamp ramp, std::uint8_t target,
                                      std::uint16_t intervalMilliseconds) = 0;
};
} // namespace ports
} // namespace larp
