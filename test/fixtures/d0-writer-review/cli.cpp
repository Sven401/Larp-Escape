#include "../d0-fact-review/Support.h"
#include "Companion/D0FactWriter.h"
#ifdef D0_FACT_REVIEW_PSA
#include "PsaD0FactSigner.h"
#endif

std::string unhex(const std::string &text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); i += 2)
        out += static_cast<char>(std::stoul(text.substr(i, 2), nullptr, 16));
    return out;
}
std::string hex(const std::uint8_t *bytes, std::size_t size) {
    if (!size)
        return "-";
    const char digits[] = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < size; ++i) {
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 15];
    }
    return out;
}
#ifndef D0_FACT_REVIEW_PSA
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
class FaultSigner final : public d0::IFactSigner {
    d0::IFactSigner &real_;

  public:
    int mode = 0;
    explicit FaultSigner(d0::IFactSigner &real) : real_(real) {}
    d0::SignStatus publicKey(d0::CandidatePublicKey &out) override {
        if (mode == 1)
            return d0::SignStatus::Unavailable;
        return real_.publicKey(out);
    }
    d0::SignStatus signHash(const d0::CandidatePublicKey &key, const std::uint8_t hash[32],
                            std::uint8_t signature[64], std::size_t &written) override {
        if (mode == 2)
            return d0::SignStatus::Unavailable;
        const auto status = real_.signHash(key, hash, signature, written);
        if (status == d0::SignStatus::Success) {
            if (mode == 3)
                written = 63;
            if (mode == 4)
                signature[0] ^= 1;
        }
        return status;
    }
};
bool zero(const std::uint8_t *bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i])
            return false;
    return true;
}
bool empty(const d0::FrozenFact &out) {
    const auto &f = out.fact;
    bool clean = !out.length && !out.protectedHeader.offset && !out.protectedHeader.length &&
                 !out.payload.offset && !out.payload.length && !out.signature.offset &&
                 !out.signature.length && zero(out.original, sizeof(out.original)) &&
                 zero(out.kid.bytes, 32) && zero(out.payloadDigest.bytes, 32) && !f.generation &&
                 !f.sequence && !f.type && !f.dependencyCount && !f.rangeCount;
    for (const auto *id : {&f.event, &f.run, &f.origin, &f.station, &f.epoch, &f.subject})
        clean = clean && zero(id->bytes, 16);
    clean = clean && zero(f.binding.bytes, 32) && zero(f.package.bytes, 32);
    for (const auto &d : f.dependencies)
        clean = clean && zero(d.event.bytes, 16) && zero(d.origin.bytes, 16) &&
                zero(d.epoch.bytes, 16) && !d.sequence;
    for (const auto &r : f.ranges)
        clean = clean && !r.first && !r.last;
    return clean;
}
int main() {
#ifdef D0_FACT_REVIEW_PSA
    if (psa_crypto_init() != PSA_SUCCESS)
        return 2;
    std::uint8_t scalar[32] = {};
    scalar[31] = 1;
    psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attrs, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attrs, 256);
    psa_set_key_usage_flags(&attrs, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&attrs, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    mbedtls_svc_key_id_t keyId = 0;
    const auto imported = psa_import_key(&attrs, scalar, sizeof(scalar), &keyId);
    psa_reset_key_attributes(&attrs);
    if (imported != PSA_SUCCESS)
        return 3;
    d0::PsaD0FactSigner realSigner(keyId);
#else
    CngSigner realSigner;
#endif
    RealHash hash;
    RealCrypto crypto;
    FaultSigner signer(realSigner);
    static d0::FrozenFact decoded, out;
    static d0::FactWriteWorkspace workspace;
    std::string x, y, raw;
    int mode = 0;
    while (std::cin >> x >> y >> raw >> mode) {
        d0::CandidatePublicKey sourceKey;
        const auto xb = unhex(x), yb = unhex(y);
        std::memcpy(sourceKey.x, xb.data(), 32);
        std::memcpy(sourceKey.y, yb.data(), 32);
        Bytes source;
        source.bytes = unhex(raw);
        const auto checked = d0::checkFact(source, sourceKey, hash, crypto, decoded);
        if (checked.scope != d0::Scope::SignatureCheckedUnauthenticated)
            return 4;
        d0::Fact fact = decoded.fact;
        if (mode == 10)
            fact.type = 0;
        if (mode == 11)
            fact.dependencyCount = 9;
        if (mode == 12)
            fact.rangeCount = 9;
        if (mode == 13)
            fact.sequence = 0;
        if (mode == 14)
            fact.generation = 0;
        if (mode == 15)
            fact.event = d0::Identity{};
        if (mode == 16 || mode == 17) {
            fact.type = 1;
            fact.rangeCount = 0;
            fact.dependencyCount = mode == 16 ? 2 : 1;
            auto &d = fact.dependencies[0];
            d.event = fact.event;
            d.origin = fact.origin;
            d.epoch = fact.epoch;
            d.sequence = mode == 16 ? fact.sequence + 1 : fact.sequence;
            fact.dependencies[1] = d;
        }
        if (mode == 18) {
            fact.type = 3;
            fact.subject = fact.origin;
            fact.dependencyCount = 0;
            fact.rangeCount = 0;
        }
        if (mode == 19) {
            fact.type = 1;
            fact.rangeCount = 1;
            fact.ranges[0].first = fact.ranges[0].last = 1;
        }
        signer.mode = mode;
        const auto result = d0::writeFact(fact, hash, signer, crypto, workspace, out);
        const bool cleared = empty(out) && zero(workspace.bytes, sizeof(workspace.bytes));
        std::cout << static_cast<unsigned>(result.scope) << ' '
                  << static_cast<unsigned>(result.error) << ' ' << cleared << ' '
                  << hex(out.original, out.length) << ' '
                  << hex(out.original + out.payload.offset, out.payload.length) << ' '
                  << hex(out.payloadDigest.bytes, 32) << ' ' << hex(out.kid.bytes, 32) << ' '
                  << hex(result.publicKey.x, 32) << ' ' << hex(result.publicKey.y, 32) << '\n';
    }
#ifdef D0_FACT_REVIEW_PSA
    if (psa_destroy_key(keyId) != PSA_SUCCESS)
        return 5;
#endif
    return 0;
}
