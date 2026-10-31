#include "Support.h"
std::string unhex(const std::string &s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); i += 2)
        out += static_cast<char>(std::stoul(s.substr(i, 2), nullptr, 16));
    return out;
}
std::string hex(const std::uint8_t *bytes, std::size_t count) {
    if (!count)
        return "-";
    const char h[] = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < count; ++i) {
        s += h[bytes[i] >> 4];
        s += h[bytes[i] & 15];
    }
    return s;
}
template <typename T> std::string hex(const T &v) {
    return hex(v.bytes, sizeof(v.bytes));
}
class Source final : public IPackageByteSource {
  public:
    std::string bytes;
    std::size_t offset = 0, failure = static_cast<std::size_t>(-1);
    PackageReadStatus readByte(std::uint8_t &byte) override {
        if (offset == failure)
            return PackageReadStatus::Failure;
        if (offset == bytes.size())
            return PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes[offset++]);
        return PackageReadStatus::Byte;
    }
};
class AuditHash final : public IPackageDigest {
    RealHash real_;
    int mode_ = 0, document_ = 1;

  public:
    unsigned begins = 0, hits = 0;
    void configure(int mode, int document) {
        mode_ = mode;
        document_ = document;
        begins = hits = 0;
    }
    bool target() const {
        return static_cast<int>(begins) == document_;
    }
    bool begin() override {
        ++begins;
        if (target() && mode_ == 1) {
            ++hits;
            return false;
        }
        return real_.begin();
    }
    bool update(const std::uint8_t *b, std::size_t n) override {
        if (target() && mode_ == 2) {
            ++hits;
            return false;
        }
        return real_.update(b, n);
    }
    bool finish(std::uint8_t digest[32], std::size_t &written) override {
        if (target() && mode_ == 3) {
            ++hits;
            return false;
        }
        const bool ok = real_.finish(digest, written);
        if (target() && mode_ == 4) {
            written = 31;
            ++hits;
        }
        return ok;
    }
    bool abort() override {
        const bool ok = real_.abort();
        if (target() && mode_ == 5) {
            ++hits;
            return false;
        }
        return ok;
    }
};
class AuditCrypto final : public d0::IFactCrypto {
    RealCrypto real_;
    int mode_ = 0, resetFailure_ = 0;

