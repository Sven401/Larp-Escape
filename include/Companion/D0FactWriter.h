#pragma once
#include "Companion/D0Fact.h"

namespace larp {
namespace companion {
namespace d0 {
enum class SignStatus : std::uint8_t { Success, InvalidKey, Unavailable, Failure };
class IFactSigner {
  public:
    virtual ~IFactSigner() = default;
    // The configured local key is exclusively held stable for the whole write.
    // These operations confer no origin/GM rights and never publish anything.
    virtual SignStatus publicKey(CandidatePublicKey &destination) = 0;
    virtual SignStatus signHash(const CandidatePublicKey &expected, const std::uint8_t hash[32],
                                std::uint8_t signature[64], std::size_t &written) = 0;
};
struct FactWriteWorkspace {
    std::uint8_t bytes[2048] = {};
};
enum class WriteScope : std::uint8_t { Rejected, SignedUnpublished };
enum class WriteError : std::uint8_t {
    None,
    InvalidFact,
    LengthLimit,
    InvalidKey,
    SignerUnavailable,
    SigningFailure,
    SignatureLength,
    HashFailure,
    VerificationFailure,
    CleanupFailure
};
struct WriteResult {
    WriteScope scope = WriteScope::Rejected;
    WriteError error = WriteError::None;
    Result verification;
    // Original key copy used to verify these exact bytes. Preserve alongside
    // historical outbox records; this is not a current trust/role assertion.
    CandidatePublicKey publicKey;
};
// Exclusive synchronous worker call; crypto may allocate/block. Never call from
// ISR/radio callbacks or under a short owner lock. Copy input before callbacks or output reset.
// Workspace and destination must not alias; callbacks must not mutate/retain
// input, workspace, destination or transient buffers. Input may be destination.fact.
// The completed bytes are checked by the actual decoder and verifier against
// the originally exported public-key COPY, not a later lookup of the same key ID.
// SignedUnpublished does not assert installed trust, durable commit, authority,
// local loopback admission or permission to publish. Retry after a pending commit
// uses the already frozen result, never a new randomized signature.
WriteResult writeFact(const Fact &input, configuration::IPackageDigest &hash, IFactSigner &signer,
                      IFactCrypto &verifier, FactWriteWorkspace &workspace,
                      FrozenFact &destination);
} // namespace d0
} // namespace companion
} // namespace larp
