#include "PsaPackageDigest.h"
#include <cstring>

namespace larp {
namespace configuration {
PsaPackageDigest::~PsaPackageDigest() {
    abort();
}
bool PsaPackageDigest::abort() {
    const bool closed = psa_hash_abort(&operation_) == PSA_SUCCESS;
    if (closed) {
        active_ = false;
        failed_ = false;
    } else
        failed_ = true;
    return closed;
}
bool PsaPackageDigest::begin() {
    if (!abort() || psa_crypto_init() != PSA_SUCCESS)
        return false;
    if (psa_hash_setup(&operation_, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        failed_ = true;
        return false;
    }
    active_ = true;
    return true;
}
bool PsaPackageDigest::update(const std::uint8_t *bytes, std::size_t count) {
    if (!active_ || failed_ || (!bytes && count)) {
        failed_ = true;
        return false;
    }
    if (psa_hash_update(&operation_, bytes, count) != PSA_SUCCESS) {
        failed_ = true;
        return false;
    }
    return true;
}
bool PsaPackageDigest::finish(std::uint8_t digest[32], std::size_t &written) {
    written = 0;
    if (digest)
        std::memset(digest, 0, 32);
    if (!active_ || failed_ || !digest) {
        failed_ = true;
        return false;
    }
    std::size_t count = 0;
    const bool complete =
        psa_hash_finish(&operation_, digest, 32, &count) == PSA_SUCCESS && count == 32;
    active_ = false;
    if (!complete) {
        std::memset(digest, 0, 32);
        failed_ = true;
        return false;
    }
    written = count;
    return true;
}
} // namespace configuration
} // namespace larp
