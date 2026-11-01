// Closed-boundary storage tests. No positive setup trust anchor or event grant.
#include "DeviceRuntime/EventAuthorityStorage.h"
#include <cstring>
#include <unity.h>
using namespace larp::devices::event_authority;
namespace {
Identity id(unsigned n) {
    Identity v;
    v.bytes[15] = static_cast<std::uint8_t>(n);
    return v;
}
struct Hash final : larp::configuration::IPackageDigest {
    bool begin() override {
        return true;
    }
    bool update(const std::uint8_t *, std::size_t) override {
        return true;
    }
    bool finish(std::uint8_t *p, std::size_t &n) override {
        std::memset(p, 0, 32);
        n = 32;
        return true;
    }
    bool abort() override {
        return true;
    }
};
struct NoAuthorization final : IOriginalAuthorization {
    unsigned calls = 0;
    Proof verify(const Snapshot &, const std::uint8_t *, std::size_t) override {
        ++calls;
        return Proof::Unavailable;
    }
};
struct Disk {
    Record bytes;
    bool present = false;
};
struct Store final : IStore {
    Disk &disk;
    bool closed = false, uncertain = false, badReadback = false;
    unsigned writes = 0;
    explicit Store(Disk &d) : disk(d) {}
    bool fenced() const override {
        return closed;
    }
    Read read(Record &r) override {
        if (closed)
            return Read::Failure;
        if (!disk.present)
            return Read::Missing;
        r = disk.bytes;
        if (badReadback)
            r.bytes[0] ^= 1;
        return Read::Present;
    }
    Write replace(const Record *expected, const Record &next) override {
        if (closed)
            return Write::Failure;
        if ((expected && !disk.present) || (!expected && disk.present) ||
            (expected && expected->bytes != disk.bytes.bytes))
            return Write::Conflict;
        ++writes;
        disk.bytes = next;
        disk.present = true;
        return uncertain ? Write::Uncertain : Write::Confirmed;
    }
    void fence() override {
        closed = true;
    }
};
void put(std::uint8_t *p, std::uint64_t n, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
        p[i] = static_cast<std::uint8_t>(n >> (i * 8));
}
void fixCrc(Record &r) {
    std::uint32_t n = UINT32_MAX;
    for (unsigned i = 0; i < 2196; ++i) {
        n ^= r.bytes[i];
        for (unsigned j = 0; j < 8; ++j)
            n = (n >> 1) ^ ((n & 1) ? 0xedb88320u : 0u);
    }
    put(r.bytes.data() + 2196, n ^ UINT32_MAX, 4);
}
struct Fixture {
    Disk disk;
    Store store;
    Hash hash;
    NoAuthorization proof;
    Workspace work;
    Storage storage;
    Fixture() : store(disk), storage(store, hash, proof, work) {}
};
void test_missing_and_durable_closed_record_never_open_event_rights() {
    Fixture f;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Outcome::Missing),
                          static_cast<int>(f.storage.load(id(1)).outcome));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Outcome::Closed),
                          static_cast<int>(f.storage.close(0).outcome));
    TEST_ASSERT_EQUAL(1, f.store.writes);
    TEST_ASSERT_EQUAL_MEMORY("LEVTA001", f.disk.bytes.bytes.data(), 8);
    Snapshot s;
    TEST_ASSERT_TRUE(f.storage.snapshot(s));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Phase::Closed), static_cast<int>(s.phase));
    TEST_ASSERT_EQUAL(1, s.generation);
    Storage restarted(f.store, f.hash, f.proof, f.work);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Outcome::Closed),
                          static_cast<int>(restarted.load(id(1)).outcome));
    TEST_ASSERT_EQUAL(0, f.proof.calls);
    TEST_ASSERT_EQUAL(1, f.store.writes);
}
void test_unknown_closes_all_owners_on_same_session_until_true_reopen() {
    Fixture f;
    f.storage.load(id(1));
    f.store.uncertain = true;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Outcome::Unknown),
                          static_cast<int>(f.storage.close(0).outcome));
    TEST_ASSERT_TRUE(f.store.fenced());
    Storage replacement(f.store, f.hash, f.proof, f.work);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Outcome::Rejected),
                          static_cast<int>(replacement.load(id(1)).outcome));
    Snapshot s;
    TEST_ASSERT_FALSE(replacement.snapshot(s));
    // Explicit new backend session over actual durable bytes, not owner reset.
    Store reopened(f.disk);
    Workspace fresh;
    Storage recovered(reopened, f.hash, f.proof, fresh);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Outcome::Closed),
                          static_cast<int>(recovered.load(id(1)).outcome));
    TEST_ASSERT_TRUE(recovered.snapshot(s));
}
void test_canonical_record_context_and_cas_fail_closed() {
    Fixture f;
    f.storage.load(id(1));
    f.storage.close(0);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Conflict),
                          static_cast<int>(f.storage.close(4).error));
    TEST_ASSERT_EQUAL(1, f.store.writes);
    f.disk.bytes.bytes[10] = 1;
    fixCrc(f.disk.bytes);
    Workspace next;
    Storage corrupt(f.store, f.hash, f.proof, next);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Canonical),
                          static_cast<int>(corrupt.load(id(1)).error));
    TEST_ASSERT_TRUE(f.store.closed);
    Fixture other;
    other.storage.load(id(2));
    other.storage.close(0);
    Workspace w;
    Storage wrong(other.store, other.hash, other.proof, w);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Context),
                          static_cast<int>(wrong.load(id(1)).error));
}
void test_actual_bound_bytes_without_original_authorization_remain_closed() {
    Fixture f;
    f.disk.present = true;
    auto &raw = f.disk.bytes.bytes;
    std::memcpy(raw.data(), "LEVTA001", 8);
    raw[8] = 2;
    put(raw.data() + 16, 1, 8);
    const Identity box = id(1), event = id(2);
    std::memcpy(raw.data() + 24, box.bytes, 16);
    std::memcpy(raw.data() + 40, event.bytes, 16);
    put(raw.data() + 56, 1, 4);
    put(raw.data() + 124, 1, 2);
    raw[148] = 17;
    fixCrc(f.disk.bytes);
    const auto checked = f.storage.load(box);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Outcome::Closed), static_cast<int>(checked.outcome));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Unavailable), static_cast<int>(checked.error));
    TEST_ASSERT_EQUAL(1, f.proof.calls);
    Snapshot s;
    TEST_ASSERT_FALSE(f.storage.snapshot(s));
    TEST_ASSERT_EQUAL(0, f.store.writes);
}
void test_failed_readback_closes_session_and_publishes_nothing() {
    Fixture f;
    f.storage.load(id(1));
    f.store.badReadback = true;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Outcome::Unknown),
                          static_cast<int>(f.storage.close(0).outcome));
    Snapshot s;
    TEST_ASSERT_FALSE(f.storage.snapshot(s));
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_missing_and_durable_closed_record_never_open_event_rights);
    RUN_TEST(test_unknown_closes_all_owners_on_same_session_until_true_reopen);
    RUN_TEST(test_canonical_record_context_and_cas_fail_closed);
    RUN_TEST(test_actual_bound_bytes_without_original_authorization_remain_closed);
    RUN_TEST(test_failed_readback_closes_session_and_publishes_nothing);
    return UNITY_END();
}
