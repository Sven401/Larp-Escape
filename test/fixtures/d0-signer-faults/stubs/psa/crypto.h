#pragma once
#include <cstddef>
#include <cstdint>
// API/lifecycle seam only; implements no cryptography.
using psa_status_t = std::int32_t;
using mbedtls_svc_key_id_t = std::uint32_t;
using psa_algorithm_t = std::uint32_t;
struct psa_key_attributes_t {
    std::uint32_t type, bits, usage, algorithm;
};
#define PSA_KEY_ATTRIBUTES_INIT {0, 0, 0, 0}
#define PSA_SUCCESS 0
#define PSA_ERROR_INVALID_HANDLE (-136)
#define PSA_ERROR_INVALID_ARGUMENT (-135)
#define PSA_ERROR_NOT_PERMITTED (-133)
#define PSA_ERROR_NOT_SUPPORTED (-134)
#define PSA_ERROR_BAD_STATE (-137)
#define PSA_ERROR_GENERIC_ERROR (-132)
#define PSA_ECC_FAMILY_SECP_R1 0x12u
#define PSA_KEY_TYPE_ECC_KEY_PAIR(family) (0x7100u | (family))
#define PSA_KEY_USAGE_SIGN_HASH 0x1000u
#define PSA_ALG_SHA_256 0x02000009u
#define PSA_ALG_ANY_HASH 0x020000ffu
#define PSA_ALG_ECDSA(hash) (0x06000600u | ((hash) & 0xffu))
inline std::uint32_t psa_get_key_type(const psa_key_attributes_t *a) {
    return a->type;
}
inline std::size_t psa_get_key_bits(const psa_key_attributes_t *a) {
    return a->bits;
}
inline std::uint32_t psa_get_key_usage_flags(const psa_key_attributes_t *a) {
    return a->usage;
}
inline psa_algorithm_t psa_get_key_algorithm(const psa_key_attributes_t *a) {
    return a->algorithm;
}
psa_status_t psa_crypto_init();
psa_status_t psa_get_key_attributes(mbedtls_svc_key_id_t, psa_key_attributes_t *);
void psa_reset_key_attributes(psa_key_attributes_t *);
psa_status_t psa_export_public_key(mbedtls_svc_key_id_t, std::uint8_t *, std::size_t,
                                   std::size_t *);
psa_status_t psa_sign_hash(mbedtls_svc_key_id_t, psa_algorithm_t, const std::uint8_t *, std::size_t,
                           std::uint8_t *, std::size_t, std::size_t *);
// Deliberately no key import/generation/destruction/private-export declarations.
