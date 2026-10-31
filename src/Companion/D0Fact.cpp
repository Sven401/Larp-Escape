#include "Companion/D0Fact.h"
#include <cstring>
#include <new>

namespace larp {
namespace companion {
namespace d0 {
namespace {
void clear(FrozenFact &out) {
    // Placement construction initializes caller storage directly, with no
    // whole-packet temporary or heap allocation.
    out.~FrozenFact();
    ::new (static_cast<void *>(&out)) FrozenFact;
}
bool equal(const Identity &a, const Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
bool equal(const FactId &a, const FactId &b) {
    return equal(a.event, b.event) && equal(a.origin, b.origin) && equal(a.epoch, b.epoch) &&
           a.sequence == b.sequence;
}
struct Cursor {
    const std::uint8_t *data;
    std::size_t size, base, pos = 0;
    Result &result;
    Cursor(const std::uint8_t *d, std::size_t n, std::size_t b, Result &r)
        : data(d), size(n), base(b), result(r) {}
    bool fail(Error error = Error::Cbor) {
        if (result.error == Error::None) {
            result.error = error;
            result.offset = base + pos;
        }
        return false;
    }
    bool head(unsigned major, std::uint64_t &value) {
        if (pos == size)
            return fail();
        const unsigned byte = data[pos++], ai = byte & 31;
        if ((byte >> 5) != major || ai > 27)
            return fail();
        if (ai < 24) {
            value = ai;
            return true;
        }
        const unsigned n = 1u << (ai - 24);
        if (size - pos < n)
            return fail();
        value = 0;
        for (unsigned i = 0; i < n; ++i)
            value = (value << 8) | data[pos++];
        if ((n == 1 && value < 24) || (n == 2 && value <= 255) || (n == 4 && value <= 65535) ||
            (n == 8 && value <= UINT32_MAX))
            return fail();
        return true;
    }
    bool expect(unsigned major, std::uint64_t expected) {
        std::uint64_t v = 0;
        return head(major, v) && (v == expected || fail());
    }
    bool uint(std::uint64_t &value, bool positive = false) {
        return head(0, value) && (!positive || value || fail(Error::Value));
    }
    bool blob(Slice &s) {
        std::uint64_t n = 0;
        if (!head(2, n))
            return false;
        if (n > size - pos)
            return fail();
        s.offset = static_cast<std::uint16_t>(base + pos);
        s.length = static_cast<std::uint16_t>(n);
        pos += static_cast<std::size_t>(n);
        return true;
    }
    bool bytes(std::uint8_t *out, std::size_t n, bool nonzero = false) {
        Slice s;
        if (!blob(s))
            return false;
        if (s.length != n)
            return fail(Error::Value);
        std::memcpy(out, data + s.offset - base, n);
        if (nonzero) {
            std::uint8_t any = 0;
            for (std::size_t i = 0; i < n; ++i)
                any |= out[i];
            if (!any)
                return fail(Error::Value);
        }
        return true;
    }
    bool id(Identity &out) {
        return bytes(out.bytes, 16, true);
    }
    bool end() {
        return pos == size || fail();
    }
};
bool parse(FrozenFact &out, Result &r) {
    Cursor c(out.original, out.length, 0, r);
    if (!c.expect(6, 18) || !c.expect(4, 4) || !c.blob(out.protectedHeader))
        return false;
    Cursor h(out.original + out.protectedHeader.offset, out.protectedHeader.length,
             out.protectedHeader.offset, r);
    if (!h.expect(5, 2) || !h.expect(0, 1) || !h.expect(1, 6) || !h.expect(0, 4) ||
        !h.bytes(out.kid.bytes, 32) || !h.end()) {
        if (r.error == Error::Cbor)
            r.error = Error::Header;
        return false;
    }
    if (!c.expect(5, 0) || !c.blob(out.payload) || !c.blob(out.signature))
        return false;
    if (out.signature.length != 64)
        return c.fail(Error::Value);
    if (!c.end())
        return false;
    Cursor p(out.original + out.payload.offset, out.payload.length, out.payload.offset, r);
    if (!p.expect(5, 10) || !p.expect(0, 0))
        return false;
    std::uint64_t value = 0;
    if (!p.uint(value))
        return false;
    if (value != 1)
        return p.fail(Error::Version);
    if (!p.expect(0, 1) || !p.uint(value))
        return false;
    if (value != 1)
        return p.fail(Error::UnsupportedKind);
    Fact &f = out.fact;
    if (!p.expect(0, 2) || !p.id(f.event) || !p.expect(0, 3) || !p.id(f.run) || !p.expect(0, 4) ||
        !p.id(f.origin) || !p.expect(0, 5) || !p.id(f.station) || !p.expect(0, 6) ||
        !p.uint(f.generation, true) || !p.expect(0, 7) || !p.bytes(f.binding.bytes, 32) ||
        !p.expect(0, 8) || !p.bytes(f.package.bytes, 32) || !p.expect(0, 9) || !p.expect(4, 7) ||
        !p.id(f.epoch) || !p.uint(f.sequence, true) || !p.uint(value))
        return false;
    if (value < 1 || value > 3)
        return p.fail(Error::Value);
    f.type = static_cast<std::uint8_t>(value);
    if (!p.id(f.subject) || !p.expect(7, 21) || !p.head(4, value))
        return false;
    if (value > 8)
        return p.fail(Error::Value);
    f.dependencyCount = static_cast<std::uint8_t>(value);
    FactId self;
    self.event = f.event;
    self.origin = f.origin;
    self.epoch = f.epoch;
    self.sequence = f.sequence;
    for (unsigned i = 0; i < f.dependencyCount; ++i) {
        auto &d = f.dependencies[i];
        if (!p.expect(4, 4) || !p.id(d.event) || !p.id(d.origin) || !p.id(d.epoch) ||
            !p.uint(d.sequence, true))
            return false;
        if (equal(d, self))
            return p.fail(Error::SelfDependency);
        for (unsigned j = 0; j < i; ++j)
            if (equal(d, f.dependencies[j]))
                return p.fail(Error::DuplicateDependency);
    }
    if (!p.head(4, value))
        return false;
    if (value > 8)
        return p.fail(Error::Value);
    f.rangeCount = static_cast<std::uint8_t>(value);
    for (unsigned i = 0; i < f.rangeCount; ++i) {
        auto &range = f.ranges[i];
        if (!p.expect(4, 2) || !p.uint(range.first, true) || !p.uint(range.last, true))
            return false;
        if (range.first > range.last || range.last >= f.sequence ||
            (i && range.first <= f.ranges[i - 1].last))
            return p.fail(Error::Value);
    }
    if (f.type == 3 ? (!equal(f.subject, f.origin) || f.dependencyCount || !f.rangeCount)
                    : f.rangeCount != 0)
        return p.fail(Error::Value);
    return p.end();
}
struct Part {
    const std::uint8_t *bytes;
    std::size_t count;
};
bool digest(configuration::IPackageDigest &hash, const Part *parts, std::size_t count,
            std::uint8_t out[32]) {
    std::memset(out, 0, 32);
    bool ok = hash.abort();
    if (ok)
        ok = hash.begin();
    for (std::size_t i = 0; ok && i < count; ++i)
        ok = hash.update(parts[i].bytes, parts[i].count);
    std::size_t written = 0;
    if (ok)
        ok = hash.finish(out, written) && written == 32;
    const bool closed = hash.abort();
    if (!ok || !closed) {
        std::memset(out, 0, 32);
        return false;
    }
    return true;
}
std::size_t bstrHead(std::size_t n, std::uint8_t out[3]) {
    if (n < 24) {
        out[0] = static_cast<std::uint8_t>(0x40 + n);
        return 1;
    }
    if (n <= 255) {
        out[0] = 0x58;
        out[1] = static_cast<std::uint8_t>(n);
        return 2;
    }
    out[0] = 0x59;
    out[1] = static_cast<std::uint8_t>(n >> 8);
    out[2] = static_cast<std::uint8_t>(n);
    return 3;
}
} // namespace
Result checkFact(configuration::IPackageByteSource &source, const CandidatePublicKey &key,
                 configuration::IPackageDigest &hash, IFactCrypto &crypto, FrozenFact &out) {
    const CandidatePublicKey candidate = key;
    clear(out);
    Result result;
    for (;;) {
        std::uint8_t byte = 0;
        const auto status = source.readByte(byte);
        if (status == configuration::PackageReadStatus::Failure) {
            result.error = Error::SourceFailure;
            break;
        }
        if (status == configuration::PackageReadStatus::End)
            break;
        ++result.readBytes;
        if (result.readBytes > sizeof(out.original)) {
            result.error = Error::LengthLimit;
            break;
        }
        out.original[out.length++] = byte;
    }
    result.offset = result.readBytes;
    if (result.error == Error::None)
        parse(out, result);
    if (result.error == Error::None && !crypto.reset())
        result.error = Error::CleanupFailure;
    std::uint8_t computed[32] = {};
    if (result.error == Error::None) {
        std::uint8_t publicKey[77] = {0xa5, 0x01, 0x02, 0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20};
        std::memcpy(publicKey + 10, candidate.x, 32);
        publicKey[42] = 0x22;
        publicKey[43] = 0x58;
        publicKey[44] = 0x20;
        std::memcpy(publicKey + 45, candidate.y, 32);
        const Part part = {publicKey, sizeof(publicKey)};
        if (!digest(hash, &part, 1, computed))
            result.error = Error::HashFailure;
        else if (std::memcmp(computed, out.kid.bytes, 32))
            result.error = Error::KeyIdMismatch;
    }
    if (result.error == Error::None) {
        const Part part = {out.original + out.payload.offset, out.payload.length};
        if (!digest(hash, &part, 1, out.payloadDigest.bytes))
            result.error = Error::HashFailure;
    }
    if (result.error == Error::None) {
        static const std::uint8_t prefix[] = {0x84, 0x6a, 'S', 'i', 'g', 'n',
                                              'a',  't',  'u', 'r', 'e', '1'};
        static const std::uint8_t aad[] = {0x4d, 'l', 'a', 'r', 'p', '.', 'e',
                                           'v',  'e', 'n', 't', '.', 'v', '1'};
        std::uint8_t protectedHead[3], payloadHead[3];
        const Part parts[] = {
            {prefix, sizeof(prefix)},
            {protectedHead, bstrHead(out.protectedHeader.length, protectedHead)},
            {out.original + out.protectedHeader.offset, out.protectedHeader.length},
            {aad, sizeof(aad)},
            {payloadHead, bstrHead(out.payload.length, payloadHead)},
            {out.original + out.payload.offset, out.payload.length}};
        if (!digest(hash, parts, 6, computed))
            result.error = Error::HashFailure;
        else {
            switch (crypto.verifyHash(candidate, computed, out.original + out.signature.offset)) {
            case VerifyResult::Valid:
                break;
            case VerifyResult::InvalidSignature:
                result.error = Error::InvalidSignature;
                break;
            case VerifyResult::InvalidKey:
                result.error = Error::InvalidKey;
                break;
            case VerifyResult::Unavailable:
                result.error = Error::CryptoUnavailable;
                break;
            case VerifyResult::CleanupFailure:
                result.error = Error::CleanupFailure;
                break;
            default:
                result.error = Error::CryptoFailure;
                break;
            }
        }
    }
    const bool hashClosed = hash.abort(), cryptoClosed = crypto.reset();
    if (result.error == Error::None && (!hashClosed || !cryptoClosed))
        result.error = Error::CleanupFailure;
    std::memset(computed, 0, sizeof(computed));
    if (result.error == Error::None)
        result.scope = Scope::SignatureCheckedUnauthenticated;
    else
        clear(out);
    return result;
}
} // namespace d0
} // namespace companion
} // namespace larp
