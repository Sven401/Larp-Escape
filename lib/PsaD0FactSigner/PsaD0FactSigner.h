#pragma once
#include "Companion/D0FactWriter.h"
#include <psa/crypto.h>

namespace larp {
namespace companion {
namespace d0 {
// Borrows an explicitly provisioned local origin key. It never generates,
// imports, exports private bytes, persists, replaces, or destroys that key.
// The trusted key owner must retain exclusive stable ownership throughout writeFact.
// The writer MUST verify the actual signature against the original public-key copy.
class PsaD0FactSigner final : public IFactSigner {
  public:
    explicit PsaD0FactSigner(mbedtls_svc_key_id_t key) : key_(key) {}
    PsaD0FactSigner(const PsaD0FactSigner &) = delete;
    PsaD0FactSigner &operator=(const PsaD0FactSigner &) = delete;
    PsaD0FactSigner(PsaD0FactSigner &&) = delete;
    PsaD0FactSigner &operator=(PsaD0FactSigner &&) = delete;
    SignStatus publicKey(CandidatePublicKey &destination) override;
    SignStatus signHash(const CandidatePublicKey &expected, const std::uint8_t hash[32],
                        std::uint8_t signature[64], std::size_t &written) override;

  private:
    const mbedtls_svc_key_id_t key_;
};
} // namespace d0
} // namespace companion
} // namespace larp
