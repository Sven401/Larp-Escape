#include "PinnedPeerTls.h"
#include <algorithm>
#include <cstring>
#include <psa/crypto.h>
namespace larp {
namespace companion {
const std::size_t PinnedPeerTls::FragmentBytes;
const std::size_t PinnedPeerTls::CipherBytes;
const std::uint32_t PinnedPeerTls::HandshakeMilliseconds;
std::size_t PinnedPeerTls::Ring::put(const unsigned char *p, std::size_t n) noexcept {
    n = std::min(n, bytes.size() - count);
    for (std::size_t i = 0; i < n; ++i)
        bytes[(head + count + i) % bytes.size()] = p[i];
    count += n;
    return n;
}
std::size_t PinnedPeerTls::Ring::take(unsigned char *p, std::size_t n) noexcept {
    n = std::min(n, count);
    for (std::size_t i = 0; i < n; ++i) {
        p[i] = bytes[(head + i) % bytes.size()];
        bytes[(head + i) % bytes.size()] = 0;
    }
    head = (head + n) % bytes.size();
    count -= n;
    return n;
}
void PinnedPeerTls::Ring::clear() noexcept {
    bytes.fill(0);
    head = count = 0;
}
PinnedPeerTls::PinnedPeerTls(const ITlsLifetime &v) noexcept : lifetime_(v) {
    init();
}
PinnedPeerTls::~PinnedPeerTls() {
    release();
}
void PinnedPeerTls::init() noexcept {
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_config_init(&config_);
    mbedtls_x509_crt_init(&certificate_);
    mbedtls_x509_crt_init(&peer_);
    mbedtls_pk_init(&key_);
}
void PinnedPeerTls::release() noexcept {
    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_config_free(&config_);
    mbedtls_x509_crt_free(&certificate_);
    mbedtls_x509_crt_free(&peer_);
    mbedtls_pk_free(&key_);
    pending_.fill(0);
    pendingBytes_ = pendingOffset_ = 0;
    input_.clear();
    output_.clear();
    pin_.fill(0);
    pinBytes_ = 0;
}
void PinnedPeerTls::close() noexcept {
    release();
    init();
    state_ = TlsProgress::Closed;
    error_ = 0;
    origin_ = {};
}
void PinnedPeerTls::fail(int error) noexcept {
    error_ = error ? error : -1;
    release();
    init();
    state_ = TlsProgress::Failed;
}
bool PinnedPeerTls::check(const TlsOrigin &origin, std::uint32_t now) noexcept {
    // A stale foreign packet cannot poison a different current session.
    if (!(origin == origin_) || state_ == TlsProgress::Closed || state_ == TlsProgress::Failed)
        return false;
    if (!lifetime_.current(origin_) ||
        (state_ == TlsProgress::Handshake && now - started_ >= HandshakeMilliseconds)) {
        fail(-1);
        return false;
    }
    return true;
}
int PinnedPeerTls::verify(void *context, mbedtls_x509_crt *cert, int depth, std::uint32_t *flags) {
    auto &self = *static_cast<PinnedPeerTls *>(context);
    std::array<unsigned char, 256> der{};
    const int n = mbedtls_pk_write_pubkey_der(&cert->pk, der.data(), der.size());
    if (depth != 0 || n <= 0 || std::size_t(n) != self.pinBytes_ ||
        std::memcmp(der.data() + der.size() - n, self.pin_.data() + self.pin_.size() - n, n))
        return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    // Explicit leaf-key trust only. Preserve expiry, usage, signature and all
    // other flags; permit no other untrusted certificate or intermediate.
    *flags &= ~MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    return *flags ? MBEDTLS_ERR_X509_CERT_VERIFY_FAILED : 0;
}
int PinnedPeerTls::transmit(void *context, const unsigned char *data, std::size_t bytes) {
    auto &out = static_cast<PinnedPeerTls *>(context)->output_;
    if (out.count == out.bytes.size())
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    return int(out.put(data, std::min(bytes, FragmentBytes)));
}
int PinnedPeerTls::receive(void *context, unsigned char *data, std::size_t bytes) {
    auto &in = static_cast<PinnedPeerTls *>(context)->input_;
    if (!in.count)
        return MBEDTLS_ERR_SSL_WANT_READ;
    return int(in.take(data, std::min(bytes, FragmentBytes)));
}
bool PinnedPeerTls::begin(const TlsOrigin &origin, TlsPem cert, TlsPem key, TlsPem peer,
                          std::uint32_t now) noexcept {
    if (!origin.registration || !origin.epoch || !lifetime_.current(origin) ||
        state_ == TlsProgress::Handshake || state_ == TlsProgress::Ready)
        return false;
    for (const auto &v : {cert, key, peer}) {
        if (!v.data || v.bytes < 2 || v.bytes > 4096 || v.data[v.bytes - 1] != 0)
            return false;
    }
    close();
    origin_ = origin;
    started_ = now;
    state_ = TlsProgress::Handshake;
    int result = psa_crypto_init();
    if (!result)
        result = mbedtls_x509_crt_parse(&certificate_, cert.data, cert.bytes);
    if (!result)
        result = mbedtls_x509_crt_parse(&peer_, peer.data, peer.bytes);
    if (!result)
        result = mbedtls_pk_parse_key(&key_, key.data, key.bytes, nullptr, 0);
    if (!result)
        result =
            mbedtls_ssl_config_defaults(&config_, MBEDTLS_SSL_IS_SERVER,
                                        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (result) {
        fail(result);
        return false;
    }
    const int pinBytes = mbedtls_pk_write_pubkey_der(&peer_.pk, pin_.data(), pin_.size());
    if (pinBytes <= 0) {
        fail(pinBytes);
        return false;
    }
    pinBytes_ = std::size_t(pinBytes);
    static const int suites[] = {MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0};
    static const std::uint16_t groups[] = {MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1, 0};
    mbedtls_ssl_conf_authmode(&config_, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_verify(&config_, verify, this);
    mbedtls_ssl_conf_ca_chain(&config_, &peer_, nullptr);
    mbedtls_ssl_conf_ciphersuites(&config_, suites);
    mbedtls_ssl_conf_groups(&config_, groups);
    mbedtls_ssl_conf_min_tls_version(&config_, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&config_, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_renegotiation(&config_, MBEDTLS_SSL_RENEGOTIATION_DISABLED);
    mbedtls_ssl_conf_session_tickets(&config_, MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
    mbedtls_ssl_conf_session_cache(&config_, nullptr, nullptr, nullptr);
    result = mbedtls_ssl_conf_own_cert(&config_, &certificate_, &key_);
    if (!result)
        result = mbedtls_ssl_setup(&ssl_, &config_);
    if (result) {
        fail(result);
        return false;
    }
    mbedtls_ssl_set_bio(&ssl_, this, transmit, receive, nullptr);
    return check(origin, now);
}
bool PinnedPeerTls::feed(const TlsOrigin &origin, const unsigned char *data, std::size_t bytes,
                         std::uint32_t now) noexcept {
    if (!check(origin, now) || !data || !bytes || bytes > FragmentBytes ||
        bytes > input_.bytes.size() - input_.count)
        return false;
    return input_.put(data, bytes) == bytes;
}
std::size_t PinnedPeerTls::drain(const TlsOrigin &origin, unsigned char *data, std::size_t bytes,
                                 std::uint32_t now) noexcept {
    if (!check(origin, now) || !data)
        return 0;
    return output_.take(data, std::min(bytes, FragmentBytes));
}
TlsProgress PinnedPeerTls::service(std::uint32_t now) noexcept {
    if (!check(origin_, now))
        return state_;
    if (state_ == TlsProgress::Handshake) {
        const int result = mbedtls_ssl_handshake(&ssl_);
        if (!result) {
            if (mbedtls_ssl_get_verify_result(&ssl_) || !lifetime_.current(origin_))
                fail(-1);
            else
                state_ = TlsProgress::Ready;
        } else if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE)
            fail(result);
    } else if (pendingBytes_) {
        const int result = mbedtls_ssl_write(&ssl_, pending_.data() + pendingOffset_,
                                             pendingBytes_ - pendingOffset_);
        if (result > 0) {
            pendingOffset_ += std::size_t(result);
            if (pendingOffset_ == pendingBytes_) {
                pending_.fill(0);
                pendingBytes_ = pendingOffset_ = 0;
            }
        } else if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE)
            fail(result);
    }
    check(origin_, now);
    return state_;
}
std::size_t PinnedPeerTls::read(const TlsOrigin &origin, unsigned char *data, std::size_t bytes,
                                std::uint32_t now) noexcept {
    if (!check(origin, now) || state_ != TlsProgress::Ready || !data || !bytes || pendingBytes_)
        return 0;
    std::array<unsigned char, FragmentBytes> copy{};
    const int result = mbedtls_ssl_read(&ssl_, copy.data(), std::min(bytes, copy.size()));
    if (result > 0 && check(origin, now)) {
        std::copy_n(copy.data(), result, data);
        return std::size_t(result);
    }
    if (result <= 0 && result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE)
        fail(result);
    return 0;
}
bool PinnedPeerTls::send(const TlsOrigin &origin, const unsigned char *data, std::size_t bytes,
                         std::uint32_t now) noexcept {
    if (!check(origin, now) || state_ != TlsProgress::Ready || !data || !bytes ||
        bytes > pending_.size() || pendingBytes_)
        return false;
    std::copy_n(data, bytes, pending_.data());
    pendingBytes_ = bytes;
    return true;
}
} // namespace companion
} // namespace larp
