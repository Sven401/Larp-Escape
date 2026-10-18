#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace larp { namespace led {

enum class Effect : std::uint8_t { Off = 0, Twinkle = 1, Burst = 2, Ripple = 3, Resonance = 4 };
struct Pixel { std::uint8_t red = 0, green = 0, blue = 0; };
constexpr std::size_t PixelCount = 23;
using Frame = std::array<Pixel, PixelCount>;
enum class TxResult { Busy, Complete, Failed };

// One loop owns this interface. submit() retains/copies a frame until poll()
// reports completion. reset() aborts a broken transfer; it never confirms light.
class LedBus {
public:
    virtual ~LedBus() = default;
    virtual bool begin() = 0;
    virtual bool submit(const Frame &frame) = 0;
    virtual TxResult poll() = 0;
    virtual bool reset() = 0;
};

struct OutputSnapshot {
    Effect requested = Effect::Off, applied = Effect::Off;
    std::uint32_t revision = 0, appliedRevision = 0;
    bool ready = false, pending = false, off = false, completed = false, fault = false;
};

class LedOutput {
public:
    using Clock = std::uint32_t (*)();
    LedOutput(); // Product constructor: the sole SDK RMT bus and millis().
    LedOutput(LedBus &bus, Clock clock) : bus_(bus), clock_(clock) {}
    LedOutput(const LedOutput &) = delete;
    LedOutput &operator=(const LedOutput &) = delete;
    bool begin();
    bool start(Effect effect);
    bool stop();
    // Owner supplies the current raw Wake observation; unknown never retains light.
    void updateWake(bool valid, std::uint8_t mask);
    void service();
    void setTwinkleIntensity(unsigned percent) { twinkleIntensity_ = percent > 100 ? 100 : percent; }
    void tick() { service(); }
    OutputSnapshot snapshot() const { return status_; }

    static constexpr std::uint16_t LedCount = PixelCount;
    static constexpr std::uint8_t LedPin = 15, Brightness = 32;
    static constexpr std::uint16_t Milliamps = 1000;
    static constexpr std::uint32_t BurstMilliseconds = 1600, RippleMilliseconds = 1400;
    static constexpr std::uint32_t FrameMilliseconds = 33, TransferTimeoutMilliseconds = 2000;
    static constexpr std::uint32_t ResonancePeriodMilliseconds = 2400, ResonanceMergeMilliseconds = 800;
private:
    bool accept(Effect effect);
    void fail();
    void requestAutomaticOff();
    void render(std::uint32_t elapsed);
    LedBus &bus_;
    Clock clock_;
    OutputSnapshot status_;
    Frame frame_{};
    unsigned twinkleIntensity_ = 100;
    bool initialized_ = false, dirty_ = false, inFlight_ = false, active_ = false;
    bool completionPending_ = false, txCompletion_ = false, txBlack_ = true;
    bool recoveryNeeded_ = false, recoveryAttempted_ = false;
    std::uint32_t startedAt_ = 0, lastSubmit_ = 0, txStartedAt_ = 0, recoveryAt_ = 0;
    std::uint32_t txRevision_ = 0;
    Effect txEffect_ = Effect::Off;
    std::uint8_t wakeMask_ = 0;
    bool wakeValid_ = false, resonanceTogether_ = false, resonanceMerged_ = false;
    std::uint32_t resonanceTogetherAt_ = 0;
};

// Worst case all channels at the allowed maximum, plus 1 mA idle per pixel.
// This fixed product has no user-adjustable brightness or pixel count.
static_assert(PixelCount * 60U * 32U / 255U + PixelCount < 1000U,
              "The fixed brightness must remain below the product current budget");

}} // namespace larp::led
