#pragma once
#include <cstddef>
#include <cstdint>

// Negative lifecycle seam only. No crypto implementation or SDK substitute.
using psa_status_t = std::int32_t;
using mbedtls_svc_key_id_t = std::uint32_t;
using psa_algorithm_t = std::uint32_t;
struct psa_key_attributes_t {
    std::uint32_t type, bits, usage, algorithm;
};
#define PSA_KEY_ATTRIBUTES_INIT {0, 0, 0, 0}
#define PSA_SUCCESS 0
#define PSA_ERROR_INVALID_ARGUMENT (-135)
#define PSA_ERROR_INVALID_SIGNATURE (-149)
#define PSA_ERROR_NOT_SUPPORTED (-134)
#define PSA_ERROR_GENERIC_ERROR (-132)
#define PSA_ECC_FAMILY_SECP_R1 0x12u
#define PSA_KEY_TYPE_ECC_PUBLIC_KEY(family) (0x4100u | (family))
#define PSA_KEY_USAGE_VERIFY_HASH 0x2000u
#define PSA_ALG_SHA_256 0x02000009u
#define PSA_ALG_ECDSA(hash) (0x06000600u | ((hash) & 0xffu))

inline void psa_set_key_type(psa_key_attributes_t *a, std::uint32_t v) {
    a->type = v;
}
inline void psa_set_key_bits(psa_key_attributes_t *a, std::size_t v) {
    a->bits = static_cast<std::uint32_t>(v);
}
inline void psa_set_key_usage_flags(psa_key_attributes_t *a, std::uint32_t v) {
    a->usage = v;
}
inline void psa_set_key_algorithm(psa_key_attributes_t *a, psa_algorithm_t v) {
    a->algorithm = v;
}
psa_status_t psa_crypto_init();
psa_status_t psa_import_key(const psa_key_attributes_t *, const std::uint8_t *, std::size_t,
                            mbedtls_svc_key_id_t *);
void psa_reset_key_attributes(psa_key_attributes_t *);
psa_status_t psa_verify_hash(mbedtls_svc_key_id_t, psa_algorithm_t, const std::uint8_t *,
                             std::size_t, const std::uint8_t *, std::size_t);
psa_status_t psa_destroy_key(mbedtls_svc_key_id_t);
