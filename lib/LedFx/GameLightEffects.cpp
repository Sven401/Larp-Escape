#include "GameLightEffects.h"

#include "CrystalPaletteMap.h"
#include "GamePorts/IClock.h"
#include "GameRuntime/LedIndex.h"

#include <FastLED.h>

namespace {
void updateMaximum(std::atomic<std::uint32_t> &maximum, std::uint32_t candidate) {
    std::uint32_t observed = maximum.load(std::memory_order_relaxed);
    while (candidate > observed &&
           !maximum.compare_exchange_weak(observed, candidate, std::memory_order_relaxed)) {
    }
}
} // namespace

GameLightEffects::GameLightEffects(fl::MyTwinkleFox &twinkleFox, Burst &burst, Ripple ripples[4],
                                   larp::ports::IClock &clock, CRGB *ledBuffer,
                                   std::size_t ledCount)
    : twinkleFox(twinkleFox), burst(burst), ripples(ripples), clock(clock), ledBuffer(ledBuffer),
      ledCount(ledCount), brightnessCommands{}, commandQueue(nullptr), taskHandle(nullptr),
      droppedCommands(0U), minimumStackFreeBytesObserved(4096U), maximumQueueDepthObserved(0U),
      maximumRenderDurationObserved(0U), missedFrameDeadlines(0U) {}

bool GameLightEffects::begin() {
    if (ledBuffer == nullptr || ledCount == 0U || ripples == nullptr || commandQueue != nullptr ||
        taskHandle != nullptr) {
        return false;
    }

    commandQueue = xQueueCreate(commandQueueCapacity, sizeof(Command));
    if (commandQueue == nullptr) {
        return false;
    }

    const BaseType_t created =
        xTaskCreatePinnedToCore(taskEntry, "LED Task", 4096U, this, 1U, &taskHandle, 1);
    if (created != pdPASS || taskHandle == nullptr) {
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
        taskHandle = nullptr;
        return false;
    }
    return true;
}

std::uint32_t GameLightEffects::droppedCommandCount() const {
    return droppedCommands.load(std::memory_order_relaxed);
}

std::uint32_t GameLightEffects::minimumStackFreeBytes() const {
    return minimumStackFreeBytesObserved.load(std::memory_order_relaxed);
}

std::uint32_t GameLightEffects::maximumQueueDepth() const {
    return maximumQueueDepthObserved.load(std::memory_order_relaxed);
}

std::uint32_t GameLightEffects::maximumRenderDurationMicroseconds() const {
    return maximumRenderDurationObserved.load(std::memory_order_relaxed);
}

std::uint32_t GameLightEffects::frameDeadlineMissCount() const {
    return missedFrameDeadlines.load(std::memory_order_relaxed);
}

void GameLightEffects::taskEntry(void *context) {
    static_cast<GameLightEffects *>(context)->renderLoop();
}

void GameLightEffects::renderLoop() {
    TickType_t frameInterval = pdMS_TO_TICKS(33U);
    if (frameInterval == 0U) {
        frameInterval = 1U;
    }
    TickType_t nextFrame = xTaskGetTickCount();
    Command command = {};

    for (;;) {
        updateMaximum(maximumQueueDepthObserved,
                      static_cast<std::uint32_t>(uxQueueMessagesWaiting(commandQueue)));
        // A refilling producer cannot delay rendering beyond this command budget.
        for (std::size_t processed = 0U; processed < commandQueueCapacity &&
                                         xQueueReceive(commandQueue, &command, 0U) == pdTRUE;
             ++processed) {
            applyCommand(command);
        }
        const std::uint32_t renderStarted = micros();
        renderFrame();
        updateMaximum(maximumRenderDurationObserved, micros() - renderStarted);
        const std::uint32_t stackFreeBytes =
            static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
        std::uint32_t observedMinimum =
            minimumStackFreeBytesObserved.load(std::memory_order_relaxed);
        while (stackFreeBytes < observedMinimum &&
               !minimumStackFreeBytesObserved.compare_exchange_weak(observedMinimum, stackFreeBytes,
                                                                    std::memory_order_relaxed)) {
        }
        if (xTaskDelayUntil(&nextFrame, frameInterval) != pdTRUE) {
            missedFrameDeadlines.fetch_add(1U, std::memory_order_relaxed);
            // Skip missed frames instead of rendering bursts to catch up.
            nextFrame = xTaskGetTickCount();
            vTaskDelay(1U);
        }
    }
}

