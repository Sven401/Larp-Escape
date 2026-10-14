#include "WledLedOutput.h"

#include "wled.h"
#include <cstring>
#include <limits>

// Native WLED renderer at the pinned upstream commit. The wrapper below only
// controls its wave lifecycle; all propagation, blending and pixel math is WLED.
extern void mode_ripple();

namespace larp {
namespace led {
namespace {
constexpr std::uint32_t FrameTimeoutMilliseconds = 2000;
constexpr std::uint8_t NativeRippleStride = 4;
constexpr std::uint8_t NativeRipplePositionOffset = 2;
constexpr std::uint8_t RippleSpeed = 128;
const char SingleRippleMetadata[] PROGMEM = "Larp Ripple Once@;!,!;!;1";
}

constexpr std::uint16_t WledLedOutput::LedCount;
constexpr std::uint8_t WledLedOutput::LedPin;
constexpr std::uint8_t WledLedOutput::Brightness;
constexpr std::uint16_t WledLedOutput::Milliamps;
constexpr std::uint32_t WledLedOutput::BurstMilliseconds;
WledLedOutput *WledLedOutput::owner_ = nullptr;

bool WledLedOutput::hardwareValid() const {
    const Bus *bus = BusManager::getBus(0);
    std::uint8_t pins[OUTPUT_MAX_PINS] = {};
    return BusManager::getNumBusses() == 1 && bus != nullptr && bus->isOk() &&
           !bus->isPlaceholder() && bus->getType() == TYPE_WS2812_RGB &&
           bus->getStart() == 0 && bus->getLength() == LedCount &&
           bus->getColorOrder() == COL_ORDER_GRB && bus->getPins(pins) == 1 &&
           pins[0] == LedPin && strip.getLengthTotal() == LedCount &&
           strip.getSegmentsNum() == 1 && strip.getSegment(0).isActive();
}

bool WledLedOutput::begin() {
    if (owner_ != nullptr || initialized_ || strip.isServicing() ||
        strip.isUpdating() || BusManager::getNumBusses() != 0) return false;
    owner_ = this;

    // ProductMain intentionally bypasses WLED::setup(), its stored settings,
    // network interfaces, presets and timers. Initialize the necessary LED-only
    // pieces explicitly and leave the filesystem unmounted.
    turnOnAtBoot = false;
    bootPreset = 0;
    bri = briT = briOld = 0;
    briS = briLast = Brightness;
    briMultiplier = 100;
    transitionActive = false;
    nightlightActive = false;
    realtimeMode = REALTIME_MODE_INACTIVE;
    realtimeOverride = REALTIME_OVERRIDE_NONE;
    gammaCorrectCol = true;
    gammaCorrectBri = false;
    NeoGammaWLEDMethod::calcGammaTable(gammaCorrectVal);
    strip.setTransition(0);
    strip.setBrightness(0);
    strip.setTargetFps(30);
    strip.isMatrix = false;
    BusManager::setMilliampsMax(Milliamps);
    busConfigs.clear();
    std::uint8_t pins[] = {LedPin};
    // WLED TYPE_WS2812_RGB is its 800-kHz WS2811-compatible GRB bus.
    // Driver 0 selects WLED/NeoPixelBus RMT. 60 mA is the full-white estimate.
    busConfigs.emplace_back(TYPE_WS2812_RGB, pins, 0, LedCount, COL_ORDER_GRB,
                            false, 0, RGBW_MODE_MANUAL_ONLY, 0, 60, Milliamps, 0);
    strip.finalizeInit();
    strip.resetSegments();
    if (strip.getSegmentsNum() == 1) strip.getSegment(0).refreshLightCapabilities();
    rippleMode_ = strip.addEffect(255, &renderSingleRipple, SingleRippleMetadata);
    if (!hardwareValid() || rippleMode_ == 255 || errorFlag == ERR_NORAM ||
        errorFlag == ERR_NORAM_PX) {
        status_.fault = true;
        // No usable service is advertised by the caller after begin fails.
        strip.setBrightness(0);
        strip.suspend();
        return false;
    }
    initialized_ = true;
    return accept(Effect::Off);
}

bool WledLedOutput::accept(Effect effect) {
    if (!initialized_) return false;
    if (status_.revision == std::numeric_limits<std::uint32_t>::max()) {
        fail(); // Counter exhaustion still requests darkness; never wrap a token.
        return false;
    }
    ++status_.revision;
    pendingSince_ = millis();
    status_.requested = effect;
    status_.pending = true;
    status_.completed = false;
    completionPending_ = false;
    needsConfiguration_ = true;
    awaitingFrame_ = false;
    return true;
}

bool WledLedOutput::start(Effect effect) {
    if (!status_.ready || status_.fault ||
        (effect != Effect::Twinkle && effect != Effect::Burst && effect != Effect::Ripple))
        return false;
    return accept(effect);
}

bool WledLedOutput::stop() {
    // Stop remains available while a start is pending and during fault recovery.
    return accept(Effect::Off);
}

void WledLedOutput::configureRequested() {
    // The caller checked both rendering and asynchronous output are idle.
    strip.setTransition(0);
    strip.setTransitionMode(false);
    strip.restartRuntime(); // Also resets an explicitly restarted identical mode.
    strip.resetTimebase();
    Segment &segment = strip.getSegment(0);
    segment.setOption(SEG_OPTION_FREEZE, false);
    segment.setOption(SEG_OPTION_ON, status_.requested != Effect::Off);
    segment.setOpacity(255);
    segment.setColor(0, status_.requested == Effect::Ripple ? 0x008080FF : 0x00FFFFFF);
    segment.setColor(1, BLACK);
    segment.setColor(2, BLACK);
    segment.setPalette(2); // Fixed primary color, no inherited/random palette.
    segment.speed = status_.requested == Effect::Burst ? 255 : RippleSpeed;
    segment.intensity = status_.requested == Effect::Twinkle ? 128 : 0;
    segment.custom1 = segment.custom2 = segment.custom3 = 0;
    segment.check1 = segment.check2 = segment.check3 = false;
    std::uint8_t mode = FX_MODE_STATIC;
    if (status_.requested == Effect::Twinkle) mode = FX_MODE_TWINKLEFOX;
    else if (status_.requested == Effect::Burst) mode = FX_MODE_FADE;
    else if (status_.requested == Effect::Ripple) mode = rippleMode_;
    segment.setMode(mode);
    segment.markForReset();
    // Sync globals without stateUpdated(), which would invoke network interfaces.
    bri = briT = briOld = status_.requested == Effect::Off ? 0 : Brightness;
    strip.setBrightness(bri);
    offMode = status_.requested == Effect::Off;
    rippleFinished_ = rippleFailed_ = false;
    baselineShow_ = strip.getLastShow();
    configuredAt_ = millis();
    awaitingFrame_ = true;
    needsConfiguration_ = false;
    strip.trigger();
}

bool WledLedOutput::blackFrame() const {
    if (strip.getBrightness() != 0) return false;
    for (std::uint16_t pixel = 0; pixel < LedCount; ++pixel)
        if (strip.getPixelColorNoMap(pixel) != BLACK) return false;
    return true;
}

void WledLedOutput::finishOneShot() {
    status_.requested = Effect::Off;
    status_.pending = true;
    completionPending_ = true;
    pendingSince_ = millis();
    needsConfiguration_ = true;
    awaitingFrame_ = false;
}

void WledLedOutput::fail() {
    status_.fault = true;
    status_.ready = false;
    status_.completed = false;
    completionPending_ = false;
    if (status_.requested != Effect::Off) {
        status_.requested = Effect::Off;
        status_.pending = true;
        needsConfiguration_ = true;
        awaitingFrame_ = false;
    }
}

void WledLedOutput::service() {
    if (!initialized_) return;
    if (!hardwareValid()) { fail(); strip.suspend(); return; }
    if (rippleFailed_) fail();
    const std::uint32_t now = millis();
    if (status_.pending && now - pendingSince_ >= FrameTimeoutMilliseconds) fail();
    if (strip.isServicing() || strip.isUpdating() || !hardwareValid()) return;

    if (needsConfiguration_) {
        configureRequested();
        return;
    }
    if (awaitingFrame_ && status_.requested == Effect::Burst &&
        strip.getLastShow() == baselineShow_ && now - configuredAt_ >= BurstMilliseconds) {
        // A stalled loop must not start the repeating Fade in its second cycle.
        fail();
        configureRequested();
        return;
    }
    if (awaitingFrame_ && strip.getLastShow() != baselineShow_) {
        if (status_.requested == Effect::Off && !blackFrame()) {
            fail();
            return;
        }
        status_.applied = status_.requested;
        status_.appliedRevision = status_.revision;
        status_.pending = false;
        status_.off = status_.applied == Effect::Off;
        status_.completed = completionPending_;
        status_.ready = !status_.fault;
        awaitingFrame_ = false;
    }
    // This runs before the next native WLED frame. A Fade cannot enter its next
    // period, and a finished native Ripple cannot spawn a replacement wave.
    if (!awaitingFrame_ &&
        ((status_.requested == Effect::Burst && now - configuredAt_ >= BurstMilliseconds) ||
         (status_.requested == Effect::Ripple && rippleFinished_))) {
        finishOneShot();
        configureRequested();
    }
}

void WledLedOutput::renderSingleRipple() {
    if (owner_ == nullptr || SEGLEN != LedCount || SEGMENT.is2D()) {
        SEGMENT.fill(BLACK);
        if (owner_ != nullptr) owner_->rippleFailed_ = true;
        return;
    }
    // Layout of WLED 16.0.1's Ripple at pinned commit 29b389df:
    // { uint8_t state; uint8_t color; uint16_t pos; }, stride 4. Byte access and
    // memcpy avoid inventing a different C++ type for the upstream allocation.
    static_assert(sizeof(std::uint16_t) == 2, "WLED ripple position width changed");
    const unsigned slots = 1 + (SEGLEN >> 2);
    mode_ripple();
    if (SEGENV.data == nullptr) {
        SEGMENT.fill(BLACK); // Native allocation fallback must not display white.
        owner_->rippleFailed_ = true;
        return;
    }
    if (SEGENV.call == 0) {
        std::memset(SEGENV.data, 0, slots * NativeRippleStride);
        SEGENV.data[0] = 1;
        const std::uint16_t origin = LedCount / 2;
        std::memcpy(SEGENV.data + NativeRipplePositionOffset, &origin, sizeof(origin));
        return;
    }
    // intensity == 0 alone is insufficient: upstream uses random <= intensity.
    // New waves have state 1 and have not drawn any pixels yet; remove them.
    for (unsigned index = 0; index < slots; ++index) {
        std::uint8_t &waveState = SEGENV.data[index * NativeRippleStride];
        if (waveState == 1) waveState = 0;
    }
    if (SEGENV.data[0] == 0) {
        owner_->rippleFinished_ = true;
        SEGMENT.fill(BLACK);
    }
}

} // namespace led
} // namespace larp
