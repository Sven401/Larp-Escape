#include "DevelopmentLedOutput.h"

bool DevelopmentLedOutput::begin() noexcept {
    if (showMutex_) return false;
    showMutex_ = xSemaphoreCreateMutex();
    if (!showMutex_) return false;
    fill_solid(pixels_, LedCount, CRGB::Black);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 100);
    FastLED.addLeds<WS2811, 15, GRB>(pixels_, LedCount)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(32);
    return true;
}

void DevelopmentLedOutput::prepare(std::uint32_t ticket) noexcept {
    renderCalls_.store(0, std::memory_order_release);
    blackSubmitted_.store(false, std::memory_order_release);
    colorAtMs_.store(0, std::memory_order_release);
    blackAtMs_.store(0, std::memory_order_release);
    ticket_.store(ticket, std::memory_order_release);
}

bool DevelopmentLedOutput::showSingle(
    std::uint32_t ticket,
    const larp::diagnostic::DevelopmentLedStepRequest &request,
    DevelopmentLedDeadline &deadline) noexcept {
    if (!showMutex_ || ticket_.load(std::memory_order_acquire) != ticket ||
        request.index >= LedCount || request.red > 32 || request.green > 32 ||
        request.blue > 32 || !(request.red || request.green || request.blue) ||
        request.brightness < 1 || request.brightness > 32 ||
        request.durationMs < 100 || request.durationMs > 250 ||
        xSemaphoreTake(showMutex_, pdMS_TO_TICKS(50)) != pdTRUE)
        return false;
    bool submitted = false;
    if (ticket_.load(std::memory_order_acquire) == ticket &&
        !blackSubmitted_.load(std::memory_order_acquire) &&
        deadline.colorAllowed(ticket)) {
        fill_solid(pixels_, LedCount, CRGB::Black);
        pixels_[request.index] = CRGB(request.red, request.green, request.blue);
        FastLED.setBrightness(request.brightness);
        FastLED.show();
        colorAtMs_.store(millis(), std::memory_order_release);
        renderCalls_.fetch_add(1, std::memory_order_acq_rel);
        submitted = true;
    }
    xSemaphoreGive(showMutex_);
    return submitted;
}

bool DevelopmentLedOutput::submitBlack(std::uint32_t ticket) noexcept {
    if (!showMutex_ || ticket_.load(std::memory_order_acquire) != ticket ||
        xSemaphoreTake(showMutex_, pdMS_TO_TICKS(50)) != pdTRUE)
        return false;
    fill_solid(pixels_, LedCount, CRGB::Black);
    FastLED.show();
    blackAtMs_.store(millis(), std::memory_order_release);
    renderCalls_.fetch_add(1, std::memory_order_acq_rel);
    blackSubmitted_.store(true, std::memory_order_release);
    xSemaphoreGive(showMutex_);
    return true;
}
