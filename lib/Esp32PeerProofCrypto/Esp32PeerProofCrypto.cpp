#include "Esp32PeerProofCrypto.h"
#include <algorithm>
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
static_assert(MBEDTLS_VERSION_NUMBER == 0x021C0400, "Re-audit a different Mbed TLS backend");
namespace larp {
namespace companion {
namespace {
struct Public {
    mbedtls_ecp_group group;
    mbedtls_ecp_point point;
    Public() {
        mbedtls_ecp_group_init(&group);
        mbedtls_ecp_point_init(&point);
    }
    ~Public() {
        mbedtls_ecp_point_free(&point);
        mbedtls_ecp_group_free(&group);
    }
    bool load(const PeerPublicKey &key) {
        return key[0] == 4 && mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
               mbedtls_ecp_point_read_binary(&group, &point, key.data(), key.size()) == 0 &&
               mbedtls_ecp_check_pubkey(&group, &point) == 0;
    }
};
struct Scalar {
    mbedtls_mpi value;
    Scalar() {
        mbedtls_mpi_init(&value);
    }
    ~Scalar() {
        mbedtls_mpi_free(&value);
    }
};
// Canonical unsigned, fixed 32-byte big-endian r,s; reject high-S malleability.
const std::uint8_t order[32] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17,
                                0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51};
const std::uint8_t half[32] = {0x7f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff,
                               0xff, 0xff, 0xff, 0xff, 0xff, 0xde, 0x73, 0x7d, 0x56, 0xd3, 0x8b,
                               0xcf, 0x42, 0x79, 0xdc, 0xe5, 0x61, 0x7e, 0x31, 0x92, 0xa8};
bool positive(const std::uint8_t *p) {
    for (unsigned i = 0; i < 32; ++i)
        if (p[i])
            return true;
    return false;
}
bool canonical(const PeerSignature &signature) {
    const auto *r = signature.data(), *s = r + 32;
    return positive(r) && positive(s) &&
           std::lexicographical_compare(r, r + 32, order, order + 32) &&
           !std::lexicographical_compare(half, half + 32, s, s + 32);
}
} // namespace
bool Esp32PeerProofCrypto::validKey(const PeerPublicKey &key) noexcept {
    const auto owned = key;
    Public p;
    return p.load(owned);
}
bool Esp32PeerProofCrypto::verify(const PeerPublicKey &callerKey,
                                  const PeerTranscript &callerTranscript,
                                  const PeerSignature &callerSignature) noexcept {
    // Freeze before library allocation/crypto; callbacks can reuse their arrays.
    const auto key = callerKey;
    const auto transcript = callerTranscript;
    const auto signature = callerSignature;
    if (!canonical(signature))
        return false;
    Public p;
    Scalar r, s;
    std::array<unsigned char, 32> digest;
    return p.load(key) &&
           mbedtls_sha256_ret(transcript.data(), transcript.size(), digest.data(), 0) == 0 &&
           mbedtls_mpi_read_binary(&r.value, signature.data(), 32) == 0 &&
           mbedtls_mpi_read_binary(&s.value, signature.data() + 32, 32) == 0 &&
           mbedtls_ecdsa_verify(&p.group, digest.data(), digest.size(), &p.point, &r.value,
                                &s.value) == 0;
}
} // namespace companion
} // namespace larp