  public:
    unsigned verifies = 0, resets = 0, hits = 0;
    std::uint8_t checkedHash[32] = {};
    void configure(int mode, int resetFailure) {
        mode_ = mode;
        resetFailure_ = resetFailure;
        verifies = resets = hits = 0;
        std::memset(checkedHash, 0, 32);
    }
    bool reset() override {
        ++resets;
        const bool ok = real_.reset();
        if (static_cast<int>(resets) == resetFailure_) {
            ++hits;
            return false;
        }
        return ok;
    }
    d0::VerifyResult verifyHash(const d0::CandidatePublicKey &key, const std::uint8_t hash[32],
                                const std::uint8_t signature[64]) override {
        ++verifies;
        std::memcpy(checkedHash, hash, 32);
        if (mode_) {
            ++hits;
            return mode_ == 1   ? d0::VerifyResult::InvalidSignature
                   : mode_ == 2 ? d0::VerifyResult::InvalidKey
                   : mode_ == 3 ? d0::VerifyResult::Unavailable
                   : mode_ == 4 ? d0::VerifyResult::Failure
                                : d0::VerifyResult::CleanupFailure;
        }
        return real_.verifyHash(key, hash, signature);
    }
};
const char *name(d0::Error e) {
    switch (e) {
    case d0::Error::None:
        return "None";
    case d0::Error::SourceFailure:
        return "SourceFailure";
    case d0::Error::LengthLimit:
        return "LengthLimit";
    case d0::Error::Cbor:
        return "Cbor";
    case d0::Error::Header:
        return "Header";
    case d0::Error::Version:
        return "Version";
    case d0::Error::UnsupportedKind:
        return "UnsupportedKind";
    case d0::Error::Value:
        return "Value";
    case d0::Error::DuplicateDependency:
        return "DuplicateDependency";
    case d0::Error::SelfDependency:
        return "SelfDependency";
    case d0::Error::KeyIdMismatch:
        return "KeyIdMismatch";
    case d0::Error::HashFailure:
        return "HashFailure";
    case d0::Error::InvalidKey:
        return "InvalidKey";
    case d0::Error::InvalidSignature:
        return "InvalidSignature";
    case d0::Error::CryptoUnavailable:
        return "CryptoUnavailable";
    case d0::Error::CryptoFailure:
        return "CryptoFailure";
    case d0::Error::CleanupFailure:
        return "CleanupFailure";
    }
    return "InvalidError";
}
std::string fact(const d0::Fact &f) {
    std::ostringstream s;
    s << hex(f.event) << ',' << hex(f.run) << ',' << hex(f.origin) << ',' << hex(f.station) << ','
      << f.generation << ',' << hex(f.binding) << ',' << hex(f.package) << ',' << hex(f.epoch)
      << ',' << f.sequence << ',' << unsigned(f.type) << ',' << hex(f.subject) << ','
      << unsigned(f.dependencyCount) << ',' << unsigned(f.rangeCount);
    for (unsigned i = 0; i < 8; ++i) {
        const auto &d = f.dependencies[i];
        s << ',' << hex(d.event) << ',' << hex(d.origin) << ',' << hex(d.epoch) << ','
          << d.sequence;
        const auto &r = f.ranges[i];
        s << ',' << r.first << ',' << r.last;
    }
    return s.str();
}
bool empty(const d0::FrozenFact &f) {
    for (auto b : f.original)
        if (b)
            return false;
    const d0::FrozenFact zero;
    return !f.length && !f.protectedHeader.offset && !f.protectedHeader.length &&
           !f.payload.offset && !f.payload.length && !f.signature.offset && !f.signature.length &&
           hex(f.kid) == hex(zero.kid) && hex(f.payloadDigest) == hex(zero.payloadDigest) &&
           fact(f.fact) == fact(zero.fact);
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--layout") {
        std::cout << "FrozenFact " << sizeof(d0::FrozenFact) << " Fact " << sizeof(d0::Fact)
                  << " Result " << sizeof(d0::Result) << '\n';
        return 0;
    }
    d0::FrozenFact output;
    AuditHash hash;
    AuditCrypto crypto;
    std::string line;
    while (std::getline(std::cin, line)) {
        long long failure = -1;
        int hashMode = 0, hashDocument = 1, cryptoMode = 0, resetFailure = 0;
        std::istringstream h(line);
        h >> failure >> hashMode >> hashDocument >> cryptoMode >> resetFailure;
        d0::CandidatePublicKey key;
        std::getline(std::cin, line);
        auto x = unhex(line);
        std::getline(std::cin, line);
        auto y = unhex(line);
        if (x.size() != 32 || y.size() != 32)
            return 2;
        std::memcpy(key.x, x.data(), 32);
        std::memcpy(key.y, y.data(), 32);
        Source source;
        std::getline(std::cin, line);
        source.bytes = unhex(line);
        if (failure >= 0)
            source.failure = static_cast<std::size_t>(failure);
        hash.configure(hashMode, hashDocument);
        crypto.configure(cryptoMode, resetFailure);
        const auto r = d0::checkFact(source, key, hash, crypto, output);
        // Destroy caller source/key bytes after return: the result must own every published byte.
        source.bytes.assign(source.bytes.size(), static_cast<char>(0xcc));
        for (auto &b : key.x)
            b = 0;
        for (auto &b : key.y)
            b = 0;
        const auto ph = output.protectedHeader, p = output.payload, s = output.signature;
        std::cout << (r.scope == d0::Scope::SignatureCheckedUnauthenticated ? 'A' : 'R') << ' '
                  << name(r.error) << ' ' << r.offset << ' ' << r.readBytes << ' ' << empty(output)
                  << ' ' << hash.begins << ' ' << hash.hits << ' ' << crypto.verifies << ' '
                  << crypto.resets << ' ' << crypto.hits << ' ' << output.length << ' '
                  << hex(output.original, output.length) << ' '
                  << hex(output.original + ph.offset, ph.length) << ' '
                  << hex(output.original + p.offset, p.length) << ' '
                  << hex(output.original + s.offset, s.length) << ' ' << hex(output.kid) << ' '
                  << hex(output.payloadDigest) << ' ' << hex(crypto.checkedHash, 32) << ' '
                  << fact(output.fact) << '\n';
    }
}
