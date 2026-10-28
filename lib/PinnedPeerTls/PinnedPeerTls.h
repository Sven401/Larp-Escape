#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
namespace larp {
namespace companion {
struct TlsOrigin {
    std::uint32_t registration = 0, epoch = 0;
    std::uint16_t handle = 0;
    bool operator==(const TlsOrigin &v) const noexcept {
        return registration == v.registration && epoch == v.epoch && handle == v.handle;
    }
};
class ITlsLifetime {
  public:
    virtual ~ITlsLifetime() = default;
    // Trusted thread-safe observation of the ORIGINAL link/credential lifetime.
    // This is not a role grant. Loss/revocation must make this false permanently
    // for the old origin, including after reuse of a handle or address.
    virtual bool current(const TlsOrigin &) const noexcept = 0;
};
struct TlsPem {
    const unsigned char *data;
    std::size_t bytes;
};
enum class TlsProgress : std::uint8_t { Closed, Handshake, Ready, Failed };
// One serialized crypto worker. Never called from a Bluetooth callback/ISR.
// Stream ciphertext only; copied plaintext still carries origin to its final
// authorization owner. Successful TLS does not assert SMP, a bond, or a role.
class PinnedPeerTls {
  public:
    static const std::size_t FragmentBytes = 128, CipherBytes = 4096;
    static const std::uint32_t HandshakeMilliseconds = 30000;
    explicit PinnedPeerTls(const ITlsLifetime &) noexcept;
    ~PinnedPeerTls();
    PinnedPeerTls(const PinnedPeerTls &) = delete;
    PinnedPeerTls &operator=(const PinnedPeerTls &) = delete;
    // PEMs include trailing NUL and are copied by the TLS parser. Exact leaf
    // SPKI is trusted; no hostname/CA-based replacement or session resumption.
    bool begin(const TlsOrigin &, TlsPem certificate, TlsPem privateKey, TlsPem expectedPeer,
               std::uint32_t now) noexcept;
    bool feed(const TlsOrigin &, const unsigned char *, std::size_t, std::uint32_t now) noexcept;
    std::size_t drain(const TlsOrigin &, unsigned char *, std::size_t, std::uint32_t now) noexcept;
    TlsProgress service(std::uint32_t now) noexcept;
    std::size_t read(const TlsOrigin &, unsigned char *, std::size_t, std::uint32_t now) noexcept;
    // One owned application write retained unchanged over WANT_READ/WRITE.
    bool send(const TlsOrigin &, const unsigned char *, std::size_t, std::uint32_t now) noexcept;
    void close() noexcept;
    TlsProgress state() const noexcept {
        return state_;
    }
    int error() const noexcept {
        return error_;
    }

  private:
    struct Ring {
        std::array<unsigned char, CipherBytes> bytes{};
        std::size_t head = 0, count = 0;
        std::size_t put(const unsigned char *, std::size_t) noexcept;
        std::size_t take(unsigned char *, std::size_t) noexcept;
        void clear() noexcept;
    } input_, output_;
    const ITlsLifetime &lifetime_;
    TlsOrigin origin_;
    mbedtls_ssl_context ssl_;
    mbedtls_ssl_config config_;
    mbedtls_x509_crt certificate_, peer_;
    mbedtls_pk_context key_;
    std::array<unsigned char, 256> pin_{};
    std::size_t pinBytes_ = 0;
    std::array<unsigned char, FragmentBytes> pending_{};
    std::size_t pendingBytes_ = 0, pendingOffset_ = 0;
    std::uint32_t started_ = 0;
    TlsProgress state_ = TlsProgress::Closed;
    int error_ = 0;
    bool check(const TlsOrigin &, std::uint32_t) noexcept;
    void fail(int) noexcept;
    void init() noexcept;
    void release() noexcept;
    static int verify(void *, mbedtls_x509_crt *, int, std::uint32_t *);
    static int transmit(void *, const unsigned char *, std::size_t);
    static int receive(void *, unsigned char *, std::size_t);
};
} // namespace companion
} // namespace larp
