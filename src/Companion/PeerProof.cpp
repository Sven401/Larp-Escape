#include "Companion/PeerProof.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
const std::uint8_t domain[16] = {'L', 'A', 'R', 'P', '-', 'P', 'E', 'E',
                                 'R', '-', 'P', '2', '5', '6', '-', '1'};
template <std::size_t N> bool nonzero(const std::array<std::uint8_t, N> &value) {
    for (const auto byte : value)
        if (byte)
            return true;
    return false;
}
void put32(std::uint8_t *p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
std::uint32_t get32(const std::uint8_t *p) {
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i)
        v |= std::uint32_t(p[i]) << (8 * i);
    return v;
}
bool challengeValid(const PeerChallenge &v) {
    return v.original.valid() && v.peer.valid() && v.sequence && nonzero(v.nonce);
}
} // namespace
bool PeerProofBinding::valid() const noexcept {
    return device.valid() && boot.valid() && connection.token && registration && interface &&
           interface != 0xff && (connection.handle & 0xff) == interface;
}
bool encodePeerChallenge(const PeerChallenge &v, PeerTranscript &destination) noexcept {
    if (!challengeValid(v))
        return false;
    PeerTranscript bytes = {};
    std::memcpy(bytes.data(), domain, sizeof(domain));
    bytes[16] = PeerProofLimits::Version;
    bytes[17] = PeerProofLimits::ChallengeKind;
    bytes[18] = PeerProofLimits::Algorithm;
    std::memcpy(bytes.data() + 20, v.original.device.bytes.data(), 16);
    put32(bytes.data() + 36, v.original.boot.high);
    put32(bytes.data() + 40, v.original.boot.low);
    put32(bytes.data() + 44, v.original.registration);
    put32(bytes.data() + 48, v.original.connection.token);
    bytes[52] = static_cast<std::uint8_t>(v.original.connection.handle);
    bytes[53] = static_cast<std::uint8_t>(v.original.connection.handle >> 8);
    bytes[54] = v.original.interface;
    put32(bytes.data() + 56, v.sequence);
    std::memcpy(bytes.data() + 60, v.peer.bytes.data(), 16);
    std::memcpy(bytes.data() + 76, v.nonce.data(), 32);
    destination = bytes;
    return true;
}
bool decodePeerChallenge(const std::uint8_t *b, std::size_t n,
                         PeerChallenge &destination) noexcept {
    if (!b || n != PeerProofLimits::ChallengeBytes || std::memcmp(b, domain, 16) ||
        b[16] != PeerProofLimits::Version || b[17] != PeerProofLimits::ChallengeKind ||
        b[18] != PeerProofLimits::Algorithm || b[19] || b[55])
        return false;
    PeerChallenge v;
    std::memcpy(v.original.device.bytes.data(), b + 20, 16);
    v.original.boot.high = get32(b + 36);
    v.original.boot.low = get32(b + 40);
    v.original.registration = get32(b + 44);
    v.original.connection.token = get32(b + 48);
    v.original.connection.handle = std::uint16_t(b[52]) | (std::uint16_t(b[53]) << 8);
    v.original.interface = b[54];
    v.sequence = get32(b + 56);
    std::memcpy(v.peer.bytes.data(), b + 60, 16);
    std::memcpy(v.nonce.data(), b + 76, 32);
    if (!challengeValid(v))
        return false;
    destination = v;
    return true;
}
bool encodePeerProof(const PeerProof &v,
                     std::array<std::uint8_t, PeerProofLimits::ProofBytes> &destination) noexcept {
    if (!v.sequence || !nonzero(v.signature))
        return false;
    std::array<std::uint8_t, PeerProofLimits::ProofBytes> b = {};
    b[0] = PeerProofLimits::Version;
    b[1] = PeerProofLimits::ProofKind;
    b[2] = PeerProofLimits::Algorithm;
    put32(b.data() + 4, v.sequence);
    std::memcpy(b.data() + 8, v.signature.data(), 64);
    destination = b;
    return true;
}
bool decodePeerProof(const std::uint8_t *b, std::size_t n, PeerProof &destination) noexcept {
    if (!b || n != PeerProofLimits::ProofBytes || b[0] != PeerProofLimits::Version ||
        b[1] != PeerProofLimits::ProofKind || b[2] != PeerProofLimits::Algorithm || b[3])
        return false;
    PeerProof v;
    v.sequence = get32(b + 4);
    std::memcpy(v.signature.data(), b + 8, 64);
    if (!v.sequence || !nonzero(v.signature))
        return false;
    destination = v;
    return true;
}
bool PeerProofOwner::expired() const noexcept {
    return clock_.nowMilliseconds() - issuedAt_ >= PeerProofLimits::MaxAgeMilliseconds;
}
bool PeerProofOwner::current() const noexcept {
    return context_.originalLive(challenge_.original) && context_.credentialCurrent(credential_);
}
PeerProofOutcome PeerProofOwner::issue(const PeerProofBinding &caller,
                                       const PeerCredential &callerCredential) noexcept {
    const auto original = caller;
    const auto credential = callerCredential;
    if (busy_)
        return PeerProofOutcome::Busy;
    busy_ = true;
    if (phase_ != Phase::Empty && challenge_.original == original) {
        busy_ = false;
        return PeerProofOutcome::Consumed;
    }
    if (phase_ != Phase::Empty && context_.originalLive(challenge_.original)) {
        busy_ = false;
        return PeerProofOutcome::Busy;
    }
    if (!original.valid() || !context_.originalLive(original)) {
        busy_ = false;
        return PeerProofOutcome::InvalidContext;
    }
    if (!credential.peer.valid() || !credential.generation ||
        !context_.credentialCurrent(credential)) {
        busy_ = false;
        return PeerProofOutcome::InvalidCredential;
    }
    if (sequence_ == UINT32_MAX) {
        busy_ = false;
        return PeerProofOutcome::Exhausted;
    }
    challenge_ = {};
    challenge_.original = original;
    challenge_.peer = credential.peer;
    challenge_.sequence = ++sequence_;
    credential_ = credential;
    accepted_ = {};
    phase_ = Phase::Failed;
    issuedAt_ = clock_.nowMilliseconds();
    verifiedAt_ = 0;
    auto result = PeerProofOutcome::InvalidCredential;
    if (crypto_.validKey(credential_.key)) {
        PeerNonce nonce = {};
        if (nonce_.make(nonce) && nonzero(nonce)) {
            if (expired())
                result = PeerProofOutcome::Expired;
            else if (current()) {
                challenge_.nonce = nonce;
                phase_ = Phase::Pending;
                result = PeerProofOutcome::Issued;
            } else
                result = PeerProofOutcome::Stale;
        } else
            result = PeerProofOutcome::EntropyFailure;
    }
    busy_ = false;
    return result;
}
bool PeerProofOwner::challenge(const PeerProofBinding &caller,
                               PeerChallenge &destination) noexcept {
    const auto original = caller;
    if (busy_ || (phase_ != Phase::Pending && phase_ != Phase::Verified) ||
        !(original == challenge_.original))
        return false;
    busy_ = true;
    const bool valid = !expired() && current();
    if (valid)
        destination = challenge_;
    else
        phase_ = Phase::Failed;
    busy_ = false;
    return valid;
}
bool PeerProofOwner::challengeSnapshot(const PeerProofBinding &caller,
                                       PeerChallengeSnapshot &destination) noexcept {
    const auto original = caller;
    if (busy_ || (phase_ != Phase::Pending && phase_ != Phase::Verified) ||
        !(original == challenge_.original))
        return false;
    busy_ = true;
    const bool valid = !expired() && current();
    if (valid) {
        destination.challenge = challenge_;
        destination.credential = credential_;
        destination.issuedAt = issuedAt_;
    } else
        phase_ = Phase::Failed;
    busy_ = false;
    return valid;
}
PeerProofOutcome PeerProofOwner::verify(const PeerProofBinding &caller,
                                        const PeerProof &callerProof) noexcept {
    const auto original = caller;
    const auto proof = callerProof;
    if (busy_)
        return PeerProofOutcome::Busy;
    if (phase_ == Phase::Empty || !(original == challenge_.original))
        return PeerProofOutcome::Stale;
    busy_ = true;
    PeerProofOutcome result = PeerProofOutcome::Consumed;
    if (expired()) {
        phase_ = Phase::Failed;
        result = PeerProofOutcome::Expired;
    } else if (!current()) {
        phase_ = Phase::Failed;
        result = PeerProofOutcome::Stale;
    } else if (proof.sequence != challenge_.sequence)
        result = PeerProofOutcome::Mismatch;
    else if (phase_ == Phase::Verified) {
        if (proof.signature == accepted_.signature)
            result = PeerProofOutcome::Duplicate;
        else {
            phase_ = Phase::Failed;
            result = PeerProofOutcome::SignatureRejected;
        }
    } else if (phase_ == Phase::Pending) {
        phase_ = Phase::Failed; // One attempt consumed BEFORE external crypto.
        PeerTranscript transcript;
        result = PeerProofOutcome::SignatureRejected;
        if (nonzero(proof.signature) && encodePeerChallenge(challenge_, transcript) &&
            crypto_.verify(credential_.key, transcript, proof.signature)) {
            verifiedAt_ = clock_.nowMilliseconds();
            if (verifiedAt_ - issuedAt_ >= PeerProofLimits::MaxAgeMilliseconds)
                result = PeerProofOutcome::Expired;
            else if (!current())
                result = PeerProofOutcome::Stale;
            else {
                accepted_ = proof;
                phase_ = Phase::Verified;
                result = PeerProofOutcome::Verified;
            }
        }
    }
    busy_ = false;
    return result;
}
bool PeerProofOwner::readVerified(const PeerProofBinding &caller,
                                  VerifiedPeerProof &destination) noexcept {
    const auto original = caller;
    if (busy_ || phase_ != Phase::Verified || !(original == challenge_.original))
        return false;
    busy_ = true;
    const bool valid = !expired() && current();
    if (valid) {
        VerifiedPeerProof v;
        v.original = original;
        v.credential = credential_;
        v.sequence = challenge_.sequence;
        v.issuedAt = issuedAt_;
        v.verifiedAt = verifiedAt_;
        destination = v;
    } else
        phase_ = Phase::Failed;
    busy_ = false;
    return valid;
}
bool PeerProofOwner::readVerifiedEvidence(const PeerProofBinding &caller,
                                          AcceptedPeerProof &destination) noexcept {
    const auto original = caller;
    if (busy_ || phase_ != Phase::Verified || !(original == challenge_.original))
        return false;
    busy_ = true;
    AcceptedPeerProof copied;
    bool valid = !expired() && current() && encodePeerChallenge(challenge_, copied.transcript);
    if (valid) {
        copied.verified.original = original;
        copied.verified.credential = credential_;
        copied.verified.sequence = challenge_.sequence;
        copied.verified.issuedAt = issuedAt_;
        copied.verified.verifiedAt = verifiedAt_;
        copied.signature = accepted_.signature;
        valid = !expired() && current();
    }
    if (valid)
        destination = copied;
    else
        phase_ = Phase::Failed;
    busy_ = false;
    return valid;
}
bool bindPeerProofEvidence(const VerifiedPeerProof &caller,
                           const OriginalPeerSecurity &callerSecurity,
                           VerifiedPeerEvidence &destination) noexcept {
    const auto proof = caller;
    const auto security = callerSecurity;
    if (!proof.original.valid() || !proof.sequence || !proof.credential.peer.valid() ||
        !proof.credential.generation || !(proof.original == security.original) ||
        !security.encrypted || !security.mitm || !security.secureConnections || !security.bonded)
        return false;
    VerifiedPeerEvidence v;
    v.peer = proof.credential.peer;
    v.encrypted = security.encrypted;
    v.mitm = security.mitm;
    v.secureConnections = security.secureConnections;
    v.bonded = security.bonded;
    destination = v;
    return true;
}
} // namespace companion
} // namespace larp
