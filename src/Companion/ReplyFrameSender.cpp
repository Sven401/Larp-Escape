#include "Companion/ReplyFrameSender.h"
#include <algorithm>
namespace larp {
namespace companion {
void ReplyFrameSender::discard() noexcept {
    principal_ = epoch_ = started_ = 0;
    message_ = total_ = offset_ = 0;
}
ReplySendOutcome ReplyFrameSender::refresh(std::uint32_t now) noexcept {
    if (!pending())
        return ReplySendOutcome::Idle;
    TrustedAuthority current;
    if (!authority_.readAuthority(current) || !current.gameMaster ||
        current.principal != principal_ || current.epoch != epoch_) {
        discard();
        return ReplySendOutcome::Unauthorized;
    }
    if (static_cast<std::uint32_t>(now - started_) >= CommandFrameLimits::TimeoutMilliseconds) {
        discard();
        return ReplySendOutcome::Expired;
    }
    return ReplySendOutcome::Loaded;
}
ReplySendOutcome ReplyFrameSender::load(const RoutedGameReply &reply, std::uint32_t now) noexcept {
    if (refresh(now) == ReplySendOutcome::Loaded)
        return ReplySendOutcome::Busy;
    TrustedAuthority current;
    if (!reply.principal || !reply.connectionEpoch || !authority_.readAuthority(current) ||
        !current.gameMaster || current.principal != reply.principal ||
        current.epoch != reply.connectionEpoch)
        return ReplySendOutcome::Unauthorized;
    if (counterEpoch_ != current.epoch) {
        counterEpoch_ = current.epoch;
        nextMessage_ = 1;
    }
    if (nextMessage_ > UINT16_MAX)
        return ReplySendOutcome::MessageExhausted;
    std::size_t written = 0;
    if (encodeGameReply(reply.reply, document_.data(), document_.size(), written) !=
            ConfiguredEncodeResult::Success ||
        !written || written > ReplyFrameLimits::Document)
        return ReplySendOutcome::InvalidReply;
    message_ = static_cast<std::uint16_t>(nextMessage_++);
    principal_ = reply.principal;
    epoch_ = reply.connectionEpoch;
    started_ = now;
    total_ = static_cast<std::uint16_t>(written);
    offset_ = 0;
    return ReplySendOutcome::Loaded;
}
ReplySendOutcome ReplyFrameSender::service(std::uint32_t now,
                                           std::size_t maximumFrameBytes) noexcept {
    const auto state = refresh(now);
    if (state != ReplySendOutcome::Loaded)
        return state;
    if (maximumFrameBytes <= CommandFrameLimits::Header ||
        maximumFrameBytes > CommandFrameLimits::Frame)
        return ReplySendOutcome::InvalidFrameSize;
    const auto count =
        std::min<std::size_t>(maximumFrameBytes - CommandFrameLimits::Header, total_ - offset_);
    RoutedReplyFrame frame;
    frame.principal = principal_;
    frame.epoch = epoch_;
    std::size_t written = 0;
    if (!encodeReplyFrame(message_, total_, offset_, document_.data() + offset_, count,
                          frame.bytes.data(), frame.bytes.size(), written)) {
        discard();
        return ReplySendOutcome::InvalidReply;
    }
    frame.count = static_cast<std::uint16_t>(written);
    const auto copied = bearer_.copyReplyFrame(frame);
    if (copied == ReplyFrameCopy::Busy)
        return ReplySendOutcome::Busy;
    if (copied != ReplyFrameCopy::Copied) {
        discard();
        return ReplySendOutcome::TransportUnavailable;
    }
    offset_ = static_cast<std::uint16_t>(offset_ + count);
    if (offset_ == total_) {
        discard();
        return ReplySendOutcome::Completed;
    }
    return ReplySendOutcome::SentFrame;
}
} // namespace companion
} // namespace larp
