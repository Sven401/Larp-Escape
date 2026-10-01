#pragma once

#include "GamePorts/ILightEffects.h"
#include "burst.h"
#include "ripple.h"
#include "twinklefox.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace larp
{
namespace ports
{
class IClock;
}
} // namespace larp

class GameLightEffects : public larp::ports::ILightEffects
{
public:
    GameLightEffects(fl::MyTwinkleFox &twinkleFox, Burst &burst, Ripple ripples[4],
                     larp::ports::IClock &clock);

    void setTwinkleDensity(std::uint8_t density) override;
    void setTwinkleSpeed(std::uint8_t speed) override;
    void setMutedPalette() override;
    void setCrystalPalette(larp::domain::RoleId role,
                           const larp::domain::GridPosition &position) override;
    void fireBurst(std::uint8_t frames) override;
    void fireBurst(std::uint8_t frames, larp::ports::RgbColor color) override;
    void fireRipple(std::uint8_t effectIndex, std::uint8_t frames, std::uint8_t origin) override;
    void stepBrightnessToward(larp::ports::BrightnessRamp ramp, std::uint8_t target,
                              std::uint16_t intervalMilliseconds) override;

private:
    struct BrightnessRampState
    {
        bool initialized;
        std::uint32_t lastUpdateMilliseconds;
    };

    static const std::size_t brightnessRampCount =
        static_cast<std::size_t>(larp::ports::BrightnessRamp::Count);

    fl::MyTwinkleFox &twinkleFox;
    Burst &burst;
    Ripple *ripples;
    larp::ports::IClock &clock;
    std::array<BrightnessRampState, brightnessRampCount> brightnessRamps;
};
