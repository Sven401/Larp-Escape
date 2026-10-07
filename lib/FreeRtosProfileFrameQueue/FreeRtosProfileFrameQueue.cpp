#include "FreeRtosProfileFrameQueue.h"
#include <cstring>
namespace larp {
namespace companion {
bool FreeRtosProfileFrameQueue::begin() noexcept {
    if (!queue_)
        queue_ = xQueueCreateStatic(ProfileFrameQueueLimits::Depth, sizeof(CopiedProfileFrame),
                                    storage_, &control_);
    return queue_ != nullptr;
}
ProfileCopyOutcome FreeRtosProfileFrameQueue::copyWrite(const char *bytes, std::size_t count,
                                                        std::uint32_t principal,
                                                        std::uint32_t epoch,
                                                        std::uint32_t receivedAt, bool prepared,
                                                        std::uint16_t writeOffset) noexcept {
    if (!queue_)
        return ProfileCopyOutcome::Uninitialized;
    if (prepared || writeOffset || !bytes || count <= ProfileFrameLimits::HeaderBytes ||
        count > ProfileFrameLimits::ValueBytes)
        return ProfileCopyOutcome::InvalidWrite;
    CopiedProfileFrame frame;
    frame.principal = principal;
    frame.epoch = epoch;
    frame.receivedAt = receivedAt;
    frame.count = static_cast<std::uint16_t>(count);
    std::memcpy(frame.bytes.data(), bytes, count);
    TrustedAuthority current;
    if (!principal || !epoch || !authority_.readAuthority(current) || !current.gameMaster ||
        current.principal != principal || current.epoch != epoch)
        return ProfileCopyOutcome::Unauthorized;
    ProfileSourceEnvelope envelope;
    ProfileDataFrame data;
    if (!decodeProfileSourceEnvelope(frame.bytes.data(), count, envelope) &&
        !decodeProfileDataFrame(frame.bytes.data(), count, data))
        return static_cast<unsigned char>(frame.bytes[0]) != ProfileFrameLimits::Version ||
                       (static_cast<unsigned char>(frame.bytes[1]) != ProfileSourceLimits::Kind &&
                        static_cast<unsigned char>(frame.bytes[1]) != ProfileFrameLimits::Kind)
                   ? ProfileCopyOutcome::UnsupportedFrame
                   : ProfileCopyOutcome::InvalidWrite;
    // Revocation after selection may leave a copied old item. Source/start/data
    // recheck original rights. There is no lossy lifecycle message requirement.
    return xQueueSendToBack(queue_, &frame, 0) == pdTRUE ? ProfileCopyOutcome::Copied
                                                         : ProfileCopyOutcome::QueueFull;
}
bool FreeRtosProfileFrameQueue::take(CopiedProfileFrame &frame) noexcept {
    return queue_ && xQueueReceive(queue_, &frame, 0) == pdTRUE;
}
} // namespace companion
} // namespace larp
