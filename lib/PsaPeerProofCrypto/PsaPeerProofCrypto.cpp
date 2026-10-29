#include "PsaPeerProofCrypto.h"
#include <algorithm>
#include <psa/crypto.h>

namespace larp {
namespace companion {
namespace {
constexpr std::uint8_t Order[32] = {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51};
constexpr std::uint8_t HalfOrder[32] = {
    0x7f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xde, 0x73, 0x7d, 0x56, 0xd3, 0x8b, 0xcf, 0x42, 0x79, 0xdc, 0xe5, 0x61, 0x7e, 0x31, 0x92, 0xa8};
bool positive(const std::uint8_t *value) {
    for (unsigned i = 0; i < 32; ++i)
        if (value[i])
            return true;
    return false;
}
bool canonical(const PeerSignature &signature) {
    const auto *r = signature.data(), *s = r + 32;
    return positive(r) && positive(s) &&
           std::lexicographical_compare(r, r + 32, Order, Order + 32) &&
           !std::lexicographical_compare(HalfOrder, HalfOrder + 32, s, s + 32);
}
class PublicKey {
  public:
    ~PublicKey() {
        if (key_ != 0)
            psa_destroy_key(key_);
    }
    bool load(const PeerPublicKey &bytes) {
        if (bytes[0] != 4 || psa_crypto_init() != PSA_SUCCESS)
            return false;
        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
        psa_set_key_bits(&attributes, 256);
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
        psa_set_key_algorithm(&attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
        const auto status = psa_import_key(&attributes, bytes.data(), bytes.size(), &key_);
        psa_reset_key_attributes(&attributes);
        return status == PSA_SUCCESS;
    }
    mbedtls_svc_key_id_t get() const {
        return key_;
    }
    bool close() {
        const auto status = psa_destroy_key(key_);
        key_ = 0;
        return status == PSA_SUCCESS;
    }

  private:
    mbedtls_svc_key_id_t key_ = 0;
};
} // namespace
bool PsaPeerProofCrypto::validKey(const PeerPublicKey &callerKey) noexcept {
    const auto key = callerKey;
    PublicKey imported;
    return imported.load(key) && imported.close();
}
bool PsaPeerProofCrypto::verify(const PeerPublicKey &callerKey,
                                const PeerTranscript &callerTranscript,
                                const PeerSignature &callerSignature) noexcept {
    // Freeze caller storage before any library allocation or crypto operation.
    const auto key = callerKey;
    const auto transcript = callerTranscript;
    const auto signature = callerSignature;
    if (!canonical(signature))
        return false;
    PublicKey imported;
    if (!imported.load(key))
        return false;
    const bool verified =
        psa_verify_message(imported.get(), PSA_ALG_ECDSA(PSA_ALG_SHA_256), transcript.data(),
                           transcript.size(), signature.data(), signature.size()) == PSA_SUCCESS;
    const bool destroyed = imported.close();
    return verified && destroyed;
}
} // namespace companion
} // namespace larp
