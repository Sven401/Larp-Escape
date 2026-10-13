#include "Companion/PeerAuthResult.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
void put(std::uint8_t *out, std::uint32_t value, unsigned count) {
    for (unsigned n = 0; n < count; ++n)
        out[n] = static_cast<std::uint8_t>(value >> (8 * n));
}
std::uint32_t get(const std::uint8_t *bytes, unsigned count) {
    std::uint32_t value = 0;
    for (unsigned n = 0; n < count; ++n)
        value |= std::uint32_t(bytes[n]) << (8 * n);
    return value;
}
bool valid(const PeerAuthResult &v) {
    if (!v.message || !v.sequence || !v.peer.valid() || v.received > PeerProofLimits::ProofBytes)
        return false;
    switch (v.state) {
    case PeerAuthState::Receiving:
        return v.received > 0 && v.received < PeerProofLimits::ProofBytes &&
               v.reason == PeerAuthReason::None && !v.principal;
    case PeerAuthState::Rejected:
        return (v.reason == PeerAuthReason::FrameRejected ||
                v.reason == PeerAuthReason::ProofRejected ||
                v.reason == PeerAuthReason::GrantRejected) &&
               !v.principal;
    case PeerAuthState::GrantPending:
    case PeerAuthState::Authenticated:
        return v.received == PeerProofLimits::ProofBytes && v.reason == PeerAuthReason::None &&
               !v.principal;
    case PeerAuthState::Granted:
        return v.received == PeerProofLimits::ProofBytes && v.reason == PeerAuthReason::None &&
               v.principal;
    }
    return false;
}
} // namespace
bool encodePeerAuthResult(const PeerAuthResult &caller,
                          std::array<std::uint8_t, 32> &out) noexcept {
    const auto v = caller;
    if (!valid(v))
        return false;
    std::array<std::uint8_t, 32> bytes = {};
    bytes[0] = PeerAuthLimits::Version;
    bytes[1] = PeerAuthLimits::Kind;
    bytes[2] = static_cast<std::uint8_t>(v.state);
    bytes[3] = static_cast<std::uint8_t>(v.reason);
    put(bytes.data() + 4, v.message, 2);
    put(bytes.data() + 6, v.received, 2);
    put(bytes.data() + 8, v.sequence, 4);
    put(bytes.data() + 12, v.principal, 4);
    std::memcpy(bytes.data() + 16, v.peer.bytes.data(), 16);
    out = bytes;
    return true;
}
bool decodePeerAuthResult(const std::uint8_t *bytes, std::size_t count,
                          PeerAuthResult &out) noexcept {
    if (!bytes || count != PeerAuthLimits::Bytes || bytes[0] != PeerAuthLimits::Version ||
        bytes[1] != PeerAuthLimits::Kind)
        return false;
    PeerAuthResult v;
    v.state = static_cast<PeerAuthState>(bytes[2]);
    v.reason = static_cast<PeerAuthReason>(bytes[3]);
    v.message = static_cast<std::uint16_t>(get(bytes + 4, 2));
    v.received = static_cast<std::uint16_t>(get(bytes + 6, 2));
    v.sequence = get(bytes + 8, 4);
    v.principal = get(bytes + 12, 4);
    std::memcpy(v.peer.bytes.data(), bytes + 16, 16);
    if (!valid(v))
        return false;
    out = v;
    return true;
}
} // namespace companion
} // namespace larp
