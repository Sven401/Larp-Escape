#pragma once
#include "Companion/PeerProof.h"
namespace larp {
namespace companion {
struct PeerChallengeReadLimits {
    static const std::size_t Depth = 4;
    // Local admission/response policy, not a measured ATT timeout or throughput.
    static const std::uint32_t MaxAgeMilliseconds = 1000;
};
struct PeerChallengeReadRequest {
    PeerProofBinding original;
    std::array<std::uint8_t, 6> address = {};
    std::uint32_t lifecycle = 0, transaction = 0, receivedAt = 0;
    std::uint16_t attribute = 0, offset = 0;
    bool isLong = false;
    bool operator==(const PeerChallengeReadRequest &other) const noexcept {
        return original == other.original && address == other.address &&
               lifecycle == other.lifecycle && transaction == other.transaction &&
               receivedAt == other.receivedAt && attribute == other.attribute &&
               offset == other.offset && isLong == other.isLong;
    }
};
struct PeerChallengeReadReply {
    PeerChallengeReadRequest request;
    PeerChallengeSnapshot snapshot;
    // ATT Ok=0, InvalidOffset=7 or InsufficientAuthorization=8 only.
    std::uint8_t status = 8;
};
class IPeerChallengeIssueIngress;
class IPeerAuthIngress;
class IPeerChallengeReadIngress {
  public:
    virtual ~IPeerChallengeReadIngress() = default;
    virtual bool copyRead(const PeerChallengeReadRequest &) noexcept = 0;
    virtual bool workerNow() const noexcept = 0;
};
// Frozen installation bundle; all referenced objects have process lifetime.
struct PeerChallengeReadSetup {
    BootIdentity boot;
    const IPeerProofContext *context = nullptr;
    IPeerChallengeReadIngress *ingress = nullptr;
    IPeerChallengeIssueIngress *issue = nullptr;
    IPeerAuthIngress *auth = nullptr;
    bool operator==(const PeerChallengeReadSetup &other) const noexcept {
        return boot == other.boot && context == other.context && ingress == other.ingress &&
               issue == other.issue && auth == other.auth;
    }
};
} // namespace companion
} // namespace larp
