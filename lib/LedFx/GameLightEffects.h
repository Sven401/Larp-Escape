#pragma once

#include "GamePorts/ILightEffects.h"
#include "burst.h"
#include "ripple.h"
#include "twinklefox.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

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
                     larp::ports::IClock &clock, CRGB *ledBuffer, std::size_t ledCount);
    GameLightEffects(const GameLightEffects &) = delete;
    GameLightEffects &operator=(const GameLightEffects &) = delete;

    bool begin();
    std::uint32_t droppedCommandCount() const;

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
    enum class CommandType : std::uint8_t
    {
        SetTwinkleDensity,
        SetTwinkleSpeed,
        SetMutedPalette,
        SetCrystalPalette,
        FireBurst,
        FireColoredBurst,
        FireRipple,
        StepBrightness
    };

    struct Command
    {
        CommandType type;
        std::uint8_t first;
        std::uint8_t second;
        std::uint8_t third;
        std::uint8_t fourth;
        std::uint16_t intervalMilliseconds;
        larp::domain::RoleId role;
        larp::domain::GridPosition position;
    };

    struct BrightnessRampState
    {
        bool initialized;
        std::uint32_t lastUpdateMilliseconds;
    };

    struct BrightnessCommandState
    {
        bool initialized;
        std::uint32_t lastCommandMilliseconds;
    };

    static const std::size_t brightnessRampCount =
        static_cast<std::size_t>(larp::ports::BrightnessRamp::Count);

    static void taskEntry(void *context);
    void renderLoop();
    void applyCommand(const Command &command);
    void renderFrame();
    void enqueue(const Command &command);

    fl::MyTwinkleFox &twinkleFox;
    Burst &burst;
    Ripple *ripples;
    larp::ports::IClock &clock;
    CRGB *ledBuffer;
    std::size_t ledCount;
    std::array<BrightnessRampState, brightnessRampCount> brightnessRamps;
    std::array<BrightnessCommandState, brightnessRampCount> brightnessCommands;
    QueueHandle_t commandQueue;
    TaskHandle_t taskHandle;
    std::atomic<std::uint32_t> droppedCommands;
};
