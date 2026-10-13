#pragma once
#include "Companion/ProfileControlFrames.h"
#include "Companion/ProfileResponseSender.h"
#include "Companion/ProfileSourceEnvelope.h"
namespace larp {
namespace companion {
struct ProfileResponseQueueLimits {
    static const std::size_t Depth = 4;
};
enum class ProfileResponseCopy : std::uint8_t {
    Copied,
    QueueFull,
    Unauthorized,
    Expired,
    Invalid,
    Uninitialized
};
// Synchronized copy-only port. Owner keeps output unless Copied. No callbacks
// into Application from transport; trusted producer route/time are immutable.
class IProfileResponseQueue {
  public:
    virtual ~IProfileResponseQueue() = default;
    virtual ProfileResponseCopy copyResponse(const RoutedProfileResponse &) noexcept = 0;
    // Sole transport consumer: bounded stale draining, false preserves destination.
    virtual bool takeCurrent(RoutedProfileResponse &) noexcept = 0;
};
// Sole transport owner, bounded takeCurrent; pending sender/reentrant selection
// consumes no queue item. No bearer call/tick here.
class ProfileResponseReader {
  public:
    ProfileResponseReader(IProfileResponseQueue &queue, ProfileResponseSender &sender)
        : queue_(queue), sender_(sender) {}
    ProfileResponseReader(const ProfileResponseReader &) = delete;
    ProfileResponseReader &operator=(const ProfileResponseReader &) = delete;
    ProfileResponseSend select(std::uint32_t now) noexcept;

  private:
    IProfileResponseQueue &queue_;
    ProfileResponseSender &sender_;
    bool busy_ = false;
};
} // namespace companion
} // namespace larp
