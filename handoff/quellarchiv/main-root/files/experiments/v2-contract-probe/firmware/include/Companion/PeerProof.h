#pragma once
#include "Companion/IdentityAccess.h"
#include "Companion/ProfileSourceEnvelope.h"
namespace larp {
namespace companion {
struct PeerProofLimits {
    static const std::uint8_t Version = 1, Algorithm = 1, ChallengeKind = 9, ProofKind = 10;
    static const std::size_t ChallengeBytes = 108, ProofBytes = 72;
    static const std::uint32_t MaxAgeMilliseconds = 30000;
};
using PeerPublicKey = std::array<std::uint8_t, 65>;
using PeerSignature = std::array<std::uint8_t, 64>;
using PeerNonce = std::array<std::uint8_t, 32>;
using PeerTranscript = std::array<std::uint8_t, PeerProofLimits::ChallengeBytes>;
struct PeerProofBinding {
    Identity device;
    BootIdentity boot;
    BoundConnection connection;
    std::uint32_t registration = 0;
    std::uint8_t interface = 0xff;
    bool valid() const noexcept;
    bool operator==(const PeerProofBinding &other) const noexcept {
        return device == other.device && boot == other.boot && connection == other.connection &&
               registration == other.registration && interface == other.interface;
    }
};
// Trusted registry copy. Neither public key nor generation comes from a proof.
struct PeerCredential {
    Identity peer;
    PeerPublicKey key = {};
    std::uint32_t generation = 0;
    bool operator==(const PeerCredential &other) const noexcept {
        return peer == other.peer && key == other.key && generation == other.generation;
    }
};
struct PeerChallenge {
    PeerProofBinding original;
    Identity peer;
    std::uint32_t sequence = 0;
    PeerNonce nonce = {};
};
// Sole-worker export; preserves the original absolute challenge deadline.
struct PeerChallengeSnapshot {
    PeerChallenge challenge;
    PeerCredential credential;
    std::uint32_t issuedAt = 0;
};
struct PeerProof {
    std::uint32_t sequence = 0;
    PeerSignature signature = {};
};
// Key possession only. Does not assert transport encryption, a bond or GM rights.
struct VerifiedPeerProof {
    PeerProofBinding original;
    PeerCredential credential;
    std::uint32_t sequence = 0, issuedAt = 0, verifiedAt = 0;
};
bool encodePeerChallenge(const PeerChallenge &, PeerTranscript &) noexcept;
bool decodePeerChallenge(const std::uint8_t *, std::size_t, PeerChallenge &) noexcept;
bool encodePeerProof(const PeerProof &,
                     std::array<std::uint8_t, PeerProofLimits::ProofBytes> &) noexcept;
bool decodePeerProof(const std::uint8_t *, std::size_t, PeerProof &) noexcept;
class IPeerProofContext {
  public:
    virtual ~IPeerProofContext() = default;
    // Original closed token captured before deferral; includes actual service
    // readiness. No late SDK handle/MAC lookup or adoption of a current token.
    virtual bool originalLive(const PeerProofBinding &) const noexcept = 0;
    virtual bool credentialCurrent(const PeerCredential &) const noexcept = 0;
};
class IPeerProofCrypto {
  public:
    virtual ~IPeerProofCrypto() = default;
    virtual bool validKey(const PeerPublicKey &) noexcept = 0;
    virtual bool verify(const PeerPublicKey &, const PeerTranscript &,
                        const PeerSignature &) noexcept = 0;
};
class IPeerProofNonce {
  public:
    virtual ~IPeerProofNonce() = default;
    // Cryptographic entropy, not display-ID generation/clock/counter/MAC. False
    // preserves destination. Production adapter must prove its entropy source.
    virtual bool make(PeerNonce &) noexcept = 0;
};
enum class PeerProofOutcome : std::uint8_t {
    Issued,
    Verified,
    Duplicate,
    Busy,
    Stale,
    Expired,
    InvalidContext,
    InvalidCredential,
    EntropyFailure,
    SignatureRejected,
    Mismatch,
    Consumed,
    Exhausted
};
// One separate serialized crypto worker; never crypto in a short SDK callback.
// One challenge per original connection, consumed before external verification.
// Clock/context ports may observe source closure; check again after crypto.
class PeerProofOwner {
  public:
    PeerProofOwner(IPeerProofContext &context, IPeerProofCrypto &crypto, IPeerProofNonce &nonce,
                   const IProfileImportClock &clock, std::uint32_t initialSequence = 0)
        : context_(context), crypto_(crypto), nonce_(nonce), clock_(clock),
          sequence_(initialSequence) {}
    PeerProofOwner(const PeerProofOwner &) = delete;
    PeerProofOwner &operator=(const PeerProofOwner &) = delete;
    PeerProofOutcome issue(const PeerProofBinding &, const PeerCredential &) noexcept;
    bool challenge(const PeerProofBinding &, PeerChallenge &) noexcept;
    bool challengeSnapshot(const PeerProofBinding &, PeerChallengeSnapshot &) noexcept;
    PeerProofOutcome verify(const PeerProofBinding &, const PeerProof &) noexcept;
    bool readVerified(const PeerProofBinding &, VerifiedPeerProof &) noexcept;

  private:
    enum class Phase : std::uint8_t { Empty, Pending, Verified, Failed };
    IPeerProofContext &context_;
    IPeerProofCrypto &crypto_;
    IPeerProofNonce &nonce_;
    const IProfileImportClock &clock_;
    PeerChallenge challenge_;
    PeerCredential credential_;
    PeerProof accepted_;
    std::uint32_t sequence_ = 0, issuedAt_ = 0, verifiedAt_ = 0;
    Phase phase_ = Phase::Empty;
    bool busy_ = false;
    bool current() const noexcept;
    bool expired() const noexcept;
};
// Separately trusted, originally attributed SDK security evidence. A signature
// cannot fabricate these flags. Caller must recheck the proof/context at grant.
struct OriginalPeerSecurity {
    PeerProofBinding original;
    bool encrypted = false, mitm = false, secureConnections = false, bonded = false;
};
bool bindPeerProofEvidence(const VerifiedPeerProof &, const OriginalPeerSecurity &,
                           VerifiedPeerEvidence &) noexcept;
} // namespace companion
} // namespace larp
