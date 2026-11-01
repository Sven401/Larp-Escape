#include "Companion/D0FactWriter.h"
#include <cstring>
#include <new>

namespace larp {
namespace companion {
namespace d0 {
namespace {
void clear(FrozenFact &value) {
    value.~FrozenFact();
    new (&value) FrozenFact{};
}
bool nonzero(const Identity &id) {
    std::uint8_t bits = 0;
    for (const auto byte : id.bytes)
        bits |= byte;
    return bits != 0;
}
bool equal(const Identity &a, const Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
bool equal(const FactId &a, const FactId &b) {
    return equal(a.event, b.event) && equal(a.origin, b.origin) && equal(a.epoch, b.epoch) &&
           a.sequence == b.sequence;
}
bool valid(const Fact &f) {
    if (!nonzero(f.event) || !nonzero(f.run) || !nonzero(f.origin) || !nonzero(f.station) ||
        !nonzero(f.epoch) || !nonzero(f.subject) || !f.generation || !f.sequence || f.type < 1 ||
        f.type > 3 || f.dependencyCount > 8 || f.rangeCount > 8)
        return false;
    FactId self;
    self.event = f.event;
    self.origin = f.origin;
    self.epoch = f.epoch;
    self.sequence = f.sequence;
    for (unsigned i = 0; i < f.dependencyCount; ++i) {
        const auto &d = f.dependencies[i];
        if (!nonzero(d.event) || !nonzero(d.origin) || !nonzero(d.epoch) || !d.sequence ||
            equal(d, self))
            return false;
        for (unsigned j = 0; j < i; ++j)
            if (equal(d, f.dependencies[j]))
                return false;
    }
    for (unsigned i = 0; i < f.rangeCount; ++i) {
        const auto &r = f.ranges[i];
        if (!r.first || r.first > r.last || r.last >= f.sequence ||
            (i && r.first <= f.ranges[i - 1].last))
            return false;
    }
    return f.type == 3 ? equal(f.subject, f.origin) && !f.dependencyCount && f.rangeCount
                       : !f.rangeCount;
}
struct Encoder {
    std::uint8_t *data;
    std::size_t capacity, count = 0;
    bool good = true;
    Encoder(std::uint8_t *bytes, std::size_t size) : data(bytes), capacity(size) {}
    void byte(std::uint8_t value) {
        if (count == capacity) {
            good = false;
            return;
        }
        if (data)
            data[count] = value;
        ++count;
    }
    void raw(const std::uint8_t *bytes, std::size_t size) {
        if (size > capacity - count) {
            good = false;
            return;
        }
        if (data && size)
            std::memcpy(data + count, bytes, size);
        count += size;
    }
    void head(unsigned major, std::uint64_t value) {
        if (value < 24) {
            byte(static_cast<std::uint8_t>((major << 5) | value));
            return;
        }
        const unsigned size = value <= 255 ? 1 : value <= 65535 ? 2 : value <= UINT32_MAX ? 4 : 8;
        byte(static_cast<std::uint8_t>((major << 5) | (size == 1   ? 24
                                                       : size == 2 ? 25
                                                       : size == 4 ? 26
                                                                   : 27)));
        for (unsigned i = size; i; --i)
            byte(static_cast<std::uint8_t>(value >> ((i - 1) * 8)));
    }
    void blob(const std::uint8_t *bytes, std::size_t size) {
        head(2, size);
        raw(bytes, size);
    }
    void id(const Identity &value) {
        blob(value.bytes, 16);
    }
};
void payload(Encoder &e, const Fact &f) {
    e.head(5, 10);
    e.head(0, 0);
    e.head(0, 1);
    e.head(0, 1);
    e.head(0, 1);
    e.head(0, 2);
    e.id(f.event);
    e.head(0, 3);
    e.id(f.run);
    e.head(0, 4);
    e.id(f.origin);
    e.head(0, 5);
    e.id(f.station);
    e.head(0, 6);
    e.head(0, f.generation);
    e.head(0, 7);
    e.blob(f.binding.bytes, 32);
    e.head(0, 8);
    e.blob(f.package.bytes, 32);
    e.head(0, 9);
    e.head(4, 7);
    e.id(f.epoch);
    e.head(0, f.sequence);
    e.head(0, f.type);
    e.id(f.subject);
    e.byte(0xf5);
    e.head(4, f.dependencyCount);
    for (unsigned i = 0; i < f.dependencyCount; ++i) {
        const auto &d = f.dependencies[i];
        e.head(4, 4);
        e.id(d.event);
        e.id(d.origin);
        e.id(d.epoch);
        e.head(0, d.sequence);
    }
    e.head(4, f.rangeCount);
    for (unsigned i = 0; i < f.rangeCount; ++i) {
        e.head(4, 2);
        e.head(0, f.ranges[i].first);
        e.head(0, f.ranges[i].last);
    }
}
struct Part {
    const std::uint8_t *bytes;
    std::size_t size;
};
bool digest(configuration::IPackageDigest &hash, const Part *parts, std::size_t count,
            std::uint8_t result[32]) {
    bool ok = hash.abort();
    if (ok)
        ok = hash.begin();
    for (std::size_t i = 0; ok && i < count; ++i)
        ok = hash.update(parts[i].bytes, parts[i].size);
    std::size_t written = 0;
    if (ok)
        ok = hash.finish(result, written) && written == 32;
    const bool closed = hash.abort();
    if (!ok || !closed) {
        std::memset(result, 0, 32);
        return false;
    }
    return true;
}
WriteError signingError(SignStatus status) {
    return status == SignStatus::InvalidKey    ? WriteError::InvalidKey
           : status == SignStatus::Unavailable ? WriteError::SignerUnavailable
                                               : WriteError::SigningFailure;
}
class MemorySource final : public configuration::IPackageByteSource {
  public:
    MemorySource(const std::uint8_t *bytes, std::size_t size) : bytes_(bytes), size_(size) {}
    configuration::PackageReadStatus readByte(std::uint8_t &value) override {
        if (offset_ == size_)
            return configuration::PackageReadStatus::End;
        value = bytes_[offset_++];
        return configuration::PackageReadStatus::Byte;
    }

  private:
    const std::uint8_t *bytes_;
    std::size_t size_, offset_ = 0;
};
} // namespace
WriteResult writeFact(const Fact &input, configuration::IPackageDigest &hash, IFactSigner &signer,
                      IFactCrypto &verifier, FactWriteWorkspace &workspace, FrozenFact &out) {
    const Fact frozen = input;
    clear(out);
    std::memset(workspace.bytes, 0, sizeof(workspace.bytes));
    WriteResult result;
    CandidatePublicKey key;
    std::uint8_t computed[32] = {}, kid[32] = {}, signature[64] = {};
    Encoder packet(workspace.bytes, sizeof(workspace.bytes));
    if (!valid(frozen))
        result.error = WriteError::InvalidFact;
    if (result.error == WriteError::None) {
        const auto status = signer.publicKey(key);
        if (status != SignStatus::Success)
            result.error = signingError(status);
    }
    if (result.error == WriteError::None) {
        std::uint8_t encodedKey[77] = {0xa5, 1, 2, 3, 0x26, 0x20, 1, 0x21, 0x58, 0x20};
        std::memcpy(encodedKey + 10, key.x, 32);
        encodedKey[42] = 0x22;
        encodedKey[43] = 0x58;
        encodedKey[44] = 0x20;
        std::memcpy(encodedKey + 45, key.y, 32);
        const Part part = {encodedKey, sizeof(encodedKey)};
        if (!digest(hash, &part, 1, kid))
            result.error = WriteError::HashFailure;
    }
    std::size_t protectedStart = 0, payloadStart = 0, payloadSize = 0, signatureStart = 0;
    if (result.error == WriteError::None) {
        Encoder size(nullptr, sizeof(workspace.bytes));
        payload(size, frozen);
        payloadSize = size.count;
        packet.head(6, 18);
        packet.head(4, 4);
        packet.head(2, 38);
        protectedStart = packet.count;
        packet.head(5, 2);
        packet.head(0, 1);
        packet.head(1, 6);
        packet.head(0, 4);
        packet.blob(kid, 32);
        packet.head(5, 0);
        packet.head(2, payloadSize);
        payloadStart = packet.count;
        payload(packet, frozen);
        packet.head(2, 64);
        signatureStart = packet.count;
        packet.raw(signature, sizeof(signature));
        if (!size.good || !packet.good)
            result.error = WriteError::LengthLimit;
    }
    if (result.error == WriteError::None) {
        static const std::uint8_t prefix[] = {0x84, 0x6a, 'S', 'i', 'g', 'n',
                                              'a',  't',  'u', 'r', 'e', '1'};
        static const std::uint8_t aad[] = {0x4d, 'l', 'a', 'r', 'p', '.', 'e',
                                           'v',  'e', 'n', 't', '.', 'v', '1'};
        std::uint8_t protectedHead[9] = {}, payloadHead[9] = {};
        Encoder ph(protectedHead, sizeof(protectedHead)), pl(payloadHead, sizeof(payloadHead));
        ph.head(2, 38);
        pl.head(2, payloadSize);
        const Part parts[] = {{prefix, sizeof(prefix)},
                              {protectedHead, ph.count},
                              {workspace.bytes + protectedStart, 38},
                              {aad, sizeof(aad)},
                              {payloadHead, pl.count},
                              {workspace.bytes + payloadStart, payloadSize}};
        if (!digest(hash, parts, 6, computed))
            result.error = WriteError::HashFailure;
        else {
            std::size_t written = 0;
            const auto status = signer.signHash(key, computed, signature, written);
            if (status != SignStatus::Success)
                result.error = signingError(status);
            else if (written != 64)
                result.error = WriteError::SignatureLength;
            else
                std::memcpy(workspace.bytes + signatureStart, signature, 64);
        }
    }
    if (result.error == WriteError::None) {
        MemorySource source(workspace.bytes, packet.count);
        result.verification = checkFact(source, key, hash, verifier, out);
        if (result.verification.scope != Scope::SignatureCheckedUnauthenticated)
            result.error = WriteError::VerificationFailure;
    }
    const bool hashClosed = hash.abort(), cryptoClosed = verifier.reset();
    if (result.error == WriteError::None && (!hashClosed || !cryptoClosed))
        result.error = WriteError::CleanupFailure;
    std::memset(computed, 0, sizeof(computed));
    std::memset(signature, 0, sizeof(signature));
    if (result.error == WriteError::None) {
        result.scope = WriteScope::SignedUnpublished;
        result.publicKey = key;
    } else {
        result.verification.scope = Scope::Rejected;
        clear(out);
        std::memset(workspace.bytes, 0, sizeof(workspace.bytes));
    }
    return result;
}
} // namespace d0
} // namespace companion
} // namespace larp
