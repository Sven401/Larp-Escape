#include "Companion/ProfileResponseSender.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
class Call {
  public:
    explicit Call(bool &v) : v_(v) {
        v_ = true;
    }
    ~Call() {
        v_ = false;
    }

  private:
    bool &v_;
};
} // namespace
bool ProfileResponseSender::current() noexcept {
    TrustedAuthority value;
    return original_.gameMaster && original_.principal && original_.epoch &&
           authority_.readAuthority(value) && value == original_;
}
ProfileResponseSend ProfileResponseSender::load(const RoutedProfileResponse &input,
                                                std::uint32_t now) noexcept {
    if (busy_ || pending_)
        return ProfileResponseSend::Busy;
    Call call(busy_);
    const auto selected = input;
    char bytes[20];
    std::size_t written = 0;
    if (!encodeProfileResponse(selected.response, bytes, sizeof(bytes), written))
        return ProfileResponseSend::Invalid;
    original_ = selected.original;
    if (!current())
        return ProfileResponseSend::Unauthorized;
    if (now - selected.producedAt >= ProfileControlLimits::ResponseMilliseconds)
        return ProfileResponseSend::Expired;
    std::memcpy(bytes_.data(), bytes, written);
    loaded_ = selected.producedAt;
    pending_ = true;
    return ProfileResponseSend::Loaded;
}
ProfileResponseSend ProfileResponseSender::service(std::uint32_t now,
                                                   bool gameReplyPriority) noexcept {
    if (busy_)
        return ProfileResponseSend::Busy;
    if (!pending_)
        return ProfileResponseSend::Idle;
    Call call(busy_);
    if (!current()) {
        pending_ = false;
        return ProfileResponseSend::Unauthorized;
    }
    if (now - loaded_ >= ProfileControlLimits::ResponseMilliseconds) {
        pending_ = false;
        return ProfileResponseSend::Expired;
    }
    if (gameReplyPriority)
        return ProfileResponseSend::Deferred;
    RoutedReplyFrame value;
    value.principal = original_.principal;
    value.epoch = original_.epoch;
    value.count = 20;
    std::memcpy(value.bytes.data(), bytes_.data(), 20);
    const auto copied = bearer_.copyReplyFrame(value);
    if (copied == ReplyFrameCopy::Busy)
        return ProfileResponseSend::Busy;
    pending_ = false;
    return copied == ReplyFrameCopy::Copied ? ProfileResponseSend::Copied
                                            : ProfileResponseSend::Unavailable;
}
} // namespace companion
} // namespace larp

namespace larp {
namespace companion {
ProfileConfiguredStep serviceProfileConfiguredFrames(ReplyFrameSender &reply,
                                                     ProfileResponseSender &profile,
                                                     StatusFrameSender &status, std::uint32_t now,
                                                     std::size_t maximumFrameBytes) noexcept {
    ProfileConfiguredStep result;
    const bool gamePriority = reply.pending(), profilePriority = profile.pending();
    result.game = reply.service(now, maximumFrameBytes);
    result.profile = profile.service(now, gamePriority);
    result.status = status.service(now, maximumFrameBytes, gamePriority || profilePriority);
    return result;
}
} // namespace companion
} // namespace larp
