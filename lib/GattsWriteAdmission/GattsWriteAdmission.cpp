#include "GattsWriteAdmission.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
FrameCopyOutcome normalized(FrameCopyOutcome value) {
    return value;
}
FrameCopyOutcome normalized(ProfileCopyOutcome value) {
    switch (value) {
    case ProfileCopyOutcome::Copied:
        return FrameCopyOutcome::Copied;
    case ProfileCopyOutcome::QueueFull:
        return FrameCopyOutcome::QueueFull;
    case ProfileCopyOutcome::Unauthorized:
        return FrameCopyOutcome::Unauthorized;
    case ProfileCopyOutcome::InvalidWrite:
        return FrameCopyOutcome::InvalidWrite;
    case ProfileCopyOutcome::UnsupportedFrame:
        return FrameCopyOutcome::UnsupportedFrame;
    case ProfileCopyOutcome::Uninitialized:
        return FrameCopyOutcome::Uninitialized;
    }
    return FrameCopyOutcome::InvalidWrite;
}
template <typename Queue>
GattsWriteResult admit(const GattsWriteEvent &callerEvent, const GattsWriteBinding &callerBinding,
                       const IGameControlChannel &authority, Queue &queue,
                       IWriteResponseBearer &responses, std::uint32_t receivedAt) noexcept {
    const auto event = callerEvent;
    const auto original = callerBinding;
    GattsWriteResult result;
    bool hasPeerAddress = false;
    for (const auto byte : original.peerAddress)
        hasPeerAddress = hasPeerAddress || byte != 0;
    if (!hasPeerAddress || !original.connection.token || !original.attribute ||
        original.interface == 0xff)
        return result;
    if (event.interface != original.interface) {
        result.outcome = GattsWriteOutcome::WrongInterface;
        return result;
    }
    if (event.connection != original.connection.handle ||
        event.peerAddress != original.peerAddress) {
        result.outcome = GattsWriteOutcome::WrongConnection;
        return result;
    }
    // Errors may be copied only for the selected original SDK context. The bearer
    // independently rechecks liveness; unknown/recycled peers get no relabeled ack.
    char owned[CommandFrameLimits::Frame];
    const bool bounded = event.bytes && event.count > CommandFrameLimits::Header &&
                         event.count <= CommandFrameLimits::Frame;
    if (bounded)
        std::memcpy(owned, event.bytes, event.count);
    result.responseWanted = event.needResponse;
    TrustedAuthority current;
    if (!original.authority.gameMaster || !original.authority.principal ||
        !original.authority.epoch || !authority.readAuthority(current) ||
        !(current == original.authority)) {
        result.outcome = GattsWriteOutcome::Unauthorized;
    } else if (event.attribute != original.attribute) {
        result.outcome = GattsWriteOutcome::WrongAttribute;
        result.status = AttWriteStatus::InvalidHandle;
    } else if (event.prepared) {
        result.outcome = GattsWriteOutcome::PreparedUnsupported;
        result.status = AttWriteStatus::RequestUnsupported;
    } else if (event.offset) {
        result.outcome = GattsWriteOutcome::InvalidOffset;
        result.status = AttWriteStatus::InvalidOffset;
    } else if (!bounded) {
        result.outcome = GattsWriteOutcome::InvalidFrame;
        result.status = AttWriteStatus::InvalidValueLength;
    } else {
        const auto copied = queue.copyWrite(owned, event.count, original.authority.principal,
                                            original.authority.epoch, receivedAt);
        switch (normalized(copied)) {
        case FrameCopyOutcome::Copied:
            result.outcome = GattsWriteOutcome::Copied;
            result.status = AttWriteStatus::Ok;
            break;
        case FrameCopyOutcome::QueueFull:
            result.outcome = GattsWriteOutcome::QueueFull;
            result.status = AttWriteStatus::InsufficientResources;
            break;
        case FrameCopyOutcome::Unauthorized:
            result.outcome = GattsWriteOutcome::Unauthorized;
            break;
        case FrameCopyOutcome::UnsupportedFrame:
            result.outcome = GattsWriteOutcome::UnsupportedFrame;
            result.status = AttWriteStatus::RequestUnsupported;
            break;
        case FrameCopyOutcome::InvalidWrite:
            result.outcome = GattsWriteOutcome::InvalidFrame;
            result.status = AttWriteStatus::InvalidPdu;
            break;
        case FrameCopyOutcome::Uninitialized:
            result.outcome = GattsWriteOutcome::Unavailable;
            result.status = AttWriteStatus::UnlikelyError;
            break;
        }
    }
    if (result.responseWanted) {
        RoutedWriteResponse response;
        response.original = original;
        response.transaction = event.transaction;
        response.attribute = event.attribute;
        response.status = result.status;
        result.responseCopied = responses.copyWriteResponse(response);
    }
    return result;
}
} // namespace
GattsWriteResult admitGattsWrite(const GattsWriteEvent &event, const GattsWriteBinding &original,
                                 const IGameControlChannel &authority,
                                 FreeRtosCommandFrameQueue &queue, IWriteResponseBearer &responses,
                                 std::uint32_t receivedAt) noexcept {
    return admit(event, original, authority, queue, responses, receivedAt);
}
GattsWriteResult
admitGattsProfileWrite(const GattsWriteEvent &event, const GattsWriteBinding &original,
                       const IGameControlChannel &authority, FreeRtosProfileFrameQueue &queue,
                       IWriteResponseBearer &responses, std::uint32_t receivedAt) noexcept {
    return admit(event, original, authority, queue, responses, receivedAt);
}
} // namespace companion
} // namespace larp