void GameLightEffects::applyCommand(const Command &command) {
    switch (command.type) {
    case CommandType::SetTwinkleDensity:
        twinkleFox.setTwinkleDensity(command.first);
        break;
    case CommandType::SetTwinkleSpeed:
        twinkleFox.setTwinkleSpeed(command.first);
        break;
    case CommandType::SetMutedPalette:
        twinkleFox.targetPalette = MutedAllColors_p;
        break;
    case CommandType::SetCrystalPalette: {
        const TProgmemRGBPalette16 *palette =
            larp::led::paletteForCrystal(command.role, command.position);
        if (palette != nullptr) {
            twinkleFox.targetPalette = *palette;
        }
        break;
    }
    case CommandType::FireBurst:
        burst.fire(command.first);
        break;
    case CommandType::FireColoredBurst:
        burst.fire(command.first, CRGB(command.second, command.third, command.fourth));
        break;
    case CommandType::FireRipple:
        if (command.first < 4U) {
            ripples[command.first].fire(command.second, command.third);
        }
        break;
    case CommandType::StepBrightness: {
        if (command.first >= brightnessRampCount) {
            break;
        }
        // The application schedule already established that this step is due.
        // Checking time again here would lose steps to render-frame jitter.
        std::uint8_t brightness = FastLED.getBrightness();
        if (brightness != command.second) {
            brightness += (brightness < command.second) ? 1 : -1;
            FastLED.setBrightness(brightness);
        }
        break;
    }
    }
}

void GameLightEffects::renderFrame() {
    const std::uint32_t now = clock.nowMilliseconds();
    twinkleFox.draw(fl::Fx::DrawContext(now, fl::span<CRGB>(ledBuffer, ledCount)));
    burst.draw();
    for (std::size_t index = 0; index < 4U; ++index) {
        ripples[index].draw();
    }
    FastLED.show();
}

bool GameLightEffects::enqueue(const Command &command) {
    if (commandQueue == nullptr) {
        droppedCommands.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }
    if (xQueueSend(commandQueue, &command, 0U) != pdTRUE) {
        droppedCommands.fetch_add(1U, std::memory_order_relaxed);
        updateMaximum(maximumQueueDepthObserved, static_cast<std::uint32_t>(commandQueueCapacity));
        return false;
    }
    // Sampled occupancy; a consumer may already have removed the new command.
    updateMaximum(maximumQueueDepthObserved,
                  static_cast<std::uint32_t>(uxQueueMessagesWaiting(commandQueue)));
    return true;
}

void GameLightEffects::setTwinkleDensity(std::uint8_t density) {
    Command command = {};
    command.type = CommandType::SetTwinkleDensity;
    command.first = density;
    enqueue(command);
}

void GameLightEffects::setTwinkleSpeed(std::uint8_t speed) {
    Command command = {};
    command.type = CommandType::SetTwinkleSpeed;
    command.first = speed;
    enqueue(command);
}

void GameLightEffects::setMutedPalette() {
    Command command = {};
    command.type = CommandType::SetMutedPalette;
    enqueue(command);
}

void GameLightEffects::setCrystalPalette(larp::domain::RoleId role,
                                         const larp::domain::GridPosition &position) {
    Command command = {};
    command.type = CommandType::SetCrystalPalette;
    command.role = role;
    command.position = position;
    enqueue(command);
}

void GameLightEffects::fireBurst(std::uint8_t frames) {
    Command command = {};
    command.type = CommandType::FireBurst;
    command.first = frames;
    enqueue(command);
}

void GameLightEffects::fireBurst(std::uint8_t frames, larp::ports::RgbColor color) {
    Command command = {};
    command.type = CommandType::FireColoredBurst;
    command.first = frames;
    command.second = color.red;
    command.third = color.green;
    command.fourth = color.blue;
    enqueue(command);
}

void GameLightEffects::fireRipple(std::uint8_t effectIndex, std::uint8_t frames,
                                  std::uint8_t origin) {
    if (effectIndex >= 4U || !larp::runtime::isLedOriginValid(origin, ledCount)) {
        return;
    }
    Command command = {};
    command.type = CommandType::FireRipple;
    command.first = effectIndex;
    command.second = frames;
    command.third = origin;
    enqueue(command);
}

void GameLightEffects::stepBrightnessToward(larp::ports::BrightnessRamp ramp, std::uint8_t target,
                                            std::uint16_t intervalMilliseconds) {
    const std::size_t index = static_cast<std::size_t>(ramp);
    if (index >= brightnessCommands.size()) {
        return;
    }

    larp::runtime::BrightnessStepSchedule &state = brightnessCommands[index];
    const std::uint32_t now = clock.nowMilliseconds();
    if (!state.due(now, intervalMilliseconds)) {
        return;
    }

    Command command = {};
    command.type = CommandType::StepBrightness;
    command.first = static_cast<std::uint8_t>(index);
    command.second = target;
    if (enqueue(command)) {
        state.markQueued(now);
    }
}
