#pragma once

#include <cstdint>

namespace larp {
namespace led {

enum class Effect : std::uint8_t { Off = 0, Twinkle = 1, Burst = 2, Ripple = 3 };

struct OutputSnapshot {
    Effect requested = Effect::Off;
    Effect applied = Effect::Off;
    // Tokens identify explicit requests. Automatic one-shot completion retains
    // the start token; the transport publishes its own monotonically new sample.
    std::uint32_t revision = 0;
    std::uint32_t appliedRevision = 0;
    bool ready = false;
    bool pending = false;
    bool off = false; // True only after a fresh black frame completed transmission.
    bool completed = false; // One-shot ended and its black frame was transmitted.
    bool fault = false;
};

// All methods and WLED's strip.service() belong to the same Arduino loop owner.
// BLE callbacks must only copy requests into that owner's queue. WLED is the
// sole pixel renderer and bus owner; this adapter never drives GPIO itself.
class WledLedOutput {
  public:
    WledLedOutput() = default;
    WledLedOutput(const WledLedOutput &) = delete;
    WledLedOutput &operator=(const WledLedOutput &) = delete;

    // Initializes the fixed product bus and schedules black. ready remains false
    // until that frame completed. Call once, before any WLED setup or service.
    bool begin();
    // Acceptance only. Match appliedRevision and pending == false for completion.
    // A new start replaces any pending/running effect, including the same effect.
    bool start(Effect effect);
    bool stop();
    // Call before WLED strip.service(). This method never waits for a bus/frame.
    void service();
    OutputSnapshot snapshot() const { return status_; }

    static constexpr std::uint16_t LedCount = 23;
    static constexpr std::uint8_t LedPin = 15;
    static constexpr std::uint8_t Brightness = 32;
    static constexpr std::uint16_t Milliamps = 1000;
    // WLED Fade: triwave16(now * ((255 >> 3) + 10)), one complete period.
    static constexpr std::uint32_t BurstMilliseconds = (65536U + 40U) / 41U;

  private:
    static WledLedOutput *owner_;
    static void renderSingleRipple();
    bool accept(Effect effect);
    bool hardwareValid() const;
    bool blackFrame() const;
    void configureRequested();
    void finishOneShot();
    void fail();

    OutputSnapshot status_;
    std::uint32_t baselineShow_ = 0;
    std::uint32_t configuredAt_ = 0;
    std::uint32_t pendingSince_ = 0;
    std::uint8_t rippleMode_ = 255;
    bool initialized_ = false;
    bool needsConfiguration_ = false;
    bool awaitingFrame_ = false;
    bool completionPending_ = false;
    bool rippleFinished_ = false;
    bool rippleFailed_ = false;
};

} // namespace led
} // namespace larp
