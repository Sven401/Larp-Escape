#include "Companion/StatusFrameSender.h"
#include <algorithm>
namespace larp {
namespace companion {
static_assert(StatusFrameLimits::Document <= UINT16_MAX, "Status length fits the wire header");
void StatusFrameSender::cancel() noexcept {
    principal_ = epoch_ = started_ = sampled_ = 0;
    message_ = total_ = offset_ = 0;
}
StatusSendOutcome StatusFrameSender::refresh(std::uint32_t now) noexcept {
    if (!pending())
        return StatusSendOutcome::Idle;
    TrustedAuthority current;
    if (!authority_.readAuthority(current) || !current.gameMaster ||
        current.principal != principal_ || current.epoch != epoch_) {
        cancel();
        return StatusSendOutcome::Unauthorized;
    }
    if (static_cast<std::uint32_t>(now - started_) >= StatusFrameLimits::TimeoutMilliseconds) {
        cancel();
        return StatusSendOutcome::Expired;
    }
    if (!offset_ &&
        static_cast<std::uint32_t>(now - sampled_) >= StatusFrameLimits::FreshMilliseconds) {
        cancel();
        return StatusSendOutcome::StaleStatus;
    }
    return StatusSendOutcome::Loaded;
}
StatusSendOutcome StatusFrameSender::load(const RoutedConfiguredStatus &status,
                                          std::uint32_t now) noexcept {
    if (refresh(now) == StatusSendOutcome::Loaded)
        return StatusSendOutcome::Busy;
    TrustedAuthority current;
    if (!status.principal || !status.connectionEpoch || !authority_.readAuthority(current) ||
        !current.gameMaster || current.principal != status.principal ||
        current.epoch != status.connectionEpoch)
        return StatusSendOutcome::Unauthorized;
    if (counterEpoch_ != current.epoch) {
        counterEpoch_ = current.epoch;
        nextMessage_ = 1;
        lastSample_ = 0;
        lastBoot_ = {};
    }
    if (!status.status.boot.valid() || !status.status.sample ||
        (lastSample_ && !(status.status.boot == lastBoot_)))
        return StatusSendOutcome::InvalidStatus;
    if (status.status.sample == lastSample_)
        return StatusSendOutcome::AlreadySampled;
    if (status.status.sample < lastSample_ ||
        static_cast<std::uint32_t>(now - status.status.sampledMilliseconds) >=
            StatusFrameLimits::FreshMilliseconds)
        return StatusSendOutcome::StaleStatus;
    if (nextMessage_ > UINT16_MAX)
        return StatusSendOutcome::MessageExhausted;
    std::size_t written = 0;
    if (encodeConfiguredStatus(status.status, document_.data(), document_.size(), written) !=
            ConfiguredEncodeResult::Success ||
        !written || written > StatusFrameLimits::Document)
        return StatusSendOutcome::InvalidStatus;
    message_ = static_cast<std::uint16_t>(nextMessage_++);
    principal_ = status.principal;
    epoch_ = status.connectionEpoch;
    started_ = now;
    sampled_ = status.status.sampledMilliseconds;
    lastSample_ = status.status.sample;
    lastBoot_ = status.status.boot;
    total_ = static_cast<std::uint16_t>(written);
    offset_ = 0;
    return StatusSendOutcome::Loaded;
}
StatusSendOutcome StatusFrameSender::service(std::uint32_t now, std::size_t maximumFrameBytes,
                                             bool replyPriority) noexcept {
    const auto state = refresh(now);
    if (state != StatusSendOutcome::Loaded)
        return state;
    if (replyPriority)
        return StatusSendOutcome::Deferred;
    if (maximumFrameBytes <= CommandFrameLimits::Header ||
        maximumFrameBytes > CommandFrameLimits::Frame)
        return StatusSendOutcome::InvalidFrameSize;
    const auto count =
        std::min<std::size_t>(maximumFrameBytes - CommandFrameLimits::Header, total_ - offset_);
    RoutedReplyFrame frame;
    frame.principal = principal_;
    frame.epoch = epoch_;
    std::size_t written = 0;
    if (!encodeStatusFrame(message_, total_, offset_, document_.data() + offset_, count,
                           frame.bytes.data(), frame.bytes.size(), written)) {
        cancel();
        return StatusSendOutcome::InvalidStatus;
    }
    frame.count = static_cast<std::uint16_t>(written);
    const auto copied = bearer_.copyReplyFrame(frame);
    if (copied == ReplyFrameCopy::Busy)
        return StatusSendOutcome::Busy;
    if (copied != ReplyFrameCopy::Copied) {
        cancel();
        return StatusSendOutcome::TransportUnavailable;
    }
    offset_ = static_cast<std::uint16_t>(offset_ + count);
    if (offset_ == total_) {
        cancel();
        return StatusSendOutcome::Completed;
    }
    return StatusSendOutcome::SentFrame;
}
ConfiguredFrameStep serviceConfiguredFrames(ReplyFrameSender &reply, StatusFrameSender &status,
                                            std::uint32_t now,
                                            std::size_t maximumFrameBytes) noexcept {
    const bool replyPriority = reply.pending();
    ConfiguredFrameStep step;
    step.reply = reply.service(now, maximumFrameBytes);
    step.status = status.service(now, maximumFrameBytes, replyPriority);
    return step;
}
} // namespace companion
} // namespace larp
