#pragma once
#include "Companion/ReplyFrame.h"
#include <array>
namespace larp {
namespace companion {
struct RoutedReplyFrame {
    std::uint32_t principal = 0, epoch = 0;
    std::uint16_t count = 0;
    std::array<char, CommandFrameLimits::Frame> bytes = {};
};
enum class ReplyFrameCopy : std::uint8_t { Copied, Busy, Unavailable };
class IReplyFrameBearer {
  public:
    virtual ~IReplyFrameBearer() = default;
    // Zero wait, bounded copy before returning. Verify the exact bound connection
    // again at the bearer observation point; never relabel a recycled handle.
    // Copied is NOT peer delivery, indication confirmation or application ack.
    virtual ReplyFrameCopy copyReplyFrame(const RoutedReplyFrame &frame) noexcept = 0;
};
enum class ReplySendOutcome : std::uint8_t {
    Idle,
    Loaded,
    Busy,
    SentFrame,
    Completed,
    Unauthorized,
    Expired,
    InvalidReply,
    InvalidFrameSize,
    TransportUnavailable,
    MessageExhausted
};
// One transport task owns load/service/pending. One immutable encoded reply;
// no heap/profile. At most one bearer copy per service. Application keeps running.
class ReplyFrameSender {
  public:
    ReplyFrameSender(const IGameControlChannel &authority, IReplyFrameBearer &bearer)
        : authority_(authority), bearer_(bearer) {}
    ReplyFrameSender(const ReplyFrameSender &) = delete;
    ReplyFrameSender &operator=(const ReplyFrameSender &) = delete;
    ReplySendOutcome load(const RoutedGameReply &reply, std::uint32_t now) noexcept;
    ReplySendOutcome service(std::uint32_t now, std::size_t maximumFrameBytes) noexcept;
    bool pending() const noexcept {
        return message_ != 0;
    }

  private:
    const IGameControlChannel &authority_;
    IReplyFrameBearer &bearer_;
    std::array<char, maxGameReplyJsonBytes> document_ = {};
    std::uint32_t principal_ = 0, epoch_ = 0, started_ = 0;
    std::uint32_t counterEpoch_ = 0, nextMessage_ = 1;
    std::uint16_t message_ = 0, total_ = 0, offset_ = 0;
    void discard() noexcept;
    ReplySendOutcome refresh(std::uint32_t now) noexcept;
};
} // namespace companion
} // namespace larp
