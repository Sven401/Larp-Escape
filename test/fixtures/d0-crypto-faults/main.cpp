#include "PsaD0FactCrypto.h"
#include <cstring>
#include <iostream>
#include <type_traits>
using namespace larp::companion::d0;
namespace {
unsigned checks = 0, failures = 0;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            ++failures;                                                                            \
            std::cerr << __LINE__ << ": " #value "\n";                                             \
        }                                                                                          \
    } while (false)
struct Backend {
    psa_status_t initialize = PSA_SUCCESS, import = PSA_SUCCESS, verify = PSA_SUCCESS;
    unsigned initCalls = 0, imports = 0, verifies = 0, destroys = 0, resets = 0;
    bool failDestroy = false;
    mbedtls_svc_key_id_t live = 0;
    CandidatePublicKey *mutateKey = nullptr;
    std::uint8_t *mutateHash = nullptr, *mutateSignature = nullptr;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    std::uint8_t key[65] = {}, hash[32] = {}, signature[64] = {};
} backend;
CandidatePublicKey key;
std::uint8_t hash[32], signature[64];
void fresh() {
    backend = Backend{};
    for (unsigned i = 0; i < 32; ++i) {
        key.x[i] = static_cast<std::uint8_t>(i + 1);
        key.y[i] = static_cast<std::uint8_t>(i + 33);
        hash[i] = static_cast<std::uint8_t>(i + 65);
    }
    std::memset(signature, 0xfe, sizeof(signature));
}
void rejectionPaths() {
    fresh();
    {
        PsaD0FactCrypto c;
        backend.initialize = PSA_ERROR_GENERIC_ERROR;
        CHECK(c.verifyHash(key, hash, signature) == VerifyResult::Unavailable);
        CHECK(backend.imports == 0 && backend.verifies == 0 && backend.live == 0);
    }
    for (psa_status_t status :
         {PSA_ERROR_INVALID_ARGUMENT, PSA_ERROR_NOT_SUPPORTED, PSA_ERROR_GENERIC_ERROR}) {
        fresh();
        PsaD0FactCrypto c;
        backend.import = status;
        const VerifyResult expected =
            status == PSA_ERROR_INVALID_ARGUMENT ? VerifyResult::InvalidKey
            : status == PSA_ERROR_NOT_SUPPORTED  ? VerifyResult::Unavailable
                                                 : VerifyResult::Failure;
        CHECK(c.verifyHash(key, hash, signature) == expected);
        CHECK(backend.imports == 1 && backend.resets == 1 && backend.verifies == 0 &&
              backend.live == 0);
    }
    for (psa_status_t status : {PSA_ERROR_INVALID_SIGNATURE, PSA_ERROR_INVALID_ARGUMENT,
                                PSA_ERROR_NOT_SUPPORTED, PSA_ERROR_GENERIC_ERROR}) {
        fresh();
        PsaD0FactCrypto c;
        backend.verify = status;
        const VerifyResult expected =
            status == PSA_ERROR_INVALID_SIGNATURE  ? VerifyResult::InvalidSignature
            : status == PSA_ERROR_INVALID_ARGUMENT ? VerifyResult::InvalidKey
            : status == PSA_ERROR_NOT_SUPPORTED    ? VerifyResult::Unavailable
                                                   : VerifyResult::Failure;
        CHECK(c.verifyHash(key, hash, signature) == expected);
        CHECK(backend.imports == 1 && backend.verifies == 1 && backend.destroys == 1 &&
              backend.live == 0);
        CHECK(c.reset());
        CHECK(backend.destroys == 1);
    }
    fresh();
    {
        PsaD0FactCrypto c;
        CHECK(c.verifyHash(key, nullptr, signature) == VerifyResult::Failure);
        CHECK(c.verifyHash(key, hash, nullptr) == VerifyResult::Failure);
        CHECK(backend.initCalls == 0 && backend.imports == 0 && backend.verifies == 0);
    }
}
void uncertainCleanup() {
    fresh();
    {
        PsaD0FactCrypto c;
        backend.failDestroy = true;
        CHECK(c.verifyHash(key, hash, signature) == VerifyResult::CleanupFailure);
        CHECK(backend.live == 42 && backend.imports == 1 && backend.verifies == 1);
        CHECK(!c.reset());
        CHECK(backend.live == 42);
        CHECK(c.verifyHash(key, hash, signature) == VerifyResult::CleanupFailure);
        CHECK(backend.imports == 1 && backend.verifies == 1 && backend.live == 42);
        backend.failDestroy = false;
        CHECK(c.reset());
        CHECK(backend.live == 0);
        CHECK(c.verifyHash(key, hash, signature) == VerifyResult::Valid);
        CHECK(backend.imports == 2 && backend.verifies == 2 && backend.live == 0);
    }
    fresh();
    {
        PsaD0FactCrypto c;
        backend.verify = PSA_ERROR_INVALID_SIGNATURE;
        backend.failDestroy = true;
        CHECK(c.verifyHash(key, hash, signature) == VerifyResult::CleanupFailure);
        CHECK(backend.live == 42);
        backend.failDestroy = false;
    }
    CHECK(backend.live == 0 && backend.destroys == 2);
}
void frozenBuffersAndAttributes() {
    fresh();
    CandidatePublicKey expectedKey = key;
    std::uint8_t expectedHash[32], expectedSignature[64];
    std::memcpy(expectedHash, hash, 32);
    std::memcpy(expectedSignature, signature, 64);
    backend.mutateKey = &key;
    backend.mutateHash = hash;
    backend.mutateSignature = signature;
    {
        PsaD0FactCrypto c;
        CHECK(c.verifyHash(key, hash, signature) == VerifyResult::Valid);
    }
    CHECK(backend.key[0] == 4 && !std::memcmp(backend.key + 1, expectedKey.x, 32) &&
          !std::memcmp(backend.key + 33, expectedKey.y, 32));
    CHECK(!std::memcmp(backend.hash, expectedHash, 32) &&
          !std::memcmp(backend.signature, expectedSignature, 64));
    CHECK(backend.attributes.type == PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    CHECK(backend.attributes.bits == 256 && backend.attributes.usage == PSA_KEY_USAGE_VERIFY_HASH);
    CHECK(backend.attributes.algorithm == PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    CHECK(backend.resets == 1 && backend.live == 0);
}
} // namespace
psa_status_t psa_crypto_init() {
    ++backend.initCalls;
    if (backend.mutateKey)
        *backend.mutateKey = CandidatePublicKey{};
    if (backend.mutateHash)
        std::memset(backend.mutateHash, 0, 32);
    if (backend.mutateSignature)
        std::memset(backend.mutateSignature, 0, 64);
    return backend.initialize;
}
psa_status_t psa_import_key(const psa_key_attributes_t *attributes, const std::uint8_t *bytes,
                            std::size_t size, mbedtls_svc_key_id_t *handle) {
    ++backend.imports;
    CHECK(backend.live == 0);
    CHECK(size == 65);
    backend.attributes = *attributes;
    if (size == 65)
        std::memcpy(backend.key, bytes, size);
    if (backend.import != PSA_SUCCESS)
        return backend.import;
    backend.live = *handle = 42;
    return PSA_SUCCESS;
}
void psa_reset_key_attributes(psa_key_attributes_t *attributes) {
    ++backend.resets;
    *attributes = psa_key_attributes_t{};
}
psa_status_t psa_verify_hash(mbedtls_svc_key_id_t handle, psa_algorithm_t algorithm,
                             const std::uint8_t *digest, std::size_t digestSize,
                             const std::uint8_t *sig, std::size_t signatureSize) {
    ++backend.verifies;
    CHECK(handle == backend.live && handle != 0);
    CHECK(algorithm == PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    CHECK(digestSize == 32 && signatureSize == 64);
    if (digestSize == 32)
        std::memcpy(backend.hash, digest, digestSize);
    if (signatureSize == 64)
        std::memcpy(backend.signature, sig, signatureSize);
    return backend.verify;
}
psa_status_t psa_destroy_key(mbedtls_svc_key_id_t handle) {
    ++backend.destroys;
    CHECK(handle == backend.live && handle != 0);
    if (backend.failDestroy)
        return PSA_ERROR_GENERIC_ERROR;
    backend.live = 0;
    return PSA_SUCCESS;
}
int main() {
    static_assert(!std::is_copy_constructible<PsaD0FactCrypto>::value, "key owner cannot copy");
    static_assert(!std::is_move_constructible<PsaD0FactCrypto>::value, "key owner cannot move");
    rejectionPaths();
    uncertainCleanup();
    frozenBuffersAndAttributes();
    std::cout << checks << " PSA lifecycle checks, " << failures
              << " failures (not a crypto proof)\n";
    return failures ? 1 : 0;
}
