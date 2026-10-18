#include "LedOutput.h"
#include <limits>

namespace larp { namespace led {
constexpr std::uint16_t LedOutput::LedCount;
constexpr std::uint8_t LedOutput::LedPin, LedOutput::Brightness;
constexpr std::uint16_t LedOutput::Milliamps;
constexpr std::uint32_t LedOutput::BurstMilliseconds, LedOutput::RippleMilliseconds;
constexpr std::uint32_t LedOutput::FrameMilliseconds, LedOutput::TransferTimeoutMilliseconds;
constexpr std::uint32_t LedOutput::ResonancePeriodMilliseconds, LedOutput::ResonanceMergeMilliseconds;

bool LedOutput::begin() {
    if (initialized_ || status_.fault) return false;
    if (!clock_ || !bus_.begin()) { status_.fault = true; return false; }
    initialized_ = true;
    status_.revision = 1;
    status_.pending = true;
    dirty_ = true; // Initial darkness is a real transfer, not an assumed state.
    return true;
}

bool LedOutput::start(Effect effect) {
    if (!status_.ready || status_.fault ||
        (effect != Effect::Twinkle && effect != Effect::Burst && effect != Effect::Ripple &&
         effect != Effect::Resonance)) return false;
    return accept(effect);
}

bool LedOutput::stop() { return initialized_ && accept(Effect::Off); }

void LedOutput::updateWake(bool valid, std::uint8_t mask) {
    wakeValid_ = valid && (mask & ~15U) == 0;
    wakeMask_ = wakeValid_ ? mask : 0;
    const bool together = wakeValid_ && wakeMask_ == 15;
    if (together != resonanceTogether_) {
        resonanceTogetherAt_ = clock_ ? clock_() : 0;
        resonanceMerged_ = false;
    }
    resonanceTogether_ = together;
}

bool LedOutput::accept(Effect effect) {
    if (status_.revision == std::numeric_limits<std::uint32_t>::max()) {
        fail();
        return false;
    }
    ++status_.revision;
    status_.requested = effect;
    status_.pending = true;
    status_.completed = false;
    completionPending_ = false;
    dirty_ = true;
    active_ = false; // The next submitted frame starts a new time base, even for the same effect.
    return true;
}

void LedOutput::requestAutomaticOff() {
    status_.requested = Effect::Off;
    status_.pending = true;
    status_.completed = false;
    completionPending_ = true;
    dirty_ = true;
    active_ = false;
}

void LedOutput::fail() {
    status_.fault = true;
    status_.ready = false;
    status_.requested = Effect::Off;
    status_.pending = true;
    status_.completed = false;
    completionPending_ = false;
    active_ = false;
    inFlight_ = false; // An aborted/failed transfer must never satisfy a later request.
    dirty_ = true;
    recoveryNeeded_ = true;
    recoveryAttempted_ = false;
}

void LedOutput::render(std::uint32_t elapsed) {
    frame_ = Frame{};
    switch (status_.requested) {
    case Effect::Twinkle:
        for (std::size_t i = 0; i < PixelCount; ++i) {
            // Independent staggered triangular pulses. A small white floor keeps
            // every pixel active; the cubic envelope makes brief bright sparkles.
            const std::uint32_t period = 490U + static_cast<std::uint32_t>((i * 173U) % 701U);
            const std::uint32_t phase = (elapsed % period + (i * 257U) % period) % period;
            const std::uint32_t triangle = (phase <= period / 2U ? phase : period - phase) * 510U / period;
            const std::uint32_t sparkle = triangle * triangle * triangle / (255U * 255U);
            const auto value = static_cast<std::uint8_t>(4U + sparkle * (Brightness - 4U) / 255U);
            frame_[i].red = frame_[i].green = frame_[i].blue = static_cast<std::uint8_t>(value * twinkleIntensity_ / 100U);
        }
        break;
    case Effect::Burst: {
        // One symmetric white pulse; its first frame is already visible.
        const std::uint32_t half = BurstMilliseconds / 2U;
        const std::uint32_t value = elapsed < half
            ? 1U + (Brightness - 1U) * elapsed / half
            : (Brightness * (BurstMilliseconds - elapsed) + half - 1U) / half;
        for (auto &pixel : frame_) pixel.red = pixel.green = pixel.blue = static_cast<std::uint8_t>(value);
        break;
    }
    case Effect::Ripple: {
        // A single triangular band moves symmetrically from the center beyond
        // both ends. Q8 distance avoids floating point and carries no effect state.
        constexpr std::int32_t center = static_cast<std::int32_t>(PixelCount / 2U);
        const auto radius = static_cast<std::int32_t>(elapsed * (center + 2U) * 256U / RippleMilliseconds);
        for (std::size_t i = 0; i < PixelCount; ++i) {
            std::int32_t distance = static_cast<std::int32_t>(i) - center;
            if (distance < 0) distance = -distance;
            std::int32_t delta = distance * 256 - radius;
            if (delta < 0) delta = -delta;
            if (delta >= 256) continue;
            const auto value = static_cast<std::uint8_t>((256 - delta) * Brightness / 256);
            frame_[i].red = frame_[i].green = value / 4U;
            frame_[i].blue = value;
        }
        break;
    }
    case Effect::Resonance: {
        if (!wakeValid_) break;
        // Existing Wake origins on the physical strip, independent of app tile order.
        constexpr std::uint8_t origins[] = {22, 2, 16, 10};
        const auto togetherElapsed = static_cast<std::uint32_t>(clock_() - resonanceTogetherAt_);
        // Latch the settled phase so a later uint32 clock wrap cannot replay the merge.
        if (resonanceTogether_ && togetherElapsed >= ResonanceMergeMilliseconds) resonanceMerged_ = true;
        const auto blend = resonanceTogether_ ?
            (resonanceMerged_ ? 256U : togetherElapsed * 256U / ResonanceMergeMilliseconds) : 0U;
        for (std::size_t i = 0; i < PixelCount; ++i) {
            unsigned zone = 0, nearest = PixelCount;
            for (unsigned candidate = 0; candidate < 4; ++candidate) {
                const auto distance = i > origins[candidate] ? i - origins[candidate] : origins[candidate] - i;
                if (distance < nearest) { zone = candidate; nearest = static_cast<unsigned>(distance); }
            }
            if (!(wakeMask_ & (1U << zone))) continue;
            const auto phase = (elapsed % ResonancePeriodMilliseconds + zone * 600U) % ResonancePeriodMilliseconds;
            const auto triangle = phase < 1200U ? phase : ResonancePeriodMilliseconds - phase;
            const auto breath = 4U + triangle * 20U / 1200U;
            const auto value = static_cast<std::uint8_t>((breath * (256U - blend) + 24U * blend) / 256U);
            frame_[i].red = value / 6U;
            frame_[i].green = value * 2U / 3U;
            frame_[i].blue = value;
        }
        break;
    }
    case Effect::Off: break;
    }
}

void LedOutput::service() {
    if (!initialized_) return;
    const std::uint32_t now = clock_();
    if (recoveryNeeded_) {
        if (recoveryAttempted_ && static_cast<std::uint32_t>(now - recoveryAt_) < 250U) return;
        recoveryAttempted_ = true;
        recoveryAt_ = now;
        if (!bus_.reset()) return;
        recoveryNeeded_ = false;
    }
    if (active_) {
        const std::uint32_t elapsed = now - startedAt_;
        if ((status_.requested == Effect::Burst && elapsed >= BurstMilliseconds) ||
            (status_.requested == Effect::Ripple && elapsed >= RippleMilliseconds)) requestAutomaticOff();
    }
    if (inFlight_) {
        const TxResult result = bus_.poll();
        if (result == TxResult::Failed ||
            (result == TxResult::Busy && static_cast<std::uint32_t>(now - txStartedAt_) >= TransferTimeoutMilliseconds)) {
            fail();
            return;
        }
        if (result == TxResult::Busy) return;
        inFlight_ = false;
        status_.applied = txEffect_;
        status_.appliedRevision = txRevision_;
        status_.off = txBlack_;
        if (!dirty_ && txRevision_ == status_.revision && txEffect_ == status_.requested) {
            status_.pending = false;
            status_.ready = !status_.fault;
            status_.completed = txCompletion_;
        }
    }
    if (!dirty_ && (!active_ || static_cast<std::uint32_t>(now - lastSubmit_) < FrameMilliseconds)) return;
    if (dirty_) {
        startedAt_ = now;
        if (status_.requested == Effect::Resonance) {
            resonanceTogetherAt_ = now;
            resonanceMerged_ = false;
        }
    }
    render(now - startedAt_);
    if (!bus_.submit(frame_)) { fail(); return; }
    txBlack_ = true;
    for (const auto &pixel : frame_) if (pixel.red || pixel.green || pixel.blue) txBlack_ = false;
    txEffect_ = status_.requested;
    txRevision_ = status_.revision;
    txCompletion_ = completionPending_;
    txStartedAt_ = lastSubmit_ = now;
    inFlight_ = true;
    dirty_ = false;
    active_ = status_.requested != Effect::Off;
}

}} // namespace larp::led
