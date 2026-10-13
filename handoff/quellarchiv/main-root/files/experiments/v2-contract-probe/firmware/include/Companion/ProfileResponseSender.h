#pragma once
#include "Companion/ProfileControlFrames.h"
#include "Companion/ReplyFrameSender.h"
#include "Companion/StatusFrameSender.h"
namespace larp {
namespace companion {
enum class ProfileResponseSend : std::uint8_t {
    Idle,
    Loaded,
    Busy,
    Deferred,
    Copied,
    Unauthorized,
    Expired,
    Invalid,
    Unavailable
};
// One immutable20-byte response, sole transport owner; no heap or game tick.
class ProfileResponseSender {
  public:
    ProfileResponseSender(const IGameControlChannel &authority, IReplyFrameBearer &bearer)
        : authority_(authority), bearer_(bearer) {}
    ProfileResponseSender(const ProfileResponseSender &) = delete;
    ProfileResponseSender &operator=(const ProfileResponseSender &) = delete;
    ProfileResponseSend load(const RoutedProfileResponse &, std::uint32_t now) noexcept;
    ProfileResponseSend service(std::uint32_t now, bool gameReplyPriority = false) noexcept;
    bool pending() const noexcept {
        return pending_;
    }

  private:
    const IGameControlChannel &authority_;
    IReplyFrameBearer &bearer_;
    TrustedAuthority original_;
    std::array<char, ProfileControlLimits::ResponseBytes> bytes_ = {};
    std::uint32_t loaded_ = 0;
    bool pending_ = false, busy_ = false;
    bool current() noexcept;
};
struct ProfileConfiguredStep {
    ReplySendOutcome game = ReplySendOutcome::Idle;
    ProfileResponseSend profile = ProfileResponseSend::Idle;
    StatusSendOutcome status = StatusSendOutcome::Idle;
};
// Priority captured BEFORE service: a completing game/profile reply still owns
// this whole step; at most one bearer call, no added application service/tick.
ProfileConfiguredStep serviceProfileConfiguredFrames(ReplyFrameSender &, ProfileResponseSender &,
                                                     StatusFrameSender &, std::uint32_t now,
                                                     std::size_t maximumFrameBytes) noexcept;
} // namespace companion
} // namespace larp
