#pragma once
#include "GameConfiguration/PackageModules.h"

namespace larp {
namespace companion {
namespace d0 {
struct Identity {
    std::uint8_t bytes[16] = {};
};
struct Digest {
    std::uint8_t bytes[32] = {};
};
struct CandidatePublicKey {
    std::uint8_t x[32] = {}, y[32] = {};
};
struct FactId {
    Identity event, origin, epoch;
    std::uint64_t sequence = 0;
};
struct Range {
    std::uint64_t first = 0, last = 0;
};
struct Fact {
    Identity event, run, origin, station, epoch, subject;
    std::uint64_t generation = 0, sequence = 0;
    Digest binding, package;
    std::uint8_t type = 0, dependencyCount = 0, rangeCount = 0;
    FactId dependencies[8];
    Range ranges[8];
};
struct Slice {
    std::uint16_t offset = 0, length = 0;
};
// Caller-owned, exclusive inactive storage. Only a successful check publishes
// this value; all failures clear it, including previously successful results.
struct FrozenFact {
    std::uint8_t original[2048] = {};
    std::uint16_t length = 0;
    Slice protectedHeader, payload, signature;
    Digest kid, payloadDigest;
    Fact fact;
};
enum class VerifyResult : std::uint8_t {
    Valid,
    InvalidSignature,
    InvalidKey,
    Unavailable,
    Failure,
    CleanupFailure
};
class IFactCrypto {
  public:
    virtual ~IFactCrypto() = default;
    // Cleanup is retryable after failure. Never silently forget an owned key.
    virtual bool reset() = 0;
    virtual VerifyResult verifyHash(const CandidatePublicKey &, const std::uint8_t hash[32],
                                    const std::uint8_t signature[64]) = 0;
};
enum class Scope : std::uint8_t { Rejected, SignatureCheckedUnauthenticated };
enum class Error : std::uint8_t {
    None,
    SourceFailure,
    LengthLimit,
    Cbor,
    Header,
    Version,
    UnsupportedKind,
    Value,
    DuplicateDependency,
    SelfDependency,
    KeyIdMismatch,
    HashFailure,
    InvalidKey,
    InvalidSignature,
    CryptoUnavailable,
    CryptoFailure,
    CleanupFailure
};
struct Result {
    Scope scope = Scope::Rejected;
    Error error = Error::None;
    std::size_t offset = 0, readBytes = 0;
};
// Synchronous and non-reentrant. Source, hash, crypto and destination are exclusive
// for the call. Callbacks must not mutate/retain buffers or the destination. Source
// storage must not alias destination. Candidate key is copied before callbacks.
// Success checks format, candidate-key identity and signature ONLY: no installed
// trust, roles, context admission, dependency availability or replay check.
Result checkFact(configuration::IPackageByteSource &source, const CandidatePublicKey &key,
                 configuration::IPackageDigest &hash, IFactCrypto &crypto, FrozenFact &destination);
} // namespace d0
} // namespace companion
} // namespace larp
