#pragma once
#include "Companion/ProfileControlFrames.h"
#include "Companion/ProfileStaging.h"
#include "CompanionIntegration/ProfileApply.h"
namespace larp {
namespace integration {
// Sole synchronous Application owner. One private receipt, one response slot,
// one request cache. NEVER accepts client receipts or activates from local draft.
// Application/owners are serviced separately; this owner adds no game ticks.
class ProfileTransferControl : public companion::IProfileControlSink {
  public:
    ProfileTransferControl(devices::ProfileApplication &app,
                           const companion::IGameControlChannel &authority,
                           companion::ProfileStaging &source, companion::IProfileImportClock &clock)
        : app_(app), authority_(authority), source_(source), clock_(clock) {}
    ProfileTransferControl(const ProfileTransferControl &) = delete;
    ProfileTransferControl &operator=(const ProfileTransferControl &) = delete;
    void service() noexcept override;
    bool ready() const noexcept override {
        return !busy_ && !pending_ && !applying_;
    }
    void sourceStarted() noexcept override;
    void handle(const companion::ProfileControlFrame &, const companion::TrustedAuthority &,
                std::uint32_t) noexcept override;
    bool takeResponse(companion::RoutedProfileResponse &) noexcept;

  private:
    devices::ProfileApplication &app_;
    const companion::IGameControlChannel &authority_;
    companion::ProfileStaging &source_;
    companion::IProfileImportClock &clock_;
    ProfileImportReceipt receipt_;
    companion::TrustedAuthority session_;
    companion::ProfileResponseFrame cached_, response_;
    std::uint32_t lastRequest_ = 0, produced_ = 0, targetProfile_ = 0, targetGeneration_ = 0;
    bool busy_ = false, pending_ = false, applying_ = false;
    bool current(const companion::TrustedAuthority &) noexcept;
    void close() noexcept;
    void emit(const companion::ProfileResponseFrame &) noexcept;
    companion::ProfileResponseFrame
    reply(const companion::ProfileControlFrame &, companion::ProfileResponseCode,
          companion::ProfileResponseDetail = companion::ProfileResponseDetail::None) const noexcept;
};
} // namespace integration
} // namespace larp
