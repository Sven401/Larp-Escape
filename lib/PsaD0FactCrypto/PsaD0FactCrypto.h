#pragma once
#include "Companion/D0Fact.h"
#include <psa/crypto.h>
namespace larp {
namespace companion {
namespace d0 {
// Public PSA candidate-key verifier, with no installed trust or role policy.
// Exclusive synchronous owner; failed destruction retains the handle for retry.
class PsaD0FactCrypto final : public IFactCrypto {
  public:
    PsaD0FactCrypto() = default;
    ~PsaD0FactCrypto() override;
    PsaD0FactCrypto(const PsaD0FactCrypto &) = delete;
    PsaD0FactCrypto &operator=(const PsaD0FactCrypto &) = delete;
    PsaD0FactCrypto(PsaD0FactCrypto &&) = delete;
    PsaD0FactCrypto &operator=(PsaD0FactCrypto &&) = delete;
    bool reset() override;
    VerifyResult verifyHash(const CandidatePublicKey &, const std::uint8_t hash[32],
                            const std::uint8_t signature[64]) override;

  private:
    mbedtls_svc_key_id_t key_ = 0;
};
} // namespace d0
} // namespace companion
} // namespace larp
