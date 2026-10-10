#pragma once
#include "Companion/CommandFrame.h"
#include "Companion/PeerChallengeRead.h"
#include "Companion/PeerProofGrant.h"
namespace larp {
namespace companion {
struct PeerAuthLimits {
    static const std::uint8_t Version = 1, Kind = 12;
    static const std::size_t Bytes = 32, Depth = 4;
    static const std::uint32_t MaxAgeMilliseconds = 1000;
};
enum class PeerAuthState : std::uint8_t {
    Receiving = 1,
    Rejected = 2,
    GrantPending = 3,
    Authenticated = 4,
    Granted = 5
};
enum class PeerAuthReason : std::uint8_t {
    None = 0,
    FrameRejected = 1,
    ProofRejected = 2,
    GrantRejected = 3
};
struct PeerAuthResult {
    PeerAuthState state = PeerAuthState::Rejected;
    PeerAuthReason reason = PeerAuthReason::FrameRejected;
    std::uint16_t message = 0, received = 0;
    std::uint32_t sequence = 0, principal = 0;
    Identity peer;
};
bool encodePeerAuthResult(const PeerAuthResult &, std::array<std::uint8_t, 32> &) noexcept;
bool decodePeerAuthResult(const std::uint8_t *, std::size_t, PeerAuthResult &) noexcept;
struct PeerProofWriteRequest {
    PeerChallengeReadRequest scope;
    std::uint16_t count = 0;
    std::array<char, CommandFrameLimits::Frame> bytes = {};
};
struct PeerAuthReadReply {
    PeerChallengeReadRequest request;
    PeerChallengeSnapshot snapshot;
    PeerAuthResult result;
    PeerProofGrantJob job;
    TrustedAuthority ack;
    std::uint32_t frozenAt = 0;
    std::uint8_t status = 8;
};
class IPeerAuthIngress {
  public:
    virtual ~IPeerAuthIngress() = default;
    virtual bool copyProof(const PeerProofWriteRequest &) noexcept = 0;
    virtual bool copyResultRead(const PeerChallengeReadRequest &) noexcept = 0;
    virtual bool workerNow() const noexcept = 0;
};
} // namespace companion
} // namespace larp
