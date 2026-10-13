#pragma once
#include "Companion/ProfileControlFrames.h"
#include "Companion/ProfileStaging.h"
#include <array>
namespace larp {
namespace companion {
struct ProfileFrameQueueLimits {
    static const std::size_t Depth = 4;
};
struct CopiedProfileFrame {
    std::uint32_t principal = 0, epoch = 0, receivedAt = 0;
    std::uint16_t count = 0;
    std::array<char, ProfileFrameLimits::ValueBytes> bytes = {};
};
enum class ProfileCopyOutcome : std::uint8_t {
    Copied,
    QueueFull,
    Unauthorized,
    InvalidWrite,
    UnsupportedFrame,
    Uninitialized
};
class IProfileFrameQueue {
  public:
    virtual ~IProfileFrameQueue() = default;
    virtual bool take(CopiedProfileFrame &frame) noexcept = 0;
};
// Scalars only, never a borrowed payload into the local copied queue item.
struct ProfileDispatchResult {
    bool received = false, metadata = false, control = false;
    ProfileReceiveOutcome outcome = ProfileReceiveOutcome::Discarded;
    std::uint32_t principal = 0, epoch = 0, receivedAt = 0, offset = 0;
    std::uint16_t transfer = 0, payloadBytes = 0;
};
// Sole synchronous owner. Poll source even without input; at most one zero-wait
// queue take. Finish/import/apply are distinct trusted owner operations, never
// inferred from empty FIFO. Backend work happens here, never in the callback.
class ProfileFramePump {
  public:
    ProfileFramePump(IProfileFrameQueue &queue, ProfileStaging &source, IProfileImportClock &clock,
                     IProfileControlSink *controls = nullptr)
        : queue_(queue), source_(source), clock_(clock), controls_(controls) {}
    ProfileFramePump(const ProfileFramePump &) = delete;
    ProfileFramePump &operator=(const ProfileFramePump &) = delete;
    ProfileDispatchResult service() noexcept;

  private:
    IProfileFrameQueue &queue_;
    ProfileStaging &source_;
    IProfileImportClock &clock_;
    IProfileControlSink *controls_;
    bool busy_ = false;
};
} // namespace companion
} // namespace larp
