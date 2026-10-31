#pragma once
#include "GameConfiguration/PackageModules.h"
#include <psa/crypto.h>

namespace larp {
namespace configuration {
// Public PSA multipart SHA-256, supplied by the qualified native backend.
// One exclusive synchronous owner; no copy of an active PSA operation.
class PsaPackageDigest final : public IPackageDigest {
  public:
    PsaPackageDigest() = default;
    ~PsaPackageDigest() override;
    PsaPackageDigest(const PsaPackageDigest &) = delete;
    PsaPackageDigest &operator=(const PsaPackageDigest &) = delete;
    PsaPackageDigest(PsaPackageDigest &&) = delete;
    PsaPackageDigest &operator=(PsaPackageDigest &&) = delete;
    bool begin() override;
    bool update(const std::uint8_t *bytes, std::size_t count) override;
    bool finish(std::uint8_t digest[32], std::size_t &written) override;
    bool abort() override;

  private:
    psa_hash_operation_t operation_ = PSA_HASH_OPERATION_INIT;
    bool active_ = false, failed_ = false;
};
} // namespace configuration
} // namespace larp
