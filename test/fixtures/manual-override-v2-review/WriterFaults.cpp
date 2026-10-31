#if defined(MANUAL_OVERRIDE_V2_REVIEW_PSA) && !defined(D0_FACT_REVIEW_PSA)
#define D0_FACT_REVIEW_PSA
#endif
// Reuse the examined real backend/byte helpers, never its main or assertions.
#define main rootWriterReviewMain
#include "../d0-writer-review/cli.cpp"
#undef main
#ifndef D0_FACT_REVIEW_PSA
class CngSecondSigner final : public d0::IFactSigner {
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_KEY_HANDLE key_ = nullptr;
    d0::CandidatePublicKey public_;

  public:
    CngSecondSigner() {
        const auto x = unhex("7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978");
        const auto y = unhex("07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1");
        std::memcpy(public_.x, x.data(), 32);
        std::memcpy(public_.y, y.data(), 32);
        std::uint8_t blob[sizeof(BCRYPT_ECCKEY_BLOB) + 96] = {};
        BCRYPT_ECCKEY_BLOB header;
        header.dwMagic = BCRYPT_ECDSA_PRIVATE_P256_MAGIC;
        header.cbKey = 32;
        std::memcpy(blob, &header, sizeof(header));
        std::memcpy(blob + sizeof(header), public_.x, 32);
        std::memcpy(blob + sizeof(header) + 32, public_.y, 32);
        blob[sizeof(blob) - 1] = 2; // Public synthetic private scalar, never a deployment key.
        if (BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) >= 0)
            BCryptImportKeyPair(algorithm_, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key_, blob,
                                sizeof(blob), 0);
    }
    ~CngSecondSigner() {
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
class WriterAuditHash final : public IPackageDigest {
    RealHash real_;
    int mode_ = 0, target_ = 1;

  public:
    unsigned begins = 0, aborts = 0, hits = 0;
    void configure(int mode, int target) {
        mode_ = mode;
        target_ = target;
        begins = aborts = hits = 0;
    }
    bool selected() const {
        return static_cast<int>(begins) == target_;
    }
    bool begin() override {
        ++begins;
        if (mode_ == 1 && selected()) {
            ++hits;
            return false;
        }
        return real_.begin();
    }
    bool update(const std::uint8_t *b, std::size_t n) override {
        if (mode_ == 2 && selected()) {
            ++hits;
            return false;
        }
        return real_.update(b, n);
    }
    bool finish(std::uint8_t d[32], std::size_t &written) override {
        if (mode_ == 3 && selected()) {
            ++hits;
            return false;
        }
        const bool ok = real_.finish(d, written);
        if (mode_ == 4 && selected()) {
            written = 31;
            ++hits;
        }
        return ok;
    }
    bool abort() override {
        ++aborts;
        const bool ok = real_.abort();
        if ((mode_ == 5 && selected()) || (mode_ == 6 && static_cast<int>(aborts) == target_)) {
            ++hits;
            return false;
        }
        return ok;
    }
};
class WriterAuditVerifier final : public d0::IFactCrypto {
    RealCrypto &real_;
    int resetTarget_ = 0, mode_ = 0;

  public:
    unsigned resets = 0, verifies = 0, hits = 0;
    explicit WriterAuditVerifier(RealCrypto &real) : real_(real) {}
    void configure(int resetTarget, int mode) {
        resetTarget_ = resetTarget;
        mode_ = mode;
        resets = verifies = hits = 0;
    }
    bool reset() override {
        ++resets;
        const bool ok = real_.reset();
        if (static_cast<int>(resets) == resetTarget_) {
            ++hits;
            return false;
        }
        return ok;
    }
    d0::VerifyResult verifyHash(const d0::CandidatePublicKey &k, const std::uint8_t h[32],
                                const std::uint8_t s[64]) override {
        ++verifies;
        if (mode_) {
            ++hits;
            return mode_ == 1   ? d0::VerifyResult::InvalidSignature
                   : mode_ == 2 ? d0::VerifyResult::InvalidKey
                   : mode_ == 3 ? d0::VerifyResult::Unavailable
                   : mode_ == 4 ? d0::VerifyResult::Failure
                                : d0::VerifyResult::CleanupFailure;
        }
        return real_.verifyHash(k, h, s);
    }
};
class WriterAuditSigner final : public d0::IFactSigner {
    d0::IFactSigner &one_, &two_;

  public:
    bool wrong = false, mutate = false;
    unsigned hits = 0;
    d0::Fact *input = nullptr;
    std::uint8_t wrongHash[32] = {}, wrongSignature[64] = {};
    WriterAuditSigner(d0::IFactSigner &one, d0::IFactSigner &two) : one_(one), two_(two) {}
    void configure(bool wrongKey, bool mutateInput, d0::Fact &fact) {
        wrong = wrongKey;
        mutate = mutateInput;
        input = &fact;
        hits = 0;
        std::memset(wrongHash, 0, 32);
        std::memset(wrongSignature, 0, 64);
    }
    d0::SignStatus publicKey(d0::CandidatePublicKey &out) override {
        const auto result = one_.publicKey(out);
        if (mutate) {
            *input = d0::Fact{};
            ++hits;
        }
        return result;
    }
    d0::SignStatus signHash(const d0::CandidatePublicKey &k, const std::uint8_t h[32],
                            std::uint8_t s[64], std::size_t &written) override {
        if (!wrong)
            return one_.signHash(k, h, s, written);
        ++hits;
        d0::CandidatePublicKey other;
        const auto exported = two_.publicKey(other);
        if (exported != d0::SignStatus::Success)
            return exported;
        const auto result = two_.signHash(other, h, s, written);
        if (result == d0::SignStatus::Success && written == 64) {
            std::memcpy(wrongHash, h, 32);
            std::memcpy(wrongSignature, s, 64);
        }
        return result;
    }
};
const char *writeError(d0::WriteError e) {
    switch (e) {
    case d0::WriteError::None:
        return "None";
    case d0::WriteError::InvalidFact:
        return "InvalidFact";
    case d0::WriteError::LengthLimit:
        return "LengthLimit";
    case d0::WriteError::InvalidKey:
        return "InvalidKey";
    case d0::WriteError::SignerUnavailable:
        return "SignerUnavailable";
    case d0::WriteError::SigningFailure:
        return "SigningFailure";
    case d0::WriteError::SignatureLength:
        return "SignatureLength";
    case d0::WriteError::HashFailure:
        return "HashFailure";
    case d0::WriteError::VerificationFailure:
        return "VerificationFailure";
    case d0::WriteError::CleanupFailure:
        return "CleanupFailure";
    }
    return "InvalidError";
}
#ifdef D0_FACT_REVIEW_PSA
mbedtls_svc_key_id_t importSynthetic(unsigned value) {
    std::uint8_t scalar[32] = {};
    scalar[31] = static_cast<std::uint8_t>(value);
    psa_key_attributes_t a = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&a, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&a, 256);
    psa_set_key_usage_flags(&a, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&a, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    mbedtls_svc_key_id_t id = 0;
    const auto r = psa_import_key(&a, scalar, 32, &id);
    psa_reset_key_attributes(&a);
    if (r != PSA_SUCCESS)
        throw 5;
    return id;
}
#endif
int main() {
#ifdef D0_FACT_REVIEW_PSA
    if (psa_crypto_init() != PSA_SUCCESS)
        return 2;
    const auto key1 = importSynthetic(1), key2 = importSynthetic(2);
    d0::PsaD0FactSigner one(key1), two(key2);
#else
    CngSigner one;
    CngSecondSigner two;
#endif
    RealHash decodeHash;
    RealCrypto realVerifier;
    WriterAuditHash hash;
    WriterAuditVerifier verifier(realVerifier);
    WriterAuditSigner signer(one, two);
    static d0::FrozenFact decoded, out;
    static d0::FactWriteWorkspace workspace;
    std::string x, y, raw;
    int kind = 0, mode = 0, target = 1;
    while (std::cin >> x >> y >> raw >> kind >> mode >> target) {
        d0::CandidatePublicKey candidate;
        const auto xb = unhex(x), yb = unhex(y);
        std::memcpy(candidate.x, xb.data(), 32);
        std::memcpy(candidate.y, yb.data(), 32);
        Bytes input;
        input.bytes = unhex(raw);
        const auto admitted = d0::checkFact(input, candidate, decodeHash, realVerifier, decoded);
        if (admitted.scope != d0::Scope::SignatureCheckedUnauthenticated)
            return 3;
        d0::Fact fact = decoded.fact;
        hash.configure(kind == 4 ? mode : kind == 7 ? 6 : 0, target);
        verifier.configure(kind == 5 ? target : 0, kind == 6 ? mode : 0);
        signer.configure(kind == 1, kind == 2, fact);
        if (kind == 3)
            out.fact = fact;
        const auto result =
            d0::writeFact(kind == 3 ? out.fact : fact, hash, signer, verifier, workspace, out);
        bool actualWrongKeySignature = false;
        if (kind == 1) {
            d0::CandidatePublicKey other;
            if (two.publicKey(other) == d0::SignStatus::Success)
                actualWrongKeySignature =
                    realVerifier.verifyHash(other, signer.wrongHash, signer.wrongSignature) ==
                    d0::VerifyResult::Valid;
        }
        std::cout << (result.scope == d0::WriteScope::SignedUnpublished ? 'A' : 'R') << ' '
                  << writeError(result.error) << ' '
                  << (empty(out) && zero(workspace.bytes, sizeof(workspace.bytes))) << ' '
                  << hex(out.original, out.length) << ' '
                  << hex(out.original + out.payload.offset, out.payload.length) << ' '
                  << hex(out.payloadDigest.bytes, 32) << ' ' << hex(out.kid.bytes, 32) << ' '
                  << actualWrongKeySignature << ' ' << hash.hits << ' ' << verifier.hits << ' '
                  << signer.hits << ' ' << hex(signer.wrongHash, 32) << ' '
                  << hex(signer.wrongSignature, 64) << ' ' << hex(result.publicKey.x, 32) << ' '
                  << hex(result.publicKey.y, 32) << '\n';
    }
#ifdef D0_FACT_REVIEW_PSA
    if (psa_destroy_key(key1) != PSA_SUCCESS || psa_destroy_key(key2) != PSA_SUCCESS)
        return 6;
#endif
    return 0;
}
