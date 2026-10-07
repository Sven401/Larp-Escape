#pragma once
#include "Companion/ProfileSourceEnvelope.h"
#include "FreeRtosGattsResponseQueue.h"
namespace larp {
namespace companion {
struct GattsConnectionEvent {
    std::uint8_t interface = 0xff;
    std::uint16_t connection = 0;
    std::array<std::uint8_t, 6> peerAddress = {};
};
struct GattsEndpoints {
    std::uint8_t interface = 0xff;
    std::uint16_t command = 0, profile = 0;
};
enum class GattsLifecycleOutcome : std::uint8_t {
    Opened,
    Closed,
    Ignored,
    Busy,
    Invalid,
    Unavailable,
    Conflict
};
enum class GattsEndpoint : std::uint8_t { Command, Profile };
// One serialized immediate callback producer owns lifecycle AND IdentityAccess.
// SDK ordering/completeness at this boundary must be proved by its integration.
// No delayed SDK-handle/address lookup, GAP-auth inference or SDK sender here.
// Worker reads only immutable snapshots under a short lock; no external port
// runs under that lock. Same identity/channel is required for admission.
class FreeRtosGattsConnection : public IGattsResponseContext {
  public:
    FreeRtosGattsConnection(IdentityAccess &access, const IProfileImportClock &clock,
                            GattsEndpoints endpoints)
        : access_(access), clock_(clock), endpoints_(endpoints) {}
    FreeRtosGattsConnection(const FreeRtosGattsConnection &) = delete;
    FreeRtosGattsConnection &operator=(const FreeRtosGattsConnection &) = delete;
    GattsLifecycleOutcome connectNow(const GattsConnectionEvent &, BoundConnection &) noexcept;
    GattsLifecycleOutcome disconnectNow(const GattsConnectionEvent &) noexcept;
    // Deferred local work MUST retain its original token. It cannot reacquire
    // the current token from raw SDK fields while draining an old queue.
    GattsLifecycleOutcome retireOriginal(const GattsWriteBinding &) noexcept;
    // Immediate callback only, before any event is deferred. The caller must
    // already own a bounded value copy before the authority observation.
    bool captureWriteNow(const GattsWriteEvent &, GattsEndpoint, GattsWriteBinding &) noexcept;
    bool originalLive(const GattsWriteBinding &) const noexcept override;
    std::uint32_t nowMilliseconds() const noexcept override;

  private:
    IdentityAccess &access_;
    const IProfileImportClock &clock_;
    const GattsEndpoints endpoints_;
    GattsWriteBinding live_;
    mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    bool active_ = false, busy_ = false;
    bool validEndpoints() const noexcept;
    bool matches(const GattsWriteBinding &) const noexcept;
    GattsLifecycleOutcome closeOwned(const GattsWriteBinding &) noexcept;
};
// Freeze SDK scalars and <=128 borrowed bytes BEFORE lifecycle/authority/clock
// observation. One original binding and at most one admission/ACK copy.
GattsWriteResult admitOriginalGattsWrite(const GattsWriteEvent &, FreeRtosGattsConnection &,
                                         const IGameControlChannel &, FreeRtosCommandFrameQueue &,
                                         IWriteResponseBearer &, std::uint32_t receivedAt) noexcept;
GattsWriteResult admitOriginalGattsProfileWrite(const GattsWriteEvent &, FreeRtosGattsConnection &,
                                                const IGameControlChannel &,
                                                FreeRtosProfileFrameQueue &, IWriteResponseBearer &,
                                                std::uint32_t receivedAt) noexcept;
} // namespace companion
} // namespace larp
