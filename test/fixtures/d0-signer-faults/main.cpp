#include "PsaD0FactSigner.h"
#include <cstring>
#include <iostream>
#include <type_traits>
using namespace larp::companion::d0;
namespace {
unsigned checks = 0, failures = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            ++failures;                                                                            \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
        }                                                                                          \
    } while (false)
struct Backend {
    psa_status_t init = PSA_SUCCESS, attributes = PSA_SUCCESS, exporting = PSA_SUCCESS,
                 signing = PSA_SUCCESS;
    psa_key_attributes_t value = {PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1), 256,
                                  PSA_KEY_USAGE_SIGN_HASH, PSA_ALG_ECDSA(PSA_ALG_SHA_256)};
    unsigned inits = 0, reads = 0, resets = 0, exports = 0, signs = 0;
    std::size_t exportSize = 65, signSize = 64;
    std::uint8_t prefix = 4;
    CandidatePublicKey *mutateKey = nullptr;
    std::uint8_t *mutateHash = nullptr;
    bool changedKey = false;
} b;
CandidatePublicKey key;
std::uint8_t hash[32], signature[64];
void fresh() {
    b = Backend{};
    for (unsigned i = 0; i < 32; ++i) {
        key.x[i] = static_cast<std::uint8_t>(i + 1);
        key.y[i] = static_cast<std::uint8_t>(i + 33);
        hash[i] = static_cast<std::uint8_t>(i + 65);
    }
    std::memset(signature, 0xcc, 64);
}
bool zero(const void *p, std::size_t n) {
    const auto *bytes = static_cast<const std::uint8_t *>(p);
    for (std::size_t i = 0; i < n; ++i)
        if (bytes[i])
            return false;
    return true;
}
SignStatus mapped(psa_status_t s) {
    return s == PSA_ERROR_INVALID_HANDLE || s == PSA_ERROR_INVALID_ARGUMENT ||
                   s == PSA_ERROR_NOT_PERMITTED
               ? SignStatus::InvalidKey
           : s == PSA_ERROR_NOT_SUPPORTED || s == PSA_ERROR_BAD_STATE ? SignStatus::Unavailable
                                                                      : SignStatus::Failure;
}
void publicFailures() {
    fresh();
    CandidatePublicKey out = key;
    {
        PsaD0FactSigner s(0);
        CHECK(s.publicKey(out) == SignStatus::InvalidKey);
        CHECK(zero(&out, sizeof(out)));
        CHECK(!b.inits);
    }
    fresh();
    b.init = PSA_ERROR_GENERIC_ERROR;
    {
        PsaD0FactSigner s(42);
        CHECK(s.publicKey(out) == SignStatus::Unavailable);
        CHECK(!b.reads && !b.resets && !b.exports);
    }
    for (auto error :
         {PSA_ERROR_INVALID_HANDLE, PSA_ERROR_INVALID_ARGUMENT, PSA_ERROR_NOT_PERMITTED,
          PSA_ERROR_NOT_SUPPORTED, PSA_ERROR_BAD_STATE, PSA_ERROR_GENERIC_ERROR}) {
        fresh();
        b.attributes = error;
        out = key;
        PsaD0FactSigner s(42);
        CHECK(s.publicKey(out) == mapped(error));
        CHECK(b.reads == 1 && b.resets == 1 && !b.exports);
        CHECK(zero(&out, sizeof(out)));
    }
    for (unsigned mode = 0; mode < 5; ++mode) {
        fresh();
        if (mode == 0)
            b.value.type ^= 1;
        if (mode == 1)
            b.value.bits = 384;
        if (mode == 2)
            b.value.usage = 0;
        if (mode == 3)
            b.value.algorithm = PSA_ALG_SHA_256;
        if (mode == 4)
            b.value.algorithm = PSA_ALG_ECDSA(0x08);
        PsaD0FactSigner s(42);
        CHECK(s.publicKey(out) == SignStatus::InvalidKey);
        CHECK(b.resets == 1 && !b.exports && !b.signs);
        CHECK(zero(&out, sizeof(out)));
    }
    for (auto error :
         {PSA_ERROR_INVALID_HANDLE, PSA_ERROR_NOT_SUPPORTED, PSA_ERROR_GENERIC_ERROR}) {
        fresh();
        b.exporting = error;
        PsaD0FactSigner s(42);
        CHECK(s.publicKey(out) == mapped(error));
        CHECK(b.resets == 1 && b.exports == 1 && !b.signs);
        CHECK(zero(&out, sizeof(out)));
    }
    for (auto size : {std::size_t(0), std::size_t(64), std::size_t(66)}) {
        fresh();
        b.exportSize = size;
        PsaD0FactSigner s(42);
        CHECK(s.publicKey(out) == SignStatus::InvalidKey);
        CHECK(zero(&out, sizeof(out)));
    }
    fresh();
    b.prefix = 2;
    PsaD0FactSigner s(42);
    CHECK(s.publicKey(out) == SignStatus::InvalidKey);
    CHECK(zero(&out, sizeof(out)));
}
void signingFailures() {
    fresh();
    PsaD0FactSigner s(42);
    std::size_t written = 99;
    CHECK(s.signHash(key, nullptr, signature, written) == SignStatus::Failure);
    CHECK(!written && !b.inits);
    CHECK(s.signHash(key, hash, nullptr, written) == SignStatus::Failure);
    CHECK(!written && !b.inits);
    for (auto error :
         {PSA_ERROR_INVALID_HANDLE, PSA_ERROR_INVALID_ARGUMENT, PSA_ERROR_NOT_PERMITTED,
          PSA_ERROR_NOT_SUPPORTED, PSA_ERROR_BAD_STATE, PSA_ERROR_GENERIC_ERROR}) {
        fresh();
        b.signing = error;
        written = 99;
        CHECK(s.signHash(key, hash, signature, written) == mapped(error));
        CHECK(!written && zero(signature, 64));
        CHECK(b.signs == 1 && b.resets == 1);
    }
    for (auto size : {std::size_t(0), std::size_t(63), std::size_t(65)}) {
        fresh();
        b.signSize = size;
        CHECK(s.signHash(key, hash, signature, written) == SignStatus::Failure);
        CHECK(!written && zero(signature, 64));
    }
    fresh();
    b.changedKey = true;
    CHECK(s.signHash(key, hash, signature, written) == SignStatus::InvalidKey);
    CHECK(!written && !b.signs && zero(signature, 64));
    fresh();
    b.init = PSA_ERROR_GENERIC_ERROR;
    CHECK(s.signHash(key, hash, signature, written) == SignStatus::Unavailable);
    CHECK(!written && zero(signature, 64));
}
void mechanicalSuccessAndCopies() {
    // Artificial bytes only: these checks are NOT a valid-signature proof.
    for (bool anyHash : {false, true}) {
        fresh();
        if (anyHash)
            b.value.algorithm = PSA_ALG_ECDSA(PSA_ALG_ANY_HASH);
        PsaD0FactSigner s(42);
        CandidatePublicKey out;
        CHECK(s.publicKey(out) == SignStatus::Success);
        CHECK(!std::memcmp(&key, &out, sizeof(key)));
        CHECK(b.resets == 1);
        b.mutateKey = &key;
        b.mutateHash = hash;
        std::size_t written = 99;
        CHECK(s.signHash(key, hash, signature, written) == SignStatus::Success);
        CHECK(written == 64 && b.signs == 1);
        CHECK(signature[0] == 0xa5 && signature[63] == 0xa5);
        CHECK(zero(&key, sizeof(key)) && zero(hash, 32));
    }
    fresh();
    PsaD0FactSigner s(42);
    std::uint8_t shared[64] = {};
    std::memcpy(shared, hash, 32);
    std::size_t written = 99;
    CHECK(s.signHash(key, shared, shared, written) == SignStatus::Success);
    CHECK(written == 64);
}
} // namespace
psa_status_t psa_crypto_init() {
    ++b.inits;
    if (b.mutateKey)
        *b.mutateKey = CandidatePublicKey{};
    if (b.mutateHash)
        std::memset(b.mutateHash, 0, 32);
    return b.init;
}
psa_status_t psa_get_key_attributes(mbedtls_svc_key_id_t id, psa_key_attributes_t *a) {
    CHECK(id == 42);
    ++b.reads;
    *a = b.value;
    return b.attributes;
}
void psa_reset_key_attributes(psa_key_attributes_t *a) {
    ++b.resets;
    *a = psa_key_attributes_t{};
}
psa_status_t psa_export_public_key(mbedtls_svc_key_id_t id, std::uint8_t *out, std::size_t capacity,
                                   std::size_t *written) {
    CHECK(id == 42 && capacity == 65);
    ++b.exports;
    out[0] = b.prefix;
    for (unsigned i = 1; i < 65; ++i)
        out[i] = static_cast<std::uint8_t>(i + (b.changedKey ? 1 : 0));
    *written = b.exportSize;
    return b.exporting;
}
psa_status_t psa_sign_hash(mbedtls_svc_key_id_t id, psa_algorithm_t alg, const std::uint8_t *input,
                           std::size_t size, std::uint8_t *out, std::size_t capacity,
                           std::size_t *written) {
    CHECK(id == 42 && alg == PSA_ALG_ECDSA(PSA_ALG_SHA_256) && size == 32 && capacity == 64);
    ++b.signs;
    for (unsigned i = 0; i < 32; ++i)
        CHECK(input[i] == static_cast<std::uint8_t>(i + 65));
    std::memset(out, 0xa5, 64);
    *written = b.signSize;
    return b.signing;
}
int main() {
    static_assert(!std::is_copy_constructible<PsaD0FactSigner>::value, "borrow must not copy");
    static_assert(!std::is_move_constructible<PsaD0FactSigner>::value, "borrow must not move");
    static_assert(!std::is_copy_assignable<PsaD0FactSigner>::value, "borrow must not assign");
    static_assert(!std::is_move_assignable<PsaD0FactSigner>::value, "borrow must not move assign");
    publicFailures();
    signingFailures();
    mechanicalSuccessAndCopies();
    std::cout << checks << " signer API/lifecycle checks; " << failures
              << " failures (no cryptographic qualification)\n";
    return failures ? 1 : 0;
}
