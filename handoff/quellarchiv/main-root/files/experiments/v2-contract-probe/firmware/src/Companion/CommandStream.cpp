#include "Companion/CommandStream.h"
#include <cstring>
namespace larp {
namespace companion {
void CommandStream::discard() noexcept {
    principal_ = epoch_ = started_ = 0;
    message_ = total_ = received_ = 0;
}
CommandStreamSnapshot CommandStream::snapshot() const noexcept {
    CommandStreamSnapshot value;
    value.message = message_;
    value.total = total_;
    value.received = received_;
    value.complete = message_ && received_ == total_;
    return value;
}
CommandStreamResult CommandStream::result(CommandStreamOutcome outcome) const noexcept {
    CommandStreamResult value;
    value.outcome = outcome;
    value.message = message_;
    value.total = total_;
    value.received = received_;
    return value;
}
bool CommandStream::refresh(std::uint32_t now, TrustedAuthority &authority,
                            CommandStreamOutcome &event) noexcept {
    if (!sink_.readAuthority(authority) || !authority.gameMaster || !authority.principal ||
        !authority.epoch) {
        discard();
        event = CommandStreamOutcome::Unauthorized;
        return false;
    }
    if (message_ && (principal_ != authority.principal || epoch_ != authority.epoch)) {
        discard();
        event = CommandStreamOutcome::Unauthorized;
        return true; // A fresh authorized connection may begin at offset zero.
    }
    if (message_ &&
        static_cast<std::uint32_t>(now - started_) >= CommandFrameLimits::TimeoutMilliseconds) {
        discard();
        event = CommandStreamOutcome::Expired;
        return true;
    }
    event = !message_             ? CommandStreamOutcome::Idle
            : received_ == total_ ? CommandStreamOutcome::QueueFull
                                  : CommandStreamOutcome::Receiving;
    return true;
}
CommandStreamResult CommandStream::poll(std::uint32_t now) noexcept {
    TrustedAuthority authority;
    CommandStreamOutcome event;
    refresh(now, authority, event);
    return result(event);
}
CommandStreamResult CommandStream::retry(std::uint32_t now) noexcept {
    TrustedAuthority authority;
    CommandStreamOutcome event;
    refresh(now, authority, event);
    return message_ && received_ == total_ ? submit() : result(event);
}
CommandStreamResult CommandStream::submit() noexcept {
    auto value = result(CommandStreamOutcome::Idle);
    value.submission = sink_.submitJson(buffer_.data(), received_, principal_, epoch_);
    switch (value.submission.outcome) {
    case GameSubmissionOutcome::Queued:
        value.outcome = CommandStreamOutcome::Queued;
        break;
    case GameSubmissionOutcome::QueueFull:
        value.outcome = CommandStreamOutcome::QueueFull;
        break;
    case GameSubmissionOutcome::DecodeRejected:
        value.outcome = CommandStreamOutcome::DecodeRejected;
        break;
    case GameSubmissionOutcome::Uninitialized:
        value.outcome = CommandStreamOutcome::Uninitialized;
        break;
    default:
        value.outcome = CommandStreamOutcome::Unauthorized;
        break;
    }
    if (value.outcome != CommandStreamOutcome::QueueFull)
        discard();
    return value;
}
CommandStreamResult CommandStream::receive(const char *bytes, std::size_t count,
                                           std::uint32_t principal, std::uint32_t epoch,
                                           std::uint32_t now) noexcept {
    return receive(bytes, count, principal, epoch, now, now);
}
CommandStreamResult CommandStream::receive(const char *bytes, std::size_t count,
                                           std::uint32_t principal, std::uint32_t epoch,
                                           std::uint32_t receivedAt, std::uint32_t now) noexcept {
    TrustedAuthority authority;
    CommandStreamOutcome event;
    const bool authorized = refresh(now, authority, event);
    if (!authorized || !principal || !epoch || principal != authority.principal ||
        epoch != authority.epoch)
        return result(CommandStreamOutcome::Unauthorized);
    if (event == CommandStreamOutcome::Expired ||
        static_cast<std::uint32_t>(now - receivedAt) >= CommandFrameLimits::TimeoutMilliseconds)
        return result(CommandStreamOutcome::Expired);
    CommandFrame frame;
    const auto decoded = decodeCommandFrame(bytes, count, frame);
    if (decoded != FrameDecodeResult::Success)
        return result(decoded == FrameDecodeResult::Unsupported
                          ? CommandStreamOutcome::UnsupportedFrame
                          : CommandStreamOutcome::InvalidFrame);
    if (!message_) {
        if (frame.offset)
            return result(CommandStreamOutcome::OutOfOrder);
        message_ = frame.message;
        total_ = frame.total;
        principal_ = principal;
        epoch_ = epoch;
        started_ = receivedAt;
    } else if (message_ != frame.message) {
        return result(CommandStreamOutcome::Busy);
    } else if (total_ != frame.total) {
        const auto value = result(CommandStreamOutcome::FragmentConflict);
        discard();
        return value;
    }
    if (frame.offset < received_) {
        if (frame.count > static_cast<std::size_t>(received_ - frame.offset))
            return result(CommandStreamOutcome::OutOfOrder);
        if (std::memcmp(buffer_.data() + frame.offset, frame.payload, frame.count) != 0) {
            const auto value = result(CommandStreamOutcome::FragmentConflict);
            discard();
            return value;
        }
        return received_ == total_ ? submit() : result(CommandStreamOutcome::DuplicateFragment);
    }
    if (frame.offset != received_)
        return result(CommandStreamOutcome::OutOfOrder);
    std::memcpy(buffer_.data() + received_, frame.payload, frame.count);
    received_ = static_cast<std::uint16_t>(received_ + frame.count);
    return received_ == total_ ? submit() : result(CommandStreamOutcome::Receiving);
}
} // namespace companion
} // namespace larp
