#pragma once
#include "Companion/PeerProofReceiver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace larp {
namespace companion {
struct PeerProofQueueLimits {
    static const std::size_t Depth = 4;
    static const std::uint32_t MaxAgeMilliseconds = 1000;
};
struct CopiedPeerProofFrame {
    PeerProofBinding original;
    std::uint32_t receivedAt = 0;
    std::uint16_t count = 0;
    std::array<char, CommandFrameLimits::Frame> bytes = {};
};
enum class PeerProofCopy : std::uint8_t { Copied, Full, Unavailable, Invalid, Stale, Expired };
// Process lifetime, begin before callbacks/tasks. One short source producer
// supplies its ORIGINAL closed connection/time. No credential/role/crypto here.
// Zero wait, four static copies. One separate receiver takes <=4 stale items
// and <=1 current item per call. Caller also proves actual source attribution.
class FreeRtosPeerProofQueue {
  public:
    FreeRtosPeerProofQueue(const IPeerProofContext &context, const IProfileImportClock &clock)
        : context_(context), clock_(clock) {}
    FreeRtosPeerProofQueue(const FreeRtosPeerProofQueue &) = delete;
    FreeRtosPeerProofQueue &operator=(const FreeRtosPeerProofQueue &) = delete;
    bool begin() noexcept;
    PeerProofCopy copyWrite(const PeerProofBinding &, const char *, std::size_t,
                            std::uint32_t receivedAt, bool prepared = false,
                            std::uint16_t writeOffset = 0) noexcept;
    bool takeCurrent(CopiedPeerProofFrame &) noexcept;

  private:
    const IPeerProofContext &context_;
    const IProfileImportClock &clock_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(CopiedPeerProofFrame)
        std::uint8_t storage_[PeerProofQueueLimits::Depth * sizeof(CopiedPeerProofFrame)] = {};
    bool taking_ = false;
};
struct PeerProofDispatch {
    bool received = false;
    PeerProofReceive result;
};
PeerProofDispatch dispatchPeerProofFrame(FreeRtosPeerProofQueue &, PeerProofReceiver &) noexcept;
} // namespace companion
} // namespace larp
