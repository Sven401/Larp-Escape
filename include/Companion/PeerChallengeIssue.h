#pragma once
#include "Companion/PeerChallengeRead.h"
namespace larp {
namespace companion {
struct PeerChallengeIssueLimits {
    static const std::uint8_t Version = 1, Kind = 11;
    static const std::size_t Bytes = 18, Depth = 4;
    static const std::uint32_t MaxAgeMilliseconds = 1000;
};
// Public selector only. Key and generation always come from the trusted registry.
bool encodePeerChallengeIssue(const Identity &, std::array<std::uint8_t, 18> &) noexcept;
bool decodePeerChallengeIssue(const std::uint8_t *, std::size_t, Identity &) noexcept;
struct PeerChallengeIssueRequest {
    // Original transport scope, copied before the first external port call.
    PeerChallengeReadRequest scope;
    std::array<std::uint8_t, PeerChallengeIssueLimits::Bytes> bytes = {};
};
class IPeerChallengeIssueIngress {
  public:
    virtual ~IPeerChallengeIssueIngress() = default;
    virtual bool copyIssue(const PeerChallengeIssueRequest &) noexcept = 0;
};
} // namespace companion
} // namespace larp
