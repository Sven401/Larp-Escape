// Link-only SDK capability probe. The retained function is never called by setup().
// Transient keys would be generated only if an explicit future test invoked it.
#include <Arduino.h>
#include <esp_random.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>
#include <cstring>

#if !defined(MBEDTLS_ECDH_C) || !defined(MBEDTLS_ECDSA_C) || !defined(MBEDTLS_GCM_C)
#error "Pinned ESP32 SDK lacks required ECDH, ECDSA, or GCM support"
#endif
#if defined(MBEDTLS_HKDF_C)
#error "HKDF capability changed; review the APP_MUTUAL_V1 key schedule first"
#endif

namespace {
int espRandom(void *, unsigned char *output, size_t count) {
    esp_fill_random(output, count);
    return 0;
}

// This is a *capability* sample, not a protocol implementation or a production KDF.
// The digest, nonce, key and plaintext below are public synthetic probe inputs.
int linkedCryptoExercise() {
    constexpr unsigned char message[] = "APP_MUTUAL_V1_LINK_ONLY_PROBE";
    constexpr unsigned char aesKey[16] = {0};
    constexpr unsigned char nonce[12] = {0};
    constexpr unsigned char plaintext[16] = {0};
    unsigned char digest[32] = {};
    unsigned char ciphertext[sizeof(plaintext)] = {};
    unsigned char decrypted[sizeof(plaintext)] = {};
    unsigned char tag[16] = {};
    mbedtls_ecdsa_context signer;
    mbedtls_ecp_group group;
    mbedtls_ecp_point firstPublic, secondPublic;
    mbedtls_mpi firstSecret, secondSecret, firstShared, secondShared, r, s;
    mbedtls_gcm_context gcm;
    mbedtls_ecdsa_init(&signer);
    mbedtls_ecp_group_init(&group);
    mbedtls_ecp_point_init(&firstPublic);
    mbedtls_ecp_point_init(&secondPublic);
    mbedtls_mpi_init(&firstSecret);
    mbedtls_mpi_init(&secondSecret);
    mbedtls_mpi_init(&firstShared);
    mbedtls_mpi_init(&secondShared);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    mbedtls_gcm_init(&gcm);

    int rc = mbedtls_sha256_ret(message, sizeof(message) - 1, digest, 0);
    if (rc == 0)
        rc = mbedtls_ecdsa_genkey(&signer, MBEDTLS_ECP_DP_SECP256R1, espRandom, nullptr);
    if (rc == 0)
        rc = mbedtls_ecdsa_sign(&signer.grp, &r, &s, &signer.d, digest,
                               sizeof(digest), espRandom, nullptr);
    if (rc == 0)
        rc = mbedtls_ecdsa_verify(&signer.grp, digest, sizeof(digest),
                                 &signer.Q, &r, &s);
    if (rc == 0)
        rc = mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP256R1);
    if (rc == 0)
        rc = mbedtls_ecdh_gen_public(&group, &firstSecret, &firstPublic, espRandom, nullptr);
    if (rc == 0)
        rc = mbedtls_ecdh_gen_public(&group, &secondSecret, &secondPublic, espRandom, nullptr);
    if (rc == 0)
        rc = mbedtls_ecdh_compute_shared(&group, &firstShared, &secondPublic,
                                        &firstSecret, espRandom, nullptr);
    if (rc == 0)
        rc = mbedtls_ecdh_compute_shared(&group, &secondShared, &firstPublic,
                                        &secondSecret, espRandom, nullptr);
    if (rc == 0 && mbedtls_mpi_cmp_mpi(&firstShared, &secondShared) != 0)
        rc = -1;
    if (rc == 0)
        rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, aesKey, 128);
    if (rc == 0)
        rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                       sizeof(plaintext), nonce, sizeof(nonce),
                                       nullptr, 0, plaintext, ciphertext,
                                       sizeof(tag), tag);
    if (rc == 0)
        rc = mbedtls_gcm_auth_decrypt(&gcm, sizeof(ciphertext), nonce,
                                      sizeof(nonce), nullptr, 0, tag,
                                      sizeof(tag), ciphertext, decrypted);
    if (rc == 0 && std::memcmp(plaintext, decrypted, sizeof(plaintext)) != 0)
        rc = -1;

    mbedtls_gcm_free(&gcm);
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&secondShared);
    mbedtls_mpi_free(&firstShared);
    mbedtls_mpi_free(&secondSecret);
    mbedtls_mpi_free(&firstSecret);
    mbedtls_ecp_point_free(&secondPublic);
    mbedtls_ecp_point_free(&firstPublic);
    mbedtls_ecp_group_free(&group);
    mbedtls_ecdsa_free(&signer);
    return rc;
}

using ProbeFunction = int (*)();
volatile ProbeFunction retainedProbe = &linkedCryptoExercise;
} // namespace

void setup() {
    // Link the capability sample; deliberately do not invoke it.
    (void)retainedProbe;
}

void loop() {}
