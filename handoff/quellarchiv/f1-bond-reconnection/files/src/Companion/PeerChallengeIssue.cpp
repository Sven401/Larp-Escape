#include "Companion/PeerChallengeIssue.h"
#include <cstring>
namespace larp {
namespace companion {
bool encodePeerChallengeIssue(const Identity &caller, std::array<std::uint8_t, 18> &out) noexcept {
    const auto peer = caller;
    if (!peer.valid())
        return false;
    std::array<std::uint8_t, 18> value = {};
    value[0] = PeerChallengeIssueLimits::Version;
    value[1] = PeerChallengeIssueLimits::Kind;
    std::memcpy(value.data() + 2, peer.bytes.data(), peer.bytes.size());
    out = value;
    return true;
}
bool decodePeerChallengeIssue(const std::uint8_t *bytes, std::size_t count,
                              Identity &out) noexcept {
    if (!bytes || count != PeerChallengeIssueLimits::Bytes ||
        bytes[0] != PeerChallengeIssueLimits::Version || bytes[1] != PeerChallengeIssueLimits::Kind)
        return false;
    Identity value;
    std::memcpy(value.bytes.data(), bytes + 2, value.bytes.size());
    if (!value.valid())
        return false;
    out = value;
    return true;
}
} // namespace companion
} // namespace larp
