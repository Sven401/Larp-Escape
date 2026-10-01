#include "GameLightEffects.h"

#include "CrystalPaletteMap.h"
#include "GamePorts/IClock.h"

#include <FastLED.h>

GameLightEffects::GameLightEffects(fl::MyTwinkleFox &twinkleFox, Burst &burst, Ripple ripples[4],
                                   larp::ports::IClock &clock)
    : twinkleFox(twinkleFox), burst(burst), ripples(ripples), clock(clock), brightnessRamps{}
{
}

void GameLightEffects::setTwinkleDensity(std::uint8_t density)
{
    twinkleFox.setTwinkleDensity(density);
}

void GameLightEffects::setTwinkleSpeed(std::uint8_t speed)
{
    twinkleFox.setTwinkleSpeed(speed);
}

void GameLightEffects::setMutedPalette()
{
    twinkleFox.targetPalette = MutedAllColors_p;
}

void GameLightEffects::setCrystalPalette(larp::domain::RoleId role,
                                         const larp::domain::GridPosition &position)
{
    const TProgmemRGBPalette16 *palette = larp::led::paletteForCrystal(role, position);
    if (palette != nullptr)
    {
        twinkleFox.targetPalette = *palette;
    }
}

void GameLightEffects::fireBurst(std::uint8_t frames)
{
    burst.fire(frames);
}

void GameLightEffects::fireBurst(std::uint8_t frames, larp::ports::RgbColor color)
{
    burst.fire(frames, CRGB(color.red, color.green, color.blue));
}

void GameLightEffects::fireRipple(std::uint8_t effectIndex, std::uint8_t frames,
                                  std::uint8_t origin)
{
    if (effectIndex < 4U)
    {
        ripples[effectIndex].fire(frames, origin);
    }
}

void GameLightEffects::stepBrightnessToward(larp::ports::BrightnessRamp ramp,
                                            std::uint8_t target,
                                            std::uint16_t intervalMilliseconds)
{
    const std::size_t index = static_cast<std::size_t>(ramp);
    if (index >= brightnessRamps.size())
    {
        return;
    }

    BrightnessRampState &state = brightnessRamps[index];
    const std::uint32_t now = clock.nowMilliseconds();
    if (!state.initialized)
    {
        state.initialized = true;
        state.lastUpdateMilliseconds = now;
        return;
    }
    if ((now - state.lastUpdateMilliseconds) < intervalMilliseconds)
    {
        return;
    }

    state.lastUpdateMilliseconds = now;
    std::uint8_t brightness = FastLED.getBrightness();
    if (brightness != target)
    {
        brightness += (brightness < target) ? 1 : -1;
        FastLED.setBrightness(brightness);
    }
}
