#include "DeviceRuntime/ProgressJournal.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <unity.h>
#include <vector>

using namespace larp::devices;
namespace {
// Byte-addressed NOR fault model: program can only clear bits. Failed program
// may persist a prefix; failed sync may leave all bytes visible. Reopening uses
// the same modeled durable bytes. This is not a physical power-loss proof.
struct NorStore : IProgressStore {
    std::vector<std::uint8_t> bytes;
    std::size_t reads = 0, programs = 0, syncs = 0, programmed = 0;
    std::size_t readFailure = 0, syncFailure = 0;
    std::size_t budget = std::numeric_limits<std::size_t>::max();
    bool online = true;
    NorStore() : bytes(ProgressLimits::StorageBytes, 0xff) {}
    std::size_t capacity() const override {
        return bytes.size();
    }
    bool read(std::size_t offset, std::uint8_t *out, std::size_t count) override {
        ++reads;
        if (!online || (readFailure && reads == readFailure) || offset > bytes.size() ||
            count > bytes.size() - offset)
            return false;
        std::memcpy(out, bytes.data() + offset, count);
        return true;
    }
    bool program(std::size_t offset, const std::uint8_t *in, std::size_t count) override {
        ++programs;
        if (!online || offset > bytes.size() || count > bytes.size() - offset)
            return false;
        const std::size_t written = std::min(count, budget);
        for (std::size_t i = 0; i < written; ++i) {
            if (bytes[offset + i] != 0xff || (bytes[offset + i] & in[i]) != in[i])
                return false;
            bytes[offset + i] &= in[i];
            ++programmed;
        }
        budget -= written;
        return written == count;
    }
    bool sync() override {
        ++syncs;
        return online && (!syncFailure || syncs != syncFailure);
    }
    void clearFaults() {
        readFailure = syncFailure = 0;
        budget = std::numeric_limits<std::size_t>::max();
        online = true;
    }
};
struct Fence : IProgressCommitFence {
    bool permitted = true;
    std::size_t calls = 0;
    ProgressRecord seen;
    bool allow(const ProgressRecord &record) override {
        ++calls;
        seen = record;
        return permitted;
    }
};
ProgressOrigin origin() {
    ProgressOrigin value;
    for (std::size_t i = 0; i < value.event.size(); ++i) {
        value.event[i] = static_cast<std::uint8_t>(0x10 + i);
        value.box[i] = static_cast<std::uint8_t>(0x30 + i);
        value.epoch[i] = static_cast<std::uint8_t>(0x50 + i);
    }
    return value;
}
const std::uint8_t Body[] = {0x01, 0x82, 0x00, 0xff, 0x25, 0x03};
ProgressRequest request(std::uint8_t id = 1, std::uint64_t revision = 0, std::uint64_t issued = 0) {
    ProgressRequest value;
    value.operationId.fill(id);
    value.requestDigest.fill(static_cast<std::uint8_t>(id + 1));
    value.expectedRevision = revision;
    value.expectedLastIssued = issued;
    value.profileRevision = 12;
    value.trustRevision = 34;
    value.payload = Body;
    value.payloadBytes = sizeof(Body);
    return value;
}
void equal64(std::uint64_t expected, std::uint64_t actual) {
    TEST_ASSERT_TRUE_MESSAGE(expected == actual, "64-bit values differ");
}
void outcome(ProgressOutcome expected, const ProgressResult &result) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(expected), static_cast<int>(result.outcome),
                                  progressOutcomeName(result.outcome));
}
void initialized(ProgressJournal &journal) {
    outcome(ProgressOutcome::Initialized, journal.initialize(origin()));
    TEST_ASSERT_TRUE(journal.snapshot().ready);
}
void sameBytes(const std::vector<std::uint8_t> &a, const NorStore &store) {
    TEST_ASSERT_EQUAL(a.size(), store.bytes.size());
    TEST_ASSERT_TRUE(a == store.bytes);
}
std::uint32_t crc(const std::uint8_t *data, std::size_t size) {
    std::uint32_t value = 0xffffffffU;
    for (std::size_t i = 0; i < size; ++i) {
        value ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1U) ? 0xedb88320U : 0U);
    }
    return ~value;
}
void put64(std::vector<std::uint8_t> &bytes, std::size_t at, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
void repairCrc(NorStore &store, std::size_t slot) {
    const std::size_t begin = slot * ProgressLimits::SlotBytes;
    auto headerBytes = store.bytes;
    if (slot == 0)
        std::fill(headerBytes.begin() + 80, headerBytes.begin() + 143, 0xff);
    const std::uint32_t value = crc(headerBytes.data() + begin, 4088);
    for (unsigned i = 0; i < 4; ++i)
        store.bytes[begin + 4088 + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
void closed(ProgressJournal &journal) {
    TEST_ASSERT_FALSE(journal.snapshot().ready);
    Fence fence;
    const auto rejected = journal.commit(request(99), fence);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(ProgressOutcome::Committed),
                          static_cast<int>(rejected.outcome));
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(ProgressOutcome::Duplicate),
                          static_cast<int>(rejected.outcome));
    TEST_ASSERT_EQUAL(0, fence.calls);
}

void test_explicit_creation_and_nonempty_rejection() {
    NorStore store;
    ProgressJournal journal(store);
    outcome(ProgressOutcome::Empty, journal.recover(origin()));
    closed(journal);
    TEST_ASSERT_EQUAL(0, store.programs);
    ProgressOrigin invalid;
    outcome(ProgressOutcome::Invalid, journal.initialize(invalid));
    initialized(journal);
    const auto saved = store.bytes;
    const auto again = journal.initialize(origin());
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(ProgressOutcome::Initialized),
                          static_cast<int>(again.outcome));
    sameBytes(saved, store);
    NorStore dirty;
    dirty.bytes.back() = 0xfe;
    const auto dirtySaved = dirty.bytes;
    ProgressJournal blocked(dirty);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(ProgressOutcome::Initialized),
                          static_cast<int>(blocked.initialize(origin()).outcome));
    sameBytes(dirtySaved, dirty);
    TEST_ASSERT_EQUAL(0, dirty.programs);
}
void test_fixed_header_and_record_bytes() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    const std::uint8_t header[] = {'L', 'A', 'R', 'P', 'D', '1', 'H', '1', 1, 0};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(header, store.bytes.data(), sizeof(header));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(origin().event.data(), store.bytes.data() + 32, 16);
    const std::uint8_t marker[] = {'D', 'O', 'N', 'E'};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(marker, store.bytes.data() + 4092, 4);
    Fence fence;
    auto value = request();
    value.kind = ProgressKind::Application;
    value.issueCount = 2;
    outcome(ProgressOutcome::Committed, journal.commit(value, fence));
    const std::uint8_t record[] = {'L', 'A', 'R', 'P', 'D', '1', 'R', '1',
                                   1,   0,   2,   2,   6,   0,   255, 255};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(record, store.bytes.data() + 4096, sizeof(record));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(Body, store.bytes.data() + 4096 + 128, sizeof(Body));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(marker, store.bytes.data() + 8192 - 4, 4);
    equal64(1, fence.seen.firstIssued);
    equal64(2, fence.seen.lastIssued);
}
void test_restart_sequence_and_record_reconstruction() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    auto first = request();
    first.issueCount = 2;
    first.kind = ProgressKind::Application;
    auto result = journal.commit(first, fence);
    outcome(ProgressOutcome::Committed, result);
    equal64(1, result.revision);
    equal64(1, result.firstIssued);
    equal64(2, result.lastIssued);
    auto second = request(2, 1, 2);
    result = journal.commit(second, fence);
    outcome(ProgressOutcome::Committed, result);
    equal64(0, result.firstIssued);
    equal64(0, result.lastIssued);
    equal64(2, journal.snapshot().lastIssued);
    ProgressJournal reopened(store);
    outcome(ProgressOutcome::Recovered, reopened.recover(origin()));
    equal64(2, reopened.snapshot().revision);
    equal64(2, reopened.snapshot().lastIssued);
    std::array<std::uint8_t, 32> payload{};
    ProgressRecord row;
    outcome(ProgressOutcome::Read, reopened.readRecord(0, row, payload.data(), payload.size()));
    equal64(1, row.revision);
    equal64(2, row.lastIssued);
    equal64(12, row.profileRevision);
    equal64(34, row.trustRevision);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(Body, payload.data(), sizeof(Body));
    auto third = request(3, 2, 2);
    third.issueCount = 1;
    result = reopened.commit(third, fence);
    outcome(ProgressOutcome::Committed, result);
    equal64(3, result.firstIssued);
    equal64(3, result.lastIssued);
}
void test_lost_response_and_exact_replay_do_not_write_or_call_fence() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    auto value = request();
    value.issueCount = 3;
    outcome(ProgressOutcome::Committed, journal.commit(value, fence));
    const auto saved = store.bytes;
    const auto programs = store.programs;
    const auto syncs = store.syncs;
    fence.permitted = false;
    outcome(ProgressOutcome::Duplicate, journal.commit(value, fence));
    TEST_ASSERT_EQUAL(1, fence.calls);
    ProgressJournal reopened(store);
    outcome(ProgressOutcome::Recovered, reopened.recover(origin()));
    const auto again = reopened.commit(value, fence);
    outcome(ProgressOutcome::Duplicate, again);
    equal64(1, again.firstIssued);
    equal64(3, again.lastIssued);
    equal64(1, reopened.snapshot().revision);
    TEST_ASSERT_EQUAL(1, fence.calls);
    TEST_ASSERT_EQUAL(programs, store.programs);
    TEST_ASSERT_EQUAL(syncs, store.syncs);
    sameBytes(saved, store);
}
void test_replay_compares_all_bindings_and_payload_not_only_digest() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    const auto original = request();
    outcome(ProgressOutcome::Committed, journal.commit(original, fence));
    const auto saved = store.bytes;
    std::uint8_t changed[sizeof(Body)];
    std::memcpy(changed, Body, sizeof(Body));
    changed[3] ^= 1;
    for (unsigned variation = 0; variation < 9; ++variation) {
        auto different = original;
        switch (variation) {
        case 0:
            different.requestDigest[0] ^= 1;
            break;
        case 1:
            different.kind = ProgressKind::Application;
            break;
        case 2:
            ++different.profileRevision;
            break;
        case 3:
            ++different.trustRevision;
            break;
        case 4:
            ++different.expectedRevision;
            break;
        case 5:
            ++different.expectedLastIssued;
            break;
        case 6:
            ++different.issueCount;
            break;
        case 7:
            different.payload = changed;
            break;
        default:
            --different.payloadBytes;
            break;
        }
        outcome(ProgressOutcome::Conflict, journal.commit(different, fence));
        sameBytes(saved, store);
    }
    TEST_ASSERT_EQUAL(1, fence.calls);
}
void test_received_application_and_effect_records_remain_distinct() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    const ProgressKind kinds[] = {ProgressKind::Received, ProgressKind::Application,
                                  ProgressKind::EffectClaim, ProgressKind::EffectResult,
                                  ProgressKind::Control};
    for (std::uint8_t i = 0; i < 5; ++i) {
        auto value = request(static_cast<std::uint8_t>(i + 1), i, 0);
        value.kind = kinds[i];
        outcome(ProgressOutcome::Committed, journal.commit(value, fence));
    }
    ProgressJournal reopened(store);
    outcome(ProgressOutcome::Recovered, reopened.recover(origin()));
    for (std::uint8_t i = 0; i < 5; ++i) {
        ProgressRecord row;
        std::uint8_t bytes[sizeof(Body)] = {};
        outcome(ProgressOutcome::Read, reopened.readRecord(i, row, bytes, sizeof(bytes)));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(kinds[i]), static_cast<int>(row.kind));
    }
    // Storage reports persisted claims; whether to resume as unknown is the
    // future reducer's obligation. No physical action is executed by this API.
}
void test_origin_and_geometry_are_strict() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    const auto saved = store.bytes;
    for (unsigned field = 0; field < 3; ++field) {
        auto other = origin();
        if (field == 0)
            other.event[0] ^= 1;
        if (field == 1)
            other.box[0] ^= 1;
        if (field == 2)
            other.epoch[0] ^= 1;
        ProgressJournal wrong(store);
        outcome(ProgressOutcome::WrongOrigin, wrong.recover(other));
        closed(wrong);
        sameBytes(saved, store);
    }
    NorStore shortStore;
    shortStore.bytes.pop_back();
    ProgressJournal small(shortStore);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(ProgressOutcome::Initialized),
                          static_cast<int>(small.initialize(origin()).outcome));
    TEST_ASSERT_EQUAL(0, shortStore.programs);
}
void test_request_limits_staleness_and_no_rejected_writes() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    const auto saved = store.bytes;
    const auto programs = store.programs;
    for (unsigned variation = 0; variation < 5; ++variation) {
        auto value = request();
        switch (variation) {
        case 0:
            value.operationId.fill(0);
            break;
        case 1:
            value.kind = static_cast<ProgressKind>(255);
            break;
        case 2:
            value.issueCount = ProgressLimits::FactsPerCommit + 1;
            break;
        case 3:
            value.payloadBytes = ProgressLimits::PayloadBytes + 1;
            break;
        case 4:
            value.payload = nullptr;
            break;
        }
        outcome(ProgressOutcome::Invalid, journal.commit(value, fence));
    }
    for (unsigned field = 0; field < 3; ++field) {
        auto value = request();
        if (field == 0)
            ++value.expectedRevision;
        if (field == 1)
            ++value.expectedLastIssued;
        if (field == 2)
            value.expectedLastIssued = std::numeric_limits<std::uint64_t>::max();
        outcome(ProgressOutcome::Stale, journal.commit(value, fence));
    }
    TEST_ASSERT_EQUAL(0, fence.calls);
    TEST_ASSERT_EQUAL(programs, store.programs);
    sameBytes(saved, store);
}
void test_zero_digest_and_empty_payload_are_valid_opaque_values() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    auto value = request();
    value.requestDigest.fill(0);
    value.payload = nullptr;
    value.payloadBytes = 0;
    outcome(ProgressOutcome::Committed, journal.commit(value, fence));
    ProgressJournal reopened(store);
    outcome(ProgressOutcome::Recovered, reopened.recover(origin()));
    outcome(ProgressOutcome::Duplicate, reopened.commit(value, fence));
    ProgressRecord row;
    outcome(ProgressOutcome::Read, reopened.readRecord(0, row, nullptr, 0));
    TEST_ASSERT_EQUAL(0, row.payloadBytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(value.requestDigest.data(), row.requestDigest.data(), 32);
}
void test_full_capacity_preserves_all_records_and_replay() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    std::array<std::uint8_t, ProgressLimits::PayloadBytes> large;
    large.fill(0xa5);
    for (std::uint8_t i = 0; i < ProgressLimits::Records; ++i) {
        auto value = request(static_cast<std::uint8_t>(i + 1), i, i * 8);
        value.issueCount = 8;
        value.payload = large.data();
        value.payloadBytes = large.size();
        outcome(ProgressOutcome::Committed, journal.commit(value, fence));
    }
    equal64(504, journal.snapshot().lastIssued);
    TEST_ASSERT_EQUAL(63, journal.snapshot().records);
    const auto saved = store.bytes;
    const auto programs = store.programs;
    outcome(ProgressOutcome::Full, journal.commit(request(100, 63, 504), fence));
    ProgressJournal reopened(store);
    outcome(ProgressOutcome::Recovered, reopened.recover(origin()));
    auto replay = request(1);
    replay.issueCount = 8;
    replay.payload = large.data();
    replay.payloadBytes = large.size();
    outcome(ProgressOutcome::Duplicate, reopened.commit(replay, fence));
    TEST_ASSERT_EQUAL(programs, store.programs);
    sameBytes(saved, store);
}
void test_fence_rejection_and_reentrant_calls() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence denied;
    denied.permitted = false;
    const auto saved = store.bytes;
    outcome(ProgressOutcome::Denied, journal.commit(request(), denied));
    sameBytes(saved, store);
    struct Reentrant : IProgressCommitFence {
        ProgressJournal &journal;
        explicit Reentrant(ProgressJournal &j) : journal(j) {}
        bool allow(const ProgressRecord &) override {
            Fence inner;
            outcome(ProgressOutcome::Busy, journal.commit(request(2), inner));
            outcome(ProgressOutcome::Busy, journal.recover(origin()));
            outcome(ProgressOutcome::Busy, journal.initialize(origin()));
            return true;
        }
    } reentrant(journal);
    outcome(ProgressOutcome::Committed, journal.commit(request(), reentrant));
    equal64(1, journal.snapshot().revision);
}
void test_read_bounds_failure_does_not_publish_partial_payload() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    outcome(ProgressOutcome::Committed, journal.commit(request(), fence));
    std::array<std::uint8_t, 32> bytes;
    bytes.fill(0xcc);
    ProgressRecord row;
    row.revision = 99;
    outcome(ProgressOutcome::Invalid, journal.readRecord(0, row, bytes.data(), sizeof(Body) - 1));
    equal64(0, row.revision);
    for (auto b : bytes)
        TEST_ASSERT_EQUAL_HEX8(0xcc, b);
    outcome(ProgressOutcome::Invalid, journal.readRecord(1, row, bytes.data(), bytes.size()));
    store.readFailure = store.reads + 1;
    outcome(ProgressOutcome::IoFailure, journal.readRecord(0, row, bytes.data(), bytes.size()));
    equal64(0, row.revision);
    for (auto b : bytes)
        TEST_ASSERT_EQUAL_HEX8(0xcc, b);
    closed(journal);
    store.clearFaults();
    outcome(ProgressOutcome::IoFailure, journal.recover(origin()));
    ProgressJournal reopened(store);
    outcome(ProgressOutcome::Recovered, reopened.recover(origin()));
}
void test_missing_or_unreadable_store_never_creates_progress() {
    NorStore store;
    store.online = false;
    ProgressJournal missing(store);
    outcome(ProgressOutcome::IoFailure, missing.recover(origin()));
    closed(missing);
    TEST_ASSERT_EQUAL(0, store.programs);
    NorStore failed;
    failed.readFailure = 64;
    ProgressJournal opening(failed);
    outcome(ProgressOutcome::IoFailure, opening.initialize(origin()));
    TEST_ASSERT_EQUAL(0, failed.programs);
}
void test_corruption_unknown_version_hole_and_tail_reject_all_progress() {
    NorStore base;
    ProgressJournal journal(base);
    initialized(journal);
    Fence fence;
    outcome(ProgressOutcome::Committed, journal.commit(request(), fence));
    outcome(ProgressOutcome::Committed, journal.commit(request(2, 1), fence));
    const std::size_t corruptionOffsets[] = {
        0,           32,          4088,
        4092,        4096 + 128,  8192 + 128,
        8192 + 4088, 8192 + 4092, ProgressLimits::StorageBytes - 1};
    for (const auto offset : corruptionOffsets) {
        NorStore broken = base;
        broken.bytes[offset] ^= 1;
        ProgressJournal reopen(broken);
        const auto result = reopen.recover(origin());
        TEST_ASSERT_NOT_EQUAL(static_cast<int>(ProgressOutcome::Recovered),
                              static_cast<int>(result.outcome));
        closed(reopen);
    }
    NorStore version = base;
    version.bytes[8] = 2;
    repairCrc(version, 0);
    ProgressJournal unsupported(version);
    outcome(ProgressOutcome::Unsupported, unsupported.recover(origin()));
    closed(unsupported);
    NorStore hole = base;
    std::fill(hole.bytes.begin() + 4096, hole.bytes.begin() + 8192, 0xff);
    ProgressJournal gap(hole);
    outcome(ProgressOutcome::Corrupt, gap.recover(origin()));
    closed(gap);
}
void test_forged_allocator_and_predecessor_do_not_pass_crc_alone() {
    NorStore base;
    ProgressJournal journal(base);
    initialized(journal);
    Fence fence;
    auto value = request();
    value.issueCount = 1;
    outcome(ProgressOutcome::Committed, journal.commit(value, fence));
    for (unsigned field = 0; field < 4; ++field) {
        NorStore broken = base;
        if (field == 0)
            put64(broken.bytes, 4096 + 24, std::numeric_limits<std::uint64_t>::max());
        if (field == 1)
            put64(broken.bytes, 4096 + 16, std::numeric_limits<std::uint64_t>::max());
        if (field == 2)
            broken.bytes[4096 + 112] ^= 1;
        if (field == 3)
            broken.bytes[4096 + 14] = 0;
        repairCrc(broken, 1);
        ProgressJournal reopen(broken);
        outcome(ProgressOutcome::Corrupt, reopen.recover(origin()));
        closed(reopen);
    }
}
void test_entire_confirmed_tail_loss_never_reuses_sequence() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    auto first = request();
    first.issueCount = 2;
    outcome(ProgressOutcome::Committed, journal.commit(first, fence));
    auto second = request(2, 1, 2);
    second.issueCount = 3;
    outcome(ProgressOutcome::Committed, journal.commit(second, fence));
    // A lost newest slot is indistinguishable from unused space without the
    // separate durable reservation. The earlier valid record is no fallback.
    std::fill(store.bytes.begin() + 8192, store.bytes.begin() + 12288, 0xff);
    ProgressJournal reboot(store);
    outcome(ProgressOutcome::Corrupt, reboot.recover(origin()));
    const auto writes = store.programs;
    closed(reboot);
    TEST_ASSERT_EQUAL(writes, store.programs);
    // Even a consumer that had opened before the corruption must rescan.
    std::uint8_t bytes[sizeof(Body)];
    std::fill(bytes, bytes + sizeof(bytes), 0xcc);
    ProgressRecord row;
    outcome(ProgressOutcome::Corrupt, journal.readRecord(0, row, bytes, sizeof(bytes)));
    equal64(0, row.revision);
    for (auto byte : bytes)
        TEST_ASSERT_EQUAL_HEX8(0xcc, byte);
}
void test_live_rollback_to_earlier_valid_image_is_rejected() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    Fence fence;
    auto first = request();
    first.issueCount = 1;
    outcome(ProgressOutcome::Committed, journal.commit(first, fence));
    const auto earlier = store.bytes;
    outcome(ProgressOutcome::Committed, journal.commit(request(2, 1, 1), fence));
    store.bytes = earlier;
    const auto writes = store.programs;
    outcome(ProgressOutcome::Corrupt, journal.commit(request(3, 2, 1), fence));
    closed(journal);
    TEST_ASSERT_EQUAL(writes, store.programs);
    // A complete older image after process loss has no trusted monotonic
    // anchor in this primitive. Deliberate restore needs new D0 incarnation.
}
void test_fence_sees_frozen_bytes_and_metadata() {
    NorStore store;
    ProgressJournal journal(store);
    initialized(journal);
    std::uint8_t payload[sizeof(Body)];
    std::memcpy(payload, Body, sizeof(Body));
    auto value = request();
    value.payload = payload;
    value.issueCount = 1;
    const auto original = value;
    struct Mutating : IProgressCommitFence {
        ProgressRequest &request;
        std::uint8_t *bytes;
        Mutating(ProgressRequest &r, std::uint8_t *p) : request(r), bytes(p) {}
        bool allow(const ProgressRecord &record) override {
            equal64(1, record.firstIssued);
            request.operationId.fill(0x55);
            request.kind = ProgressKind::Control;
            request.issueCount = 8;
            std::fill(bytes, bytes + sizeof(Body), 0xa5);
            return true;
        }
    } fence(value, payload);
    outcome(ProgressOutcome::Committed, journal.commit(value, fence));
    ProgressRecord row;
    std::uint8_t readback[sizeof(Body)] = {};
    outcome(ProgressOutcome::Read, journal.readRecord(0, row, readback, sizeof(readback)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original.operationId.data(), row.operationId.data(), 16);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProgressKind::Received), static_cast<int>(row.kind));
    equal64(1, row.lastIssued);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(Body, readback, sizeof(Body));
}
void test_each_recovery_read_failure_blocks_without_writes() {
    NorStore base;
    ProgressJournal journal(base);
    initialized(journal);
    Fence fence;
    outcome(ProgressOutcome::Committed, journal.commit(request(), fence));
    for (std::size_t call = 1; call <= ProgressLimits::Slots; ++call) {
        NorStore store = base;
        store.readFailure = store.reads + call;
        const auto writes = store.programs;
        const auto saved = store.bytes;
        ProgressJournal reboot(store);
        outcome(ProgressOutcome::IoFailure, reboot.recover(origin()));
        closed(reboot);
        TEST_ASSERT_EQUAL(writes, store.programs);
        sameBytes(saved, store);
    }
}
void test_initialization_sync_failure_is_not_automatic_repair() {
    for (unsigned stage = 1; stage <= 2; ++stage) {
        NorStore store;
        store.syncFailure = stage;
        ProgressJournal creating(store);
        outcome(ProgressOutcome::WriteUncertain, creating.initialize(origin()));
        closed(creating);
        store.clearFaults();
        outcome(ProgressOutcome::WriteUncertain, creating.initialize(origin()));
        ProgressJournal reboot(store);
        outcome(stage == 1 ? ProgressOutcome::Corrupt : ProgressOutcome::Recovered,
                reboot.recover(origin()));
        if (stage == 1)
            closed(reboot);
        else
            equal64(0, reboot.snapshot().lastIssued);
    }
}
void test_every_header_program_prefix_is_closed_or_empty() {
    // Every byte boundary before the entire 4096-byte header is programmed.
    for (std::size_t cut = 0; cut < ProgressLimits::SlotBytes; ++cut) {
        NorStore store;
        store.budget = cut;
        ProgressJournal creating(store);
        outcome(ProgressOutcome::WriteUncertain, creating.initialize(origin()));
        closed(creating);
        store.clearFaults();
        outcome(ProgressOutcome::WriteUncertain, creating.recover(origin()));
        ProgressJournal reboot(store);
        const auto reopened = reboot.recover(origin());
        if (cut == 0)
            outcome(ProgressOutcome::Empty, reopened);
        else
            TEST_ASSERT_TRUE(reopened.outcome == ProgressOutcome::Corrupt ||
                             reopened.outcome == ProgressOutcome::Unsupported);
        closed(reboot);
    }
}
void test_every_record_program_prefix_never_rolls_back_a_dirty_tail() {
    NorStore base;
    ProgressJournal journal(base);
    initialized(journal);
    Fence fence;
    auto first = request();
    first.issueCount = 1;
    outcome(ProgressOutcome::Committed, journal.commit(first, fence));
    // Every byte boundary in reservation anchor/body/CRC/final commit marker.
    for (std::size_t cut = 0; cut < ProgressLimits::SlotBytes + 1; ++cut) {
        NorStore store = base;
        ProgressJournal writer(store);
        outcome(ProgressOutcome::Recovered, writer.recover(origin()));
        auto next = request(2, 1, 1);
        next.issueCount = 2;
        store.budget = cut;
        outcome(ProgressOutcome::WriteUncertain, writer.commit(next, fence));
        closed(writer);
        store.clearFaults();
        outcome(ProgressOutcome::WriteUncertain, writer.recover(origin()));
        ProgressJournal reboot(store);
        if (cut == 0) {
            outcome(ProgressOutcome::Recovered, reboot.recover(origin()));
            equal64(1, reboot.snapshot().lastIssued);
            outcome(ProgressOutcome::Committed, reboot.commit(next, fence));
            equal64(3, reboot.snapshot().lastIssued);
        } else {
            const auto reopened = reboot.recover(origin());
            TEST_ASSERT_TRUE(reopened.outcome == ProgressOutcome::Corrupt ||
                             reopened.outcome == ProgressOutcome::Unsupported);
            closed(reboot);
        }
    }
}
void test_sync_failure_before_and_after_marker_and_no_live_retry() {
    for (unsigned stage = 1; stage <= 3; ++stage) {
        NorStore store;
        ProgressJournal journal(store);
        initialized(journal);
        Fence fence;
        auto value = request();
        value.issueCount = 2;
        store.syncFailure = store.syncs + stage;
        outcome(ProgressOutcome::WriteUncertain, journal.commit(value, fence));
        equal64(0, journal.snapshot().lastIssued);
        const auto writes = store.programs;
        store.clearFaults();
        outcome(ProgressOutcome::WriteUncertain, journal.commit(value, fence));
        TEST_ASSERT_EQUAL(writes, store.programs);
        ProgressJournal reboot(store);
        if (stage < 3) {
            outcome(ProgressOutcome::Corrupt, reboot.recover(origin()));
            closed(reboot);
        } else {
            outcome(ProgressOutcome::Recovered, reboot.recover(origin()));
            equal64(2, reboot.snapshot().lastIssued);
            outcome(ProgressOutcome::Duplicate, reboot.commit(value, fence));
            TEST_ASSERT_EQUAL(writes, store.programs);
        }
    }
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_explicit_creation_and_nonempty_rejection);
    RUN_TEST(test_fixed_header_and_record_bytes);
    RUN_TEST(test_restart_sequence_and_record_reconstruction);
    RUN_TEST(test_lost_response_and_exact_replay_do_not_write_or_call_fence);
    RUN_TEST(test_replay_compares_all_bindings_and_payload_not_only_digest);
    RUN_TEST(test_received_application_and_effect_records_remain_distinct);
    RUN_TEST(test_origin_and_geometry_are_strict);
    RUN_TEST(test_request_limits_staleness_and_no_rejected_writes);
    RUN_TEST(test_zero_digest_and_empty_payload_are_valid_opaque_values);
    RUN_TEST(test_full_capacity_preserves_all_records_and_replay);
    RUN_TEST(test_fence_rejection_and_reentrant_calls);
    RUN_TEST(test_read_bounds_failure_does_not_publish_partial_payload);
    RUN_TEST(test_missing_or_unreadable_store_never_creates_progress);
    RUN_TEST(test_corruption_unknown_version_hole_and_tail_reject_all_progress);
    RUN_TEST(test_forged_allocator_and_predecessor_do_not_pass_crc_alone);
    RUN_TEST(test_entire_confirmed_tail_loss_never_reuses_sequence);
    RUN_TEST(test_live_rollback_to_earlier_valid_image_is_rejected);
    RUN_TEST(test_fence_sees_frozen_bytes_and_metadata);
    RUN_TEST(test_each_recovery_read_failure_blocks_without_writes);
    RUN_TEST(test_initialization_sync_failure_is_not_automatic_repair);
    RUN_TEST(test_every_header_program_prefix_is_closed_or_empty);
    RUN_TEST(test_every_record_program_prefix_never_rolls_back_a_dirty_tail);
    RUN_TEST(test_sync_failure_before_and_after_marker_and_no_live_retry);
    return UNITY_END();
}
