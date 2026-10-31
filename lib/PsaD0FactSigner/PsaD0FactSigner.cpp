#include "PsaD0FactSigner.h"
#include <cstring>

namespace larp {
namespace companion {
namespace d0 {
namespace {
SignStatus status(psa_status_t value) {
    return value == PSA_SUCCESS ? SignStatus::Success
           : value == PSA_ERROR_INVALID_HANDLE || value == PSA_ERROR_INVALID_ARGUMENT ||
                   value == PSA_ERROR_NOT_PERMITTED
               ? SignStatus::InvalidKey
           : value == PSA_ERROR_NOT_SUPPORTED || value == PSA_ERROR_BAD_STATE
               ? SignStatus::Unavailable
               : SignStatus::Failure;
}
} // namespace
SignStatus PsaD0FactSigner::publicKey(CandidatePublicKey &destination) {
    destination = CandidatePublicKey{};
    if (key_ == 0)
        return SignStatus::InvalidKey;
    if (psa_crypto_init() != PSA_SUCCESS)
        return SignStatus::Unavailable;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    const auto read = psa_get_key_attributes(key_, &attributes);
    const bool compatible =
        read == PSA_SUCCESS &&
        psa_get_key_type(&attributes) == PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1) &&
        psa_get_key_bits(&attributes) == 256 &&
        (psa_get_key_usage_flags(&attributes) & PSA_KEY_USAGE_SIGN_HASH) != 0 &&
        (psa_get_key_algorithm(&attributes) == PSA_ALG_ECDSA(PSA_ALG_SHA_256) ||
         psa_get_key_algorithm(&attributes) == PSA_ALG_ECDSA(PSA_ALG_ANY_HASH));
    psa_reset_key_attributes(&attributes);
    if (read != PSA_SUCCESS)
        return status(read);
    if (!compatible)
        return SignStatus::InvalidKey;
    std::uint8_t point[65] = {};
    std::size_t written = 0;
    const auto exported = psa_export_public_key(key_, point, sizeof(point), &written);
    if (exported != PSA_SUCCESS)
        return status(exported);
    if (written != sizeof(point) || point[0] != 4)
        return SignStatus::InvalidKey;
    std::memcpy(destination.x, point + 1, 32);
    std::memcpy(destination.y, point + 33, 32);
    return SignStatus::Success;
}
SignStatus PsaD0FactSigner::signHash(const CandidatePublicKey &expected,
                                     const std::uint8_t hash[32], std::uint8_t signature[64],
                                     std::size_t &written) {
    const CandidatePublicKey key = expected;
    std::uint8_t copiedHash[32] = {};
    written = 0;
    if (!hash || !signature)
        return SignStatus::Failure;
    std::memcpy(copiedHash, hash, 32);
    std::memset(signature, 0, 64);
    CandidatePublicKey current;
    const auto checked = publicKey(current);
    if (checked != SignStatus::Success)
        return checked;
    if (std::memcmp(key.x, current.x, 32) || std::memcmp(key.y, current.y, 32))
        return SignStatus::InvalidKey;
    std::size_t count = 0;
    const auto signedHash = psa_sign_hash(key_, PSA_ALG_ECDSA(PSA_ALG_SHA_256), copiedHash,
                                          sizeof(copiedHash), signature, 64, &count);
    std::memset(copiedHash, 0, sizeof(copiedHash));
    if (signedHash != PSA_SUCCESS || count != 64) {
        std::memset(signature, 0, 64);
        return signedHash == PSA_SUCCESS ? SignStatus::Failure : status(signedHash);
    }
    written = count;
    return SignStatus::Success;
}
} // namespace d0
} // namespace companion
} // namespace larp
