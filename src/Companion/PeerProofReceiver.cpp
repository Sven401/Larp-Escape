#include "Companion/PeerProofReceiver.h"
#include <cstring>
namespace larp {
namespace companion {
PeerProofReceive PeerProofReceiver::receive(const PeerProofBinding &caller, const char *callerBytes,
                                            std::size_t count) noexcept {
    const auto original = caller;
    char owned[CommandFrameLimits::Frame];
    PeerProofReceive result;
    if (busy_) {
        result.kind = PeerProofReceiveKind::Busy;
        return result;
    }
    if (!callerBytes || count > sizeof(owned) || count <= CommandFrameLimits::Header)
        return result;
    std::memcpy(owned, callerBytes, count); // Before clock/context/owner observation.
    CommandFrame frame;
    const auto decoded = decodePeerProofFrame(owned, count, frame);
    if (decoded != FrameDecodeResult::Success || frame.total != PeerProofLimits::ProofBytes) {
        if (decoded == FrameDecodeResult::Unsupported)
            result.kind = PeerProofReceiveKind::UnsupportedFrame;
        return result;
    }
    busy_ = true;
    PeerChallenge current;
    if (!owner_.challenge(original, current)) {
        result.kind = PeerProofReceiveKind::Stale;
        busy_ = false;
        return result;
    }
    if (message_ && !(original_ == original)) {
        message_ = received_ = 0;
    }
    result.message = frame.message;
    result.received = received_;
    if (message_ && message_ != frame.message) {
        result.kind = PeerProofReceiveKind::Busy;
        busy_ = false;
        return result;
    }
    if (!message_) {
        if (frame.offset) {
            result.kind = PeerProofReceiveKind::OutOfOrder;
            busy_ = false;
            return result;
        }
        original_ = original;
        message_ = frame.message;
    }
    if (frame.offset < received_ &&
        frame.count <= static_cast<std::size_t>(received_ - frame.offset)) {
        if (!std::memcmp(bytes_.data() + frame.offset, frame.payload, frame.count))
            result.kind = PeerProofReceiveKind::DuplicateFragment;
        else {
            result.kind = PeerProofReceiveKind::Conflict;
            message_ = received_ = 0;
        }
        busy_ = false;
        return result;
    }
    if (frame.offset != received_) {
        result.kind = PeerProofReceiveKind::OutOfOrder;
        busy_ = false;
        return result;
    }
    std::memcpy(bytes_.data() + received_, frame.payload, frame.count);
    received_ += static_cast<std::uint16_t>(frame.count);
    result.received = received_;
    result.kind = PeerProofReceiveKind::Receiving;
    if (received_ == PeerProofLimits::ProofBytes) {
        PeerProof proof;
        if (!decodePeerProof(reinterpret_cast<const std::uint8_t *>(bytes_.data()), bytes_.size(),
                             proof))
            result.kind = PeerProofReceiveKind::DecodeRejected;
        else {
            result.kind = PeerProofReceiveKind::Completed;
            result.proof = owner_.verify(original_, proof);
        }
        message_ = received_ = 0;
    }
    busy_ = false;
    return result;
}
} // namespace companion
} // namespace larp
