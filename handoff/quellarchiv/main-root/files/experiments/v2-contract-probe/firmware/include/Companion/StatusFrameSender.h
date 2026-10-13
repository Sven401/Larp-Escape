#pragma once
#include "Companion/ReplyFrameSender.h"
#include "Companion/StatusFrame.h"
namespace larp {
namespace companion {
enum class StatusSendOutcome : std::uint8_t {
    Idle,
    Loaded,
    Busy,
    AlreadySampled,
    StaleStatus,
    InvalidStatus,
    InvalidFrameSize,
    SentFrame,
    Completed,
    Deferred,
    Unauthorized,
    Expired,
    TransportUnavailable,
    MessageExhausted
};
// Sole transport owner. One fixed, immutable encoded snapshot, never a profile.
// Uses the same bounded copied bearer contract; type4 identifies status values.
class StatusFrameSender {
  public:
    StatusFrameSender(const IGameControlChannel &authority, IReplyFrameBearer &bearer)
        : authority_(authority), bearer_(bearer) {}
    StatusFrameSender(const StatusFrameSender &) = delete;
    StatusFrameSender &operator=(const StatusFrameSender &) = delete;
    StatusSendOutcome load(const RoutedConfiguredStatus &status, std::uint32_t now) noexcept;
    // replyPriority is trusted owner scheduling, never a client-provided flag.
    StatusSendOutcome service(std::uint32_t now, std::size_t maximumFrameBytes,
                              bool replyPriority = false) noexcept;
    void cancel() noexcept;
    bool pending() const noexcept {
        return message_ != 0;
    }

  private:
    const IGameControlChannel &authority_;
    IReplyFrameBearer &bearer_;
    std::array<char, maxConfiguredStatusJsonBytes> document_ = {};
    std::uint32_t principal_ = 0, epoch_ = 0, started_ = 0, sampled_ = 0;
    std::uint32_t counterEpoch_ = 0, nextMessage_ = 1, lastSample_ = 0;
    BootIdentity lastBoot_;
    std::uint16_t message_ = 0, total_ = 0, offset_ = 0;
    StatusSendOutcome refresh(std::uint32_t now) noexcept;
};
struct ConfiguredFrameStep {
    ReplySendOutcome reply = ReplySendOutcome::Idle;
    StatusSendOutcome status = StatusSendOutcome::Idle;
};
// Caller takes current queued replies before this step. Even a completing reply
// owns this entire step: at most one reply/status bearer copy, zero extra ticks.
ConfiguredFrameStep serviceConfiguredFrames(ReplyFrameSender &reply, StatusFrameSender &status,
                                            std::uint32_t now,
                                            std::size_t maximumFrameBytes) noexcept;
} // namespace companion
} // namespace larp
