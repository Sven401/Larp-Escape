#include "Companion/D0Fact.h"
#include <algorithm>
#include <cstring>
#include <unity.h>
#include <vector>
using namespace larp::companion::d0;
using namespace larp::configuration;
namespace {
// These tests deliberately use noncryptographic seams. Real signed acceptance
// is independently exercised by the CNG and native PSA fixture targets.
struct Bytes {
    std::vector<std::uint8_t> b;
    void head(unsigned m, std::uint64_t n) {
        if (n < 24) {
            b.push_back(static_cast<std::uint8_t>(m * 32 + n));
            return;
        }
        unsigned count = n <= 255 ? 1 : n <= 65535 ? 2 : n <= UINT32_MAX ? 4 : 8;
        b.push_back(static_cast<std::uint8_t>(m * 32 + (count == 1   ? 24
                                                        : count == 2 ? 25
                                                        : count == 4 ? 26
                                                                     : 27)));
        for (unsigned i = count; i; --i)
            b.push_back(static_cast<std::uint8_t>(n >> (8 * (i - 1))));
    }
    void id(unsigned n) {
        head(2, 16);
        b.insert(b.end(), 15, 0);
        b.push_back(static_cast<std::uint8_t>(n));
    }
    void digest() {
        head(2, 32);
        b.insert(b.end(), 32, 0x42);
    }
    void blob(const Bytes &x) {
        head(2, x.b.size());
        b.insert(b.end(), x.b.begin(), x.b.end());
    }
};
std::vector<std::uint8_t> packet(unsigned type = 1, unsigned deps = 0, unsigned ranges = 0) {
    Bytes p;
    p.head(5, 10);
    p.head(0, 0);
    p.head(0, 1);
    p.head(0, 1);
    p.head(0, 1);
    for (unsigned key = 2; key <= 5; ++key) {
        p.head(0, key);
        p.id(key);
    }
    p.head(0, 6);
    p.head(0, UINT64_MAX);
    p.head(0, 7);
    p.digest();
    p.head(0, 8);
    p.digest();
    p.head(0, 9);
    p.head(4, 7);
    p.id(6);
    p.head(0, UINT64_MAX);
    p.head(0, type);
    p.id(type == 3 ? 4 : 7);
    p.head(7, 21);
    p.head(4, deps);
    for (unsigned i = 0; i < deps; ++i) {
        p.head(4, 4);
        p.id(2);
        p.id(30 - i);
        p.id(6);
        p.head(0, UINT64_MAX - i);
    }
    p.head(4, ranges);
    for (unsigned i = 0; i < ranges; ++i) {
        p.head(4, 2);
        p.head(0, 2 * i + 1);
        p.head(0, 2 * i + 2);
    }
    Bytes h;
    h.head(5, 2);
    h.head(0, 1);
    h.head(1, 6);
    h.head(0, 4);
    h.digest();
    Bytes root;
    root.head(6, 18);
    root.head(4, 4);
    root.blob(h);
    root.head(5, 0);
    root.blob(p);
    root.head(2, 64);
    root.b.insert(root.b.end(), 64, 0x77);
    return root.b;
}
struct Source : IPackageByteSource {
    const std::vector<std::uint8_t> &bytes;
    std::size_t pos = 0, failAt = static_cast<std::size_t>(-1), reads = 0;
    CandidatePublicKey *mutate = nullptr;
    explicit Source(const std::vector<std::uint8_t> &b) : bytes(b) {}
    PackageReadStatus readByte(std::uint8_t &byte) override {
        ++reads;
        if (mutate) {
            mutate->x[0] = 99;
            mutate = nullptr;
        }
        if (pos == failAt)
            return PackageReadStatus::Failure;
        if (pos == bytes.size())
            return PackageReadStatus::End;
        byte = bytes[pos++];
        return PackageReadStatus::Byte;
    }
};
struct Hash : IPackageDigest {
    unsigned begins = 0, updates = 0, finishes = 0, aborts = 0;
    unsigned failBegin = 0, failUpdate = 0, failFinish = 0, failAbort = 0;
    std::size_t finishLength = 32;
    std::vector<std::vector<std::uint8_t>> messages;
    bool begin() override {
        messages.push_back({});
        return ++begins != failBegin;
    }
    bool update(const std::uint8_t *b, std::size_t n) override {
        ++updates;
        if (!messages.empty())
            messages.back().insert(messages.back().end(), b, b + n);
        return updates != failUpdate;
    }
    bool finish(std::uint8_t out[32], std::size_t &written) override {
        std::memset(out, 0x42, 32);
        written = finishLength;
        return ++finishes != failFinish;
    }
    bool abort() override {
        return ++aborts != failAbort;
    }
};
struct Crypto : IFactCrypto {
    unsigned resets = 0, verifies = 0, failReset = 0;
    VerifyResult answer = VerifyResult::Valid;
    CandidatePublicKey seen;
    bool reset() override {
        return ++resets != failReset;
    }
    VerifyResult verifyHash(const CandidatePublicKey &key, const std::uint8_t[32],
                            const std::uint8_t[64]) override {
        seen = key;
        ++verifies;
        return answer;
    }
};
FrozenFact output;
void rejected(const Result &r) {
    TEST_ASSERT_EQUAL(static_cast<int>(Scope::Rejected), static_cast<int>(r.scope));
    TEST_ASSERT_EQUAL_UINT16(0, output.length);
    TEST_ASSERT_TRUE(output.fact.sequence == 0);
    std::uint8_t zero[32] = {};
    TEST_ASSERT_EQUAL_MEMORY(zero, output.payloadDigest.bytes, 32);
    TEST_ASSERT_EQUAL_UINT16(0, output.payload.length);
}
void test_copied_fields_original_slices_and_candidate_freeze_seam() {
    auto bytes = packet(1, 8);
    Source s(bytes);
    Hash h;
    Crypto c;
    CandidatePublicKey key;
    key.x[0] = 7;
    s.mutate = &key;
    const auto r = checkFact(s, key, h, c, output);
    TEST_ASSERT_EQUAL(static_cast<int>(Scope::SignatureCheckedUnauthenticated),
                      static_cast<int>(r.scope));
    TEST_ASSERT_TRUE(output.fact.generation == UINT64_MAX);
    TEST_ASSERT_TRUE(output.fact.sequence == UINT64_MAX);
    TEST_ASSERT_EQUAL_UINT8(8, output.fact.dependencyCount);
    TEST_ASSERT_EQUAL_UINT8(30, output.fact.dependencies[0].origin.bytes[15]);
    TEST_ASSERT_EQUAL_UINT8(23, output.fact.dependencies[7].origin.bytes[15]);
    TEST_ASSERT_EQUAL_UINT8(7, c.seen.x[0]);
    TEST_ASSERT_EQUAL_MEMORY(bytes.data(), output.original, bytes.size());
    TEST_ASSERT_EQUAL_UINT32(bytes.size() + 1, s.reads);
    TEST_ASSERT_EQUAL_UINT32(3, h.messages.size());
    TEST_ASSERT_EQUAL_UINT32(77, h.messages[0].size());
    TEST_ASSERT_EQUAL_MEMORY(bytes.data() + output.payload.offset, h.messages[1].data(),
                             output.payload.length);
    const std::uint8_t prefix[] = {0x84, 0x6a, 'S', 'i', 'g', 'n', 'a', 't', 'u', 'r', 'e', '1'};
    TEST_ASSERT_EQUAL_MEMORY(prefix, h.messages[2].data(), sizeof(prefix));
    const std::uint8_t aad[] = {0x4d, 'l', 'a', 'r', 'p', '.', 'e',
                                'v',  'e', 'n', 't', '.', 'v', '1'};
    TEST_ASSERT_NOT_NULL(std::search(h.messages[2].begin(), h.messages[2].end(), aad,
                                     aad + sizeof(aad)) != h.messages[2].end()
                             ? &h
                             : nullptr);
    std::fill(bytes.begin(), bytes.end(), 0);
    TEST_ASSERT_EQUAL_HEX8(0xd2, output.original[0]);
}
void test_all_source_failures_win_before_crypto_and_clean_reuse() {
    const auto bytes = packet();
    for (std::size_t i = 0; i <= bytes.size(); ++i) {
        Source s(bytes);
        s.failAt = i;
        Hash h;
        Crypto c;
        c.answer = VerifyResult::Failure;
        const auto r = checkFact(s, {}, h, c, output);
        rejected(r);
        TEST_ASSERT_EQUAL(static_cast<int>(Error::SourceFailure), static_cast<int>(r.error));
        TEST_ASSERT_EQUAL_UINT32(i, r.offset);
        TEST_ASSERT_EQUAL_UINT32(0, h.begins);
        TEST_ASSERT_EQUAL_UINT32(0, c.verifies);
        Source good(bytes);
        Hash gh;
        Crypto gc;
        TEST_ASSERT_EQUAL(static_cast<int>(Scope::SignatureCheckedUnauthenticated),
                          static_cast<int>(checkFact(good, {}, gh, gc, output).scope));
    }
}
void test_packet_limit_observes_exact_2048_eof_and_extra_byte() {
    for (unsigned n = 2048; n <= 2049; ++n) {
        std::vector<std::uint8_t> bytes(n, 0);
        Source s(bytes);
        Hash h;
        Crypto c;
        auto r = checkFact(s, {}, h, c, output);
        rejected(r);
        TEST_ASSERT_EQUAL_UINT32(2049, s.reads);
        TEST_ASSERT_EQUAL(static_cast<int>(n == 2049 ? Error::LengthLimit : Error::Cbor),
                          static_cast<int>(r.error));
        Source failure(bytes);
        failure.failAt = 2048;
        Hash fh;
        Crypto fc;
        r = checkFact(failure, {}, fh, fc, output);
        rejected(r);
        TEST_ASSERT_EQUAL(static_cast<int>(Error::SourceFailure), static_cast<int>(r.error));
    }
}
void test_crypto_and_hash_failures_never_publish_stale_values() {
    const auto bytes = packet();
    const VerifyResult answers[] = {VerifyResult::InvalidSignature, VerifyResult::InvalidKey,
                                    VerifyResult::Unavailable, VerifyResult::Failure,
                                    VerifyResult::CleanupFailure};
    const Error expected[] = {Error::InvalidSignature, Error::InvalidKey, Error::CryptoUnavailable,
                              Error::CryptoFailure, Error::CleanupFailure};
    for (unsigned i = 0; i < 5; ++i) {
        Source s(bytes);
        Hash h;
        Crypto c;
        c.answer = answers[i];
        const auto r = checkFact(s, {}, h, c, output);
        rejected(r);
        TEST_ASSERT_EQUAL(static_cast<int>(expected[i]), static_cast<int>(r.error));
    }
    for (unsigned stage = 0; stage < 5; ++stage)
        for (unsigned ordinal = 1; ordinal <= 3; ++ordinal) {
            Source s(bytes);
            Hash h;
            Crypto c;
            if (stage == 0)
                h.failBegin = ordinal;
            if (stage == 1)
                h.failUpdate = ordinal;
            if (stage == 2)
                h.failFinish = ordinal;
            if (stage == 3)
                h.failAbort = ordinal;
            if (stage == 4)
                h.finishLength = 31;
            rejected(checkFact(s, {}, h, c, output));
        }
    for (unsigned i = 1; i <= 2; ++i) {
        Source s(bytes);
        Hash h;
        Crypto c;
        c.failReset = i;
        const auto r = checkFact(s, {}, h, c, output);
        rejected(r);
        TEST_ASSERT_EQUAL(static_cast<int>(Error::CleanupFailure), static_cast<int>(r.error));
    }
}
void test_shortest_structure_ranges_and_eof_precedence() {
    auto bytes = packet(3, 0, 8);
    Source valid(bytes);
    Hash h;
    Crypto c;
    TEST_ASSERT_EQUAL(static_cast<int>(Scope::SignatureCheckedUnauthenticated),
                      static_cast<int>(checkFact(valid, {}, h, c, output).scope));
    TEST_ASSERT_EQUAL_UINT8(8, output.fact.rangeCount);
    bytes.insert(bytes.begin() + 1, 18);
    bytes[0] = 0xd8;
    Source bad(bytes);
    Hash bh;
    Crypto bc;
    rejected(checkFact(bad, {}, bh, bc, output));
    TEST_ASSERT_EQUAL_UINT32(0, bc.verifies);
    Source failure(bytes);
    failure.failAt = bytes.size();
    Hash fh;
    Crypto fc;
    const auto r = checkFact(failure, {}, fh, fc, output);
    rejected(r);
    TEST_ASSERT_EQUAL(static_cast<int>(Error::SourceFailure), static_cast<int>(r.error));
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_copied_fields_original_slices_and_candidate_freeze_seam);
    RUN_TEST(test_all_source_failures_win_before_crypto_and_clean_reuse);
    RUN_TEST(test_packet_limit_observes_exact_2048_eof_and_extra_byte);
    RUN_TEST(test_crypto_and_hash_failures_never_publish_stale_values);
    RUN_TEST(test_shortest_structure_ranges_and_eof_precedence);
    return UNITY_END();
}
