#pragma once
#include "Companion/IdentityAccess.h"
#include "FreeRtosCommandFrameQueue.h"
#include "FreeRtosProfileFrameQueue.h"
namespace larp {
namespace companion {
// Original immutable SDK/lifecycle observation, not a current-handle lookup
// while draining old events. Callback owner must synchronize its acquisition.
struct GattsWriteBinding {
    std::uint8_t interface = 0xff;
    std::uint16_t attribute = 0;
    std::array<std::uint8_t, 6> peerAddress = {};
    BoundConnection connection;
    TrustedAuthority authority;
};
// Borrows SDK value only through this call. SDK fields confer no rights.
struct GattsWriteEvent {
    std::uint8_t interface = 0xff;
    std::uint16_t connection = 0, attribute = 0, offset = 0;
    std::array<std::uint8_t, 6> peerAddress = {};
    std::uint32_t transaction = 0;
    const char *bytes = nullptr;
    std::size_t count = 0;
    bool prepared = false, needResponse = false;
};
enum class AttWriteStatus : std::uint8_t {
    Ok = 0x00,
    InvalidHandle = 0x01,
    InvalidPdu = 0x04,
    RequestUnsupported = 0x06,
    InvalidOffset = 0x07,
    InsufficientAuthorization = 0x08,
    InvalidValueLength = 0x0d,
    UnlikelyError = 0x0e,
    InsufficientResources = 0x11
};
struct RoutedWriteResponse {
    GattsWriteBinding original;
    std::uint32_t transaction = 0;
    // First raw callback time, never refreshed by queue/worker selection.
    std::uint32_t receivedAt = 0;
    std::uint16_t attribute = 0;
    AttWriteStatus status = AttWriteStatus::InsufficientAuthorization;
};
class IWriteResponseBearer {
  public:
    virtual ~IWriteResponseBearer() = default;
    // One bounded zero-wait copy. Recheck exact original SDK connection/generation/
    // peer/interface before actual submission; never respond on a recycled handle.
    // This copy is neither peer delivery nor command admission/execution.
    virtual bool copyWriteResponse(const RoutedWriteResponse &response) noexcept = 0;
};
enum class GattsWriteOutcome : std::uint8_t {
    Copied,
    QueueFull,
    Unauthorized,
    NoBinding,
    WrongInterface,
    WrongConnection,
    WrongAttribute,
    PreparedUnsupported,
    InvalidOffset,
    InvalidFrame,
    UnsupportedFrame,
    Unavailable
};
struct GattsWriteResult {
    GattsWriteOutcome outcome = GattsWriteOutcome::NoBinding;
    AttWriteStatus status = AttWriteStatus::InsufficientAuthorization;
    bool responseWanted = false, responseCopied = false;
};
// Raw task-context callback only. One copied queue value, one optional response,
// no heap/profile/JSON/filesystem/tick/retry. Caller owns the original binding.
GattsWriteResult admitGattsWrite(const GattsWriteEvent &event, const GattsWriteBinding &original,
                                 const IGameControlChannel &authority,
                                 FreeRtosCommandFrameQueue &queue, IWriteResponseBearer &responses,
                                 std::uint32_t receivedAt) noexcept;
// Same immutable original SDK gate, independent profile queue (kind5/6/7).
GattsWriteResult admitGattsProfileWrite(const GattsWriteEvent &, const GattsWriteBinding &,
                                        const IGameControlChannel &, FreeRtosProfileFrameQueue &,
                                        IWriteResponseBearer &, std::uint32_t receivedAt) noexcept;
} // namespace companion
} // namespace larp
