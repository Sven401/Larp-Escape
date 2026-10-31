#pragma once
// Real synthetic-key fixture signer, preserved from the root writer audit.
#include "../d0-fact-review/Support.h"
#include "Companion/D0FactWriter.h"
#include <list>
#include <memory>
#ifdef D0_FACT_REVIEW_PSA
#include "PsaD0FactSigner.h"
#else
class CngSigner final : public d0::IFactSigner {
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_KEY_HANDLE key_ = nullptr;
    d0::CandidatePublicKey public_;

  public:
    CngSigner() {
        const auto x = unhex("6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296");
        const auto y = unhex("4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5");
        std::memcpy(public_.x, x.data(), 32);
        std::memcpy(public_.y, y.data(), 32);
        std::uint8_t blob[sizeof(BCRYPT_ECCKEY_BLOB) + 96] = {};
        BCRYPT_ECCKEY_BLOB header;
        header.dwMagic = BCRYPT_ECDSA_PRIVATE_P256_MAGIC;
        header.cbKey = 32;
        std::memcpy(blob, &header, sizeof(header));
        std::memcpy(blob + sizeof(header), public_.x, 32);
        std::memcpy(blob + sizeof(header) + 32, public_.y, 32);
        blob[sizeof(blob) - 1] = 1; // Public synthetic private scalar, never a deployment key.
        if (BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) >= 0)
            BCryptImportKeyPair(algorithm_, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key_, blob,
                                sizeof(blob), 0);
    }
    ~CngSigner() {
        if (key_)
            BCryptDestroyKey(key_);
        if (algorithm_)
            BCryptCloseAlgorithmProvider(algorithm_, 0);
    }
    d0::SignStatus publicKey(d0::CandidatePublicKey &out) override {
        out = public_;
        return key_ ? d0::SignStatus::Success : d0::SignStatus::Unavailable;
    }
    d0::SignStatus signHash(const d0::CandidatePublicKey &expected, const std::uint8_t hash[32],
                            std::uint8_t signature[64], std::size_t &written) override {
        written = 0;
        if (!key_ || std::memcmp(expected.x, public_.x, 32) ||
            std::memcmp(expected.y, public_.y, 32))
            return d0::SignStatus::InvalidKey;
        ULONG count = 0;
        if (BCryptSignHash(key_, nullptr, const_cast<PUCHAR>(hash), 32, signature, 64, &count, 0) <
            0)
            return d0::SignStatus::Failure;
        written = count;
        return d0::SignStatus::Success;
    }
};
#endif
struct SigningOwner {
#ifdef D0_FACT_REVIEW_PSA
    mbedtls_svc_key_id_t key = 0;
    std::unique_ptr<d0::PsaD0FactSigner> signer;
    SigningOwner() {
        requireCodec(psa_crypto_init() == PSA_SUCCESS, "real PSA signing initialize");
        std::uint8_t scalar[32] = {};
        scalar[31] = 1;
        psa_key_attributes_t a = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&a, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
        psa_set_key_bits(&a, 256);
        psa_set_key_usage_flags(&a, PSA_KEY_USAGE_SIGN_HASH);
        psa_set_key_algorithm(&a, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
        const auto status = psa_import_key(&a, scalar, 32, &key);
        psa_reset_key_attributes(&a);
        requireCodec(status == PSA_SUCCESS, "real synthetic test scalar import");
        signer.reset(new d0::PsaD0FactSigner(key));
    }
    ~SigningOwner() {
        signer.reset();
        if (key)
            requireCodec(psa_destroy_key(key) == PSA_SUCCESS, "test-owned PSA key cleanup");
    }
    d0::IFactSigner &get() {
        return *signer;
    }
#else
    CngSigner signer;
    d0::IFactSigner &get() {
        return signer;
    }
#endif
};
