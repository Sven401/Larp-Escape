#pragma once

#include "DevelopmentLedDeadline.h"
#include "CompanionIntegration/DiagnosticDevelopmentLedStep.h"
#include <FastLED.h>
#include <atomic>
#include <freertos/semphr.h>

// The only FastLED owner in the isolated test image. Both color and the
// independent off task use the same mutex; return values mean software
// submissions only, never measured light or current.
class DevelopmentLedOutput final : public DevelopmentLedBlackPort {
  public:
    bool begin() noexcept;
    void prepare(std::uint32_t ticket) noexcept;
    bool showSingle(std::uint32_t ticket,
                    const larp::diagnostic::DevelopmentLedStepRequest &request,
                    DevelopmentLedDeadline &deadline) noexcept;
    bool submitBlack(std::uint32_t ticket) noexcept override;
    std::uint8_t renderCalls() const noexcept {
        return renderCalls_.load(std::memory_order_acquire);
    }
    std::uint32_t colorSubmittedAtMs() const noexcept {
        return colorAtMs_.load(std::memory_order_acquire);
    }
    std::uint32_t blackSubmittedAtMs() const noexcept {
        return blackAtMs_.load(std::memory_order_acquire);
    }

  private:
    static constexpr std::uint8_t LedCount = 23;
    CRGBArray<LedCount> pixels_;
    SemaphoreHandle_t showMutex_ = nullptr;
    std::atomic<std::uint32_t> ticket_{0};
    std::atomic<std::uint8_t> renderCalls_{0};
    std::atomic<bool> blackSubmitted_{false};
    std::atomic<std::uint32_t> colorAtMs_{0};
    std::atomic<std::uint32_t> blackAtMs_{0};
};
