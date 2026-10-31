#include "PsaD0FactCrypto.h"
#include <cstring>
namespace larp {
namespace companion {
namespace d0 {
PsaD0FactCrypto::~PsaD0FactCrypto() {
    reset();
}
bool PsaD0FactCrypto::reset() {
    if (key_ == 0)
        return true;
    if (psa_destroy_key(key_) != PSA_SUCCESS)
        return false;
    key_ = 0;
    return true;
}
VerifyResult PsaD0FactCrypto::verifyHash(const CandidatePublicKey &candidate,
                                         const std::uint8_t hash[32],
                                         const std::uint8_t signature[64]) {
    const CandidatePublicKey copied = candidate;
    std::uint8_t copiedHash[32] = {}, copiedSignature[64] = {};
    if (!hash || !signature)
        return VerifyResult::Failure;
    std::memcpy(copiedHash, hash, 32);
    std::memcpy(copiedSignature, signature, 64);
    if (!reset())
        return VerifyResult::CleanupFailure;
    if (psa_crypto_init() != PSA_SUCCESS)
        return VerifyResult::Unavailable;
    std::uint8_t publicKey[65] = {4};
    std::memcpy(publicKey + 1, copied.x, 32);
    std::memcpy(publicKey + 33, copied.y, 32);
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, 256);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    const auto imported = psa_import_key(&attributes, publicKey, sizeof(publicKey), &key_);
    psa_reset_key_attributes(&attributes);
    VerifyResult result = VerifyResult::Failure;
    if (imported == PSA_SUCCESS) {
        const auto verified =
            psa_verify_hash(key_, PSA_ALG_ECDSA(PSA_ALG_SHA_256), copiedHash, sizeof(copiedHash),
                            copiedSignature, sizeof(copiedSignature));
        result = verified == PSA_SUCCESS                   ? VerifyResult::Valid
                 : verified == PSA_ERROR_INVALID_SIGNATURE ? VerifyResult::InvalidSignature
                 : verified == PSA_ERROR_INVALID_ARGUMENT  ? VerifyResult::InvalidKey
                 : verified == PSA_ERROR_NOT_SUPPORTED     ? VerifyResult::Unavailable
                                                           : VerifyResult::Failure;
    } else
        result = imported == PSA_ERROR_INVALID_ARGUMENT ? VerifyResult::InvalidKey
                 : imported == PSA_ERROR_NOT_SUPPORTED  ? VerifyResult::Unavailable
                                                        : VerifyResult::Failure;
    if (!reset())
        result = VerifyResult::CleanupFailure;
    return result;
}
} // namespace d0
} // namespace companion
} // namespace larp
