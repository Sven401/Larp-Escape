#pragma once
#include "Companion/D0Fact.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
namespace d0 = larp::companion::d0;
using larp::configuration::IPackageByteSource;
using larp::configuration::IPackageDigest;
using larp::configuration::PackageReadStatus;
#if defined(D0_FACT_REVIEW_PSA)
#define PACKAGE_MODULE_REVIEW_PSA
#include "../rules-planner-review/CompilerSupport.h"
#include "PsaD0FactCrypto.h"
using RealHash = Hash;
using RealCrypto = d0::PsaD0FactCrypto;
#elif defined(_WIN32)
#include "../rules-planner-review/CompilerSupport.h"
using RealHash = Hash;
class RealCrypto final : public d0::IFactCrypto {
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_KEY_HANDLE key_ = nullptr;

  public:
    ~RealCrypto() {
        reset();
        if (algorithm_)
            BCryptCloseAlgorithmProvider(algorithm_, 0);
    }
    bool reset() override {
        if (!key_)
            return true;
        if (BCryptDestroyKey(key_) < 0)
            return false;
        key_ = nullptr;
        return true;
    }
    d0::VerifyResult verifyHash(const d0::CandidatePublicKey &key, const std::uint8_t hash[32],
                                const std::uint8_t signature[64]) override {
        if (!reset())
            return d0::VerifyResult::CleanupFailure;
        if (!algorithm_ &&
            BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) < 0)
            return d0::VerifyResult::Unavailable;
        std::uint8_t raw[sizeof(BCRYPT_ECCKEY_BLOB) + 64] = {};
        BCRYPT_ECCKEY_BLOB header;
        header.dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
        header.cbKey = 32;
        std::memcpy(raw, &header, sizeof(header));
        std::memcpy(raw + sizeof(header), key.x, 32);
        std::memcpy(raw + sizeof(header) + 32, key.y, 32);
        if (BCryptImportKeyPair(algorithm_, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key_, raw, sizeof(raw),
                                0) < 0)
            return d0::VerifyResult::InvalidKey;
        const auto status = BCryptVerifySignature(key_, nullptr, const_cast<PUCHAR>(hash), 32,
                                                  const_cast<PUCHAR>(signature), 64, 0);
        if (!reset())
            return d0::VerifyResult::CleanupFailure;
        return status >= 0 ? d0::VerifyResult::Valid : d0::VerifyResult::InvalidSignature;
    }
};
#else
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
class RealHash final : public IPackageDigest {
    EVP_MD_CTX *ctx_ = nullptr;

  public:
    ~RealHash() {
        abort();
    }
    bool begin() override {
        abort();
        ctx_ = EVP_MD_CTX_new();
        return ctx_ && EVP_DigestInit_ex(ctx_, EVP_sha256(), nullptr) == 1;
    }
    bool update(const std::uint8_t *bytes, std::size_t n) override {
        return ctx_ && EVP_DigestUpdate(ctx_, bytes, n) == 1;
    }
    bool finish(std::uint8_t digest[32], std::size_t &written) override {
        unsigned n = 0;
        const bool ok = ctx_ && EVP_DigestFinal_ex(ctx_, digest, &n) == 1;
        written = n;
        return ok;
    }
    bool abort() override {
        EVP_MD_CTX_free(ctx_);
        ctx_ = nullptr;
        return true;
    }
};
class RealCrypto final : public d0::IFactCrypto {
  public:
    bool reset() override {
        return true;
    }
    d0::VerifyResult verifyHash(const d0::CandidatePublicKey &key, const std::uint8_t hash[32],
                                const std::uint8_t signature[64]) override {
        std::uint8_t point[65] = {4};
        std::memcpy(point + 1, key.x, 32);
        std::memcpy(point + 33, key.y, 32);
        EVP_PKEY_CTX *build = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
        OSSL_PARAM_BLD *params = OSSL_PARAM_BLD_new();
        EVP_PKEY *pub = nullptr;
        const bool setup =
            build && params &&
            OSSL_PARAM_BLD_push_utf8_string(params, OSSL_PKEY_PARAM_GROUP_NAME, "prime256v1", 0) &&
            OSSL_PARAM_BLD_push_octet_string(params, OSSL_PKEY_PARAM_PUB_KEY, point, 65);
        OSSL_PARAM *p = setup ? OSSL_PARAM_BLD_to_param(params) : nullptr;
        const bool imported = p && EVP_PKEY_fromdata_init(build) == 1 &&
                              EVP_PKEY_fromdata(build, &pub, EVP_PKEY_PUBLIC_KEY, p) == 1;
        OSSL_PARAM_free(p);
        OSSL_PARAM_BLD_free(params);
        EVP_PKEY_CTX_free(build);
        EVP_PKEY_CTX *ctx = imported ? EVP_PKEY_CTX_new_from_pkey(nullptr, pub, nullptr) : nullptr;
        if (!ctx || EVP_PKEY_public_check(ctx) != 1) {
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(pub);
            return d0::VerifyResult::InvalidKey;
        }
        ECDSA_SIG *sig = ECDSA_SIG_new();
        BIGNUM *r = BN_bin2bn(signature, 32, nullptr), *s = BN_bin2bn(signature + 32, 32, nullptr);
        bool ok = sig && r && s;
        if (ok) {
            ok = ECDSA_SIG_set0(sig, r, s) == 1;
            if (ok) {
                r = nullptr;
                s = nullptr;
            }
        }
        unsigned char der[80], *out = der;
        const int n = ok ? i2d_ECDSA_SIG(sig, &out) : 0;
        const int verified = n > 0 && EVP_PKEY_verify_init(ctx) == 1 &&
                                     EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) == 1
                                 ? EVP_PKEY_verify(ctx, der, static_cast<std::size_t>(n), hash, 32)
                                 : 0;
        BN_free(r);
        BN_free(s);
        ECDSA_SIG_free(sig);
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pub);
        return verified == 1 ? d0::VerifyResult::Valid : d0::VerifyResult::InvalidSignature;
    }
};
#endif
