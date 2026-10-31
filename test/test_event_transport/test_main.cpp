#include "Companion/EventTransport.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <unity.h>

using namespace larp::companion::event_transport;
#define CHECK_ENUM(expected, actual)                                                               \
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

namespace {
Id id(std::uint8_t value) {
    Id result{};
    result[0] = value;
    return result;
}
Fact fact(std::uint8_t sequence, std::uint8_t digest = 1) {
    Fact result;
    result.id.event = id(1);
    result.id.origin = id(2);
    result.id.epoch = id(3);
    result.id.sequence = sequence;
    result.run = id(4);
    result.digest[0] = digest;
    return result;
}
using Bytes = std::array<std::uint8_t, 4>;
Bytes bytes(std::uint8_t sequence = 1, std::uint8_t digest = 1) {
    return Bytes{{sequence, digest, 1, 0}};
}
// These four-byte selectors are TEST INPUTS, NOT CBOR/COSE or a wire format.
// This fake models verification decisions; it supplies no cryptographic proof.
class SimulatedVerifier : public OriginVerifier {
  public:
    Verification decision = Verification::Verified;
    Verification receiptDecision = Verification::Verified;
    Receipt response{};
    std::size_t factCalls = 0;
    std::size_t receiptCalls = 0;
    Verification verifyFact(const std::uint8_t *data, std::size_t size, Fact &result) override {
        ++factCalls;
        if (decision != Verification::Verified)
            return decision;
        if (size < 4 || data[2] != 1)
            return Verification::Rejected;
        result = fact(data[0], data[1]);
        return Verification::Verified;
    }
    Verification verifyReceipt(const std::uint8_t *, std::size_t, Receipt &result) override {
        ++receiptCalls;
        result = response;
        return receiptDecision;
    }
    void receiptFor(const Fact &value, Stage stage = Stage::Durable, Id target = id(5)) {
        response = Receipt{};
        response.fact = value;
        response.confirmer = target;
        response.recipient = value.id.origin;
        response.receiptId = id(6);
        response.stage = stage;
        if (stage == Stage::Rejected)
            response.reason = Reason::Context;
        else if (stage == Stage::Deferred)
            response.reason = Reason::Capacity;
        else if (stage == Stage::EffectUnknown)
            response.reason = Reason::EffectUncertain;
    }
};
// In-memory TEST model only: no flash durability, application or replay proof.
class SimulatedDurablePort : public DurablePort {
  public:
    Publication publication = Publication::Durable;
    Acceptance acceptance = Acceptance::Durable;
    ReceiptCommit receiptCommit = ReceiptCommit::Retained;
    std::size_t publishCalls = 0;
    std::size_t acceptCalls = 0;
    std::size_t receiptCalls = 0;
    std::size_t uniqueFacts = 0;
    std::array<bool, 256> seen{};
    std::array<std::uint64_t, 32> order{};
    bool deduplicate = false;
    bool mergeReceipts = false;
    bool haveReceipt = false;
    bool conflict = false;
    Stage previousStage = Stage::Transport;
    Reason lastReceiptReason = Reason::None;
    std::array<std::uint8_t, MessageLimit> lastReceiptBytes{};
    std::size_t lastReceiptSize = 0;
    Publication mayPublish(const Fact &, const Id &, const std::uint8_t *, std::size_t) override {
        ++publishCalls;
        return publication;
    }
    Acceptance accept(const Fact &value, const std::uint8_t *, std::size_t) override {
        if (acceptCalls < order.size())
            order[acceptCalls] = value.id.sequence;
        ++acceptCalls;
        if (deduplicate && acceptance == Acceptance::Durable) {
            const auto index = static_cast<std::size_t>(value.id.sequence);
            if (seen[index])
                return Acceptance::AlreadyDurable;
            seen[index] = true;
            ++uniqueFacts;
        }
        return acceptance;
    }
    ReceiptCommit recordReceipt(const Receipt &value, const std::uint8_t *data,
                                std::size_t size) override {
        ++receiptCalls;
        lastReceiptReason = value.reason;
        lastReceiptSize = size;
        std::copy_n(data, size, lastReceiptBytes.begin());
        if (!mergeReceipts)
            return receiptCommit;
        if (!(value.fact.id == fact(1).id) || value.fact.run != id(4) ||
            value.fact.digest != fact(1).digest || value.confirmer != id(5) ||
            value.recipient != id(2))
            return ReceiptCommit::Unmatched;
        if (haveReceipt && ((previousStage == Stage::Durable && value.stage == Stage::Rejected) ||
                            (previousStage == Stage::Rejected && value.stage == Stage::Durable)))
            conflict = true;
        haveReceipt = true;
        previousStage = value.stage;
        if (conflict)
            return ReceiptCommit::Conflict;
        return value.stage == Stage::Durable ? ReceiptCommit::TargetDurable
                                             : ReceiptCommit::Retained;
    }
};
struct Fixture {
    SimulatedVerifier verifier;
    SimulatedDurablePort durable;
    Core core;
    Fixture() : core(verifier, durable) {}
    Admission enqueue(const Bytes &value = bytes(), Id target = id(5)) {
        return core.enqueue(value.data(), value.size(), target);
    }
    Admission receive(const Bytes &value = bytes()) {
        return core.receive(value.data(), value.size());
    }
    ReceiptResult receipt(std::uint64_t now) {
        const auto input = bytes();
        return core.receipt(now, input.data(), input.size());
    }
};

void test_missing_ports_never_authorize() {
    const auto input = bytes();
    OriginVerifier absentVerifier;
    DurablePort absentDurable;
    SimulatedDurablePort durable;
    SimulatedVerifier verifier;
    Core noVerifier(absentVerifier, durable);
    Core noDurable(verifier, absentDurable);
    CHECK_ENUM(Admission::Unavailable, noVerifier.enqueue(input.data(), input.size(), id(5)));
    CHECK_ENUM(Admission::Unavailable, noDurable.enqueue(input.data(), input.size(), id(5)));
    CHECK_ENUM(ReceiptResult::Unavailable, noVerifier.receipt(0, input.data(), input.size()));
    TEST_ASSERT_EQUAL_UINT(0, durable.publishCalls);
    CHECK_ENUM(Admission::Queued, noVerifier.receive(input.data(), input.size()));
    IncomingResult result;
    CHECK_ENUM(Poll::Blocked, noVerifier.processIncoming(100, result));
    CHECK_ENUM(Verification::Unavailable, result.verification);
    TEST_ASSERT_EQUAL_UINT(0, durable.acceptCalls);
    TEST_ASSERT_EQUAL_UINT(1, noVerifier.incomingCount());
    CHECK_ENUM(Admission::Queued, noDurable.receive(input.data(), input.size()));
    CHECK_ENUM(Poll::Ready, noDurable.processIncoming(0, result));
    CHECK_ENUM(Acceptance::Unavailable, result.acceptance);
    TEST_ASSERT_EQUAL_UINT(1, noDurable.incomingCount());
}
void test_invalid_sizes_rejected_before_verification() {
    Fixture f;
    std::array<std::uint8_t, MessageLimit + 1> data{};
    const auto input = bytes();
    std::copy(input.begin(), input.end(), data.begin());
    CHECK_ENUM(Admission::Invalid, f.core.enqueue(nullptr, 4, id(5)));
    CHECK_ENUM(Admission::Invalid, f.core.enqueue(data.data(), 0, id(5)));
    CHECK_ENUM(Admission::Invalid, f.core.enqueue(data.data(), data.size(), id(5)));
    CHECK_ENUM(Admission::Invalid, f.core.enqueue(data.data(), 4, Id{}));
    CHECK_ENUM(Admission::Invalid, f.core.receive(nullptr, 4));
    CHECK_ENUM(Admission::Invalid, f.core.receive(data.data(), 0));
    CHECK_ENUM(Admission::Invalid, f.core.receive(data.data(), data.size()));
    CHECK_ENUM(ReceiptResult::Invalid, f.core.receipt(0, nullptr, 4));
    CHECK_ENUM(ReceiptResult::Invalid, f.core.receipt(100, data.data(), 0));
    CHECK_ENUM(ReceiptResult::Invalid, f.core.receipt(200, data.data(), data.size()));
    TEST_ASSERT_EQUAL_UINT(0, f.verifier.factCalls);
    TEST_ASSERT_EQUAL_UINT(0, f.verifier.receiptCalls);
    CHECK_ENUM(Admission::Queued, f.core.enqueue(data.data(), MessageLimit, id(5)));
    CHECK_ENUM(Admission::Queued, f.core.receive(data.data(), MessageLimit));
    data.fill(0);
    Attempt attempt;
    CHECK_ENUM(Poll::Ready, f.core.poll(300, Bearer::EspNow, true, attempt));
    TEST_ASSERT_EQUAL_UINT(MessageLimit, attempt.size);
    TEST_ASSERT_EQUAL_MEMORY(input.data(), attempt.bytes.data(), input.size());
    IncomingResult result;
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(300, result));
    TEST_ASSERT_TRUE(result.fact.id.sequence == 1);
}
void test_only_verified_facts_enter_d1() {
    Fixture f;
    auto nonFact = bytes();
    for (std::uint8_t kind = 2; kind <= 4; ++kind) {
        nonFact[2] = kind; // Simulated observation, command, receipt.
        CHECK_ENUM(Admission::Unauthorized, f.enqueue(nonFact));
        CHECK_ENUM(Admission::Queued, f.receive(nonFact));
        IncomingResult result;
        CHECK_ENUM(Poll::Blocked, f.core.processIncoming((kind - 2) * 100, result));
        CHECK_ENUM(Verification::Rejected, result.verification);
    }
    CHECK_ENUM(Admission::Unauthorized, f.enqueue(bytes(0)));
    TEST_ASSERT_EQUAL_UINT(0, f.durable.publishCalls);
    TEST_ASSERT_EQUAL_UINT(0, f.durable.acceptCalls);
    TEST_ASSERT_EQUAL_UINT(0, f.core.incomingCount());
}
void test_full_queues_do_not_evict() {
    Fixture f;
    for (std::uint8_t value = 1; value <= QueueCapacity; ++value) {
        CHECK_ENUM(Admission::Queued, f.enqueue(bytes(value)));
        CHECK_ENUM(Admission::Queued, f.receive(bytes(value)));
    }
    CHECK_ENUM(Admission::Full, f.enqueue(bytes(9)));
    CHECK_ENUM(Admission::Full, f.receive(bytes(9)));
    TEST_ASSERT_EQUAL_UINT(QueueCapacity, f.core.outgoingCount());
    TEST_ASSERT_EQUAL_UINT(QueueCapacity, f.core.incomingCount());
    for (std::uint8_t value = 1; value <= QueueCapacity; ++value) {
        Attempt attempt;
        IncomingResult result;
        const std::uint64_t now = (value - 1) * DispatchIntervalMs;
        CHECK_ENUM(Poll::Ready, f.core.poll(now, Bearer::EspNow, true, attempt));
        TEST_ASSERT_TRUE(attempt.fact.id.sequence == value);
        CHECK_ENUM(Poll::Ready, f.core.processIncoming(now, result));
        TEST_ASSERT_TRUE(result.fact.id.sequence == value);
    }
    TEST_ASSERT_EQUAL_UINT(QueueCapacity, f.core.outgoingCount());
    TEST_ASSERT_EQUAL_UINT(0, f.core.incomingCount());
    CHECK_ENUM(Admission::Queued, f.receive(bytes(9)));
}
void test_absent_contact_and_bearer_switch_preserve_bytes_and_deadline() {
    Fixture f;
    auto input = bytes();
    const auto original = input;
    CHECK_ENUM(Admission::Queued, f.enqueue(input));
    input.fill(9);
    Attempt first;
    CHECK_ENUM(Poll::Idle, f.core.poll(0, Bearer::EspNow, false, first));
    CHECK_ENUM(Poll::Idle, f.core.poll(50000, Bearer::Wlan, false, first));
    TEST_ASSERT_EQUAL_UINT(1, f.verifier.factCalls);
    CHECK_ENUM(Poll::Ready, f.core.poll(50000, Bearer::EspNow, true, first));
    TEST_ASSERT_EQUAL_MEMORY(original.data(), first.bytes.data(), original.size());
    TEST_ASSERT_TRUE(f.core.linkResult(first.token, LinkResult::Accepted));
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    Attempt second;
    CHECK_ENUM(Poll::Idle, f.core.poll(50099, Bearer::Wlan, true, second));
    CHECK_ENUM(Poll::Ready, f.core.poll(50100, Bearer::Wlan, true, second));
    TEST_ASSERT_TRUE(first.fact.id == second.fact.id);
    TEST_ASSERT_TRUE(first.fact.digest == second.fact.digest);
    TEST_ASSERT_TRUE(first.fact.run == second.fact.run);
    TEST_ASSERT_TRUE(first.target == second.target);
    TEST_ASSERT_EQUAL_MEMORY(first.bytes.data(), second.bytes.data(), first.size);
    CHECK_ENUM(Bearer::Wlan, second.bearer);
    CHECK_ENUM(Poll::Idle, f.core.poll(50200, Bearer::EspNow, true, first));
    CHECK_ENUM(Poll::Ready, f.core.poll(50300, Bearer::EspNow, true, first));
}
void test_retry_backoff_is_capped_and_no_callback_is_required() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    const std::array<std::uint64_t, 8> due{{0, 100, 300, 700, 1500, 3100, 4700, 6300}};
    Attempt attempt;
    std::uint64_t previousToken = 0;
    for (const auto now : due) {
        if (now != 0)
            CHECK_ENUM(Poll::Idle, f.core.poll(now - 1, Bearer::Wlan, true, attempt));
        CHECK_ENUM(Poll::Ready, f.core.poll(now, Bearer::EspNow, true, attempt));
        TEST_ASSERT_TRUE(attempt.token > previousToken);
        TEST_ASSERT_FALSE(f.core.linkResult(previousToken, LinkResult::Accepted));
        previousToken = attempt.token;
        CHECK_ENUM(Poll::Idle, f.core.poll(now, Bearer::Wlan, true, attempt));
        TEST_ASSERT_EQUAL_UINT(0, attempt.size);
        TEST_ASSERT_TRUE(attempt.token == 0);
    }
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
}
void test_global_pacing_and_round_robin_fairness() {
    Fixture f;
    for (std::uint8_t value = 1; value <= QueueCapacity; ++value)
        CHECK_ENUM(Admission::Queued, f.enqueue(bytes(value)));
    for (std::size_t step = 0; step < QueueCapacity * 2; ++step) {
        Attempt attempt;
        const auto now = step * DispatchIntervalMs;
        CHECK_ENUM(Poll::Ready, f.core.poll(now, Bearer::EspNow, true, attempt));
        TEST_ASSERT_TRUE(attempt.fact.id.sequence == step % QueueCapacity + 1);
        CHECK_ENUM(Poll::Idle, f.core.poll(now, Bearer::EspNow, true, attempt));
        CHECK_ENUM(Poll::Idle, f.core.poll(now + 99, Bearer::Wlan, true, attempt));
    }
}
void test_link_results_are_one_shot_and_never_release() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    Attempt attempt;
    CHECK_ENUM(Poll::Ready, f.core.poll(0, Bearer::EspNow, true, attempt));
    const auto firstToken = attempt.token;
    TEST_ASSERT_FALSE(f.core.linkResult(firstToken, static_cast<LinkResult>(90)));
    TEST_ASSERT_TRUE(f.core.linkResult(firstToken, LinkResult::Accepted));
    TEST_ASSERT_FALSE(f.core.linkResult(firstToken, LinkResult::Accepted));
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    CHECK_ENUM(Poll::Ready, f.core.poll(100, Bearer::Wlan, true, attempt));
    TEST_ASSERT_TRUE(f.core.linkResult(attempt.token, LinkResult::Failed));
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    TEST_ASSERT_EQUAL_UINT(0, f.durable.receiptCalls);
    CHECK_ENUM(Poll::Idle, f.core.poll(200, Bearer::Wlan, true, attempt));
    CHECK_ENUM(Poll::Ready, f.core.poll(300, Bearer::Wlan, true, attempt));
}
void test_receipt_requires_full_reference_and_authority() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    f.durable.receiptCommit = ReceiptCommit::TargetDurable;
    for (int changed = 0; changed != 9; ++changed) {
        f.verifier.receiptFor(fact(1));
        auto &r = f.verifier.response;
        switch (changed) {
        case 0:
            r.fact.id.event = id(9);
            break;
        case 1:
            r.fact.id.origin = id(9);
            break;
        case 2:
            r.fact.id.epoch = id(9);
            break;
        case 3:
            r.fact.id.sequence = 9;
            break;
        case 4:
            r.fact.run = id(9);
            break;
        case 5:
            r.fact.digest[0] = 9;
            break;
        case 6:
            r.confirmer = id(9);
            break;
        case 7:
            r.recipient = id(9);
            break;
        default:
            r.receiptId = Id{};
            break;
        }
        CHECK_ENUM(changed == 8 ? ReceiptResult::Unauthorized : ReceiptResult::Unmatched,
                   f.receipt(changed * 100));
        TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    }
    TEST_ASSERT_EQUAL_UINT(8, f.durable.receiptCalls);
    f.verifier.receiptFor(fact(1));
    f.verifier.receiptDecision = Verification::Rejected;
    CHECK_ENUM(ReceiptResult::Unauthorized, f.receipt(900));
    f.verifier.receiptDecision = Verification::Unavailable;
    CHECK_ENUM(ReceiptResult::Unavailable, f.receipt(1000));
    TEST_ASSERT_EQUAL_UINT(8, f.durable.receiptCalls);
}
void test_transport_and_negative_stages_never_release_even_with_bad_d1_result() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    f.durable.receiptCommit = ReceiptCommit::TargetDurable;
    const std::array<Stage, 4> stages{
        {Stage::Transport, Stage::Rejected, Stage::Deferred, Stage::EffectUnknown}};
    std::uint64_t now = 0;
    for (const auto stage : stages) {
        f.verifier.receiptFor(fact(1), stage);
        CHECK_ENUM(ReceiptResult::Retained, f.receipt(now));
        TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
        now += 100;
    }
    Attempt attempt;
    CHECK_ENUM(Poll::Ready, f.core.poll(now, Bearer::Wlan, true, attempt));
}
void test_receipt_reason_and_original_bytes_reach_d1_unchanged() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    f.verifier.receiptFor(fact(1), Stage::Deferred);
    // Opaque test evidence, deliberately different from the outbound fact bytes.
    const std::array<std::uint8_t, 7> original{{0xD2, 0x81, 0x00, 0xFF, 0x13, 0x37, 0x42}};
    CHECK_ENUM(ReceiptResult::Retained, f.core.receipt(0, original.data(), original.size()));
    TEST_ASSERT_EQUAL_UINT(1, f.durable.receiptCalls);
    CHECK_ENUM(Reason::Capacity, f.durable.lastReceiptReason);
    TEST_ASSERT_EQUAL_UINT(original.size(), f.durable.lastReceiptSize);
    TEST_ASSERT_EQUAL_MEMORY(original.data(), f.durable.lastReceiptBytes.data(), original.size());
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
}
void test_durable_receipts_release_only_volatile_target_slot() {
    const std::array<Stage, 3> stages{{Stage::Durable, Stage::Applied, Stage::Observed}};
    for (const auto stage : stages) {
        Fixture f;
        CHECK_ENUM(Admission::Queued, f.enqueue());
        CHECK_ENUM(Admission::Queued, f.enqueue(bytes(), id(7)));
        f.verifier.receiptFor(fact(1), stage);
        f.durable.receiptCommit = ReceiptCommit::Unavailable;
        CHECK_ENUM(ReceiptResult::Unavailable, f.receipt(0));
        TEST_ASSERT_EQUAL_UINT(2, f.core.outgoingCount());
        f.durable.receiptCommit = ReceiptCommit::Retained;
        CHECK_ENUM(ReceiptResult::Retained, f.receipt(100));
        TEST_ASSERT_EQUAL_UINT(2, f.core.outgoingCount());
        f.durable.receiptCommit = ReceiptCommit::TargetDurable;
        CHECK_ENUM(ReceiptResult::Released, f.receipt(200));
        TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
        CHECK_ENUM(ReceiptResult::Unmatched, f.receipt(300));
        Attempt attempt;
        CHECK_ENUM(Poll::Ready, f.core.poll(300, Bearer::Wlan, true, attempt));
        TEST_ASSERT_TRUE(attempt.target == id(7));
    }
}
void test_late_link_completion_cannot_touch_reused_slot() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    Attempt attempt;
    CHECK_ENUM(Poll::Ready, f.core.poll(0, Bearer::EspNow, true, attempt));
    const auto oldToken = attempt.token;
    f.verifier.receiptFor(fact(1));
    f.durable.receiptCommit = ReceiptCommit::TargetDurable;
    CHECK_ENUM(ReceiptResult::Released, f.receipt(0));
    CHECK_ENUM(Admission::Queued, f.enqueue(bytes(2)));
    TEST_ASSERT_FALSE(f.core.linkResult(oldToken, LinkResult::Accepted));
    CHECK_ENUM(Poll::Idle, f.core.poll(99, Bearer::Wlan, true, attempt));
    CHECK_ENUM(Poll::Ready, f.core.poll(100, Bearer::Wlan, true, attempt));
    TEST_ASSERT_TRUE(attempt.token != oldToken);
    TEST_ASSERT_TRUE(attempt.fact.id.sequence == 2);
    TEST_ASSERT_FALSE(f.core.linkResult(oldToken, LinkResult::Failed));
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    CHECK_ENUM(ReceiptResult::Unmatched, f.receipt(100));
}
void test_duplicate_payload_does_not_replace_signature_or_reset_retry() {
    Fixture f;
    const auto original = bytes();
    CHECK_ENUM(Admission::Queued, f.enqueue(original));
    Attempt attempt;
    CHECK_ENUM(Poll::Ready, f.core.poll(0, Bearer::EspNow, true, attempt));
    auto alternateSignature = original;
    alternateSignature[3] = 99;
    CHECK_ENUM(Admission::Duplicate, f.enqueue(alternateSignature));
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    CHECK_ENUM(Poll::Idle, f.core.poll(99, Bearer::Wlan, true, attempt));
    CHECK_ENUM(Poll::Ready, f.core.poll(100, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_MEMORY(original.data(), attempt.bytes.data(), original.size());
}
void test_conflicting_fact_quarantines_all_targets_and_new_targets() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    CHECK_ENUM(Admission::Queued, f.enqueue(bytes(), id(7)));
    Attempt attempt;
    CHECK_ENUM(Poll::Ready, f.core.poll(0, Bearer::EspNow, true, attempt));
    const auto token = attempt.token;
    CHECK_ENUM(Admission::Conflict, f.enqueue(bytes(1, 9)));
    TEST_ASSERT_FALSE(f.core.linkResult(token, LinkResult::Accepted));
    CHECK_ENUM(Poll::Idle, f.core.poll(1000, Bearer::EspNow, true, attempt));
    CHECK_ENUM(Admission::Conflict, f.enqueue());
    CHECK_ENUM(Admission::Conflict, f.enqueue(bytes(), id(8)));
    TEST_ASSERT_EQUAL_UINT(2, f.core.outgoingCount());
    f.verifier.receiptFor(fact(1));
    f.durable.receiptCommit = ReceiptCommit::TargetDurable;
    CHECK_ENUM(ReceiptResult::Retained, f.receipt(1000));
}
void test_receipt_conflict_holds_slot_and_stops_retries() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    f.verifier.receiptFor(fact(1));
    f.durable.receiptCommit = ReceiptCommit::Conflict;
    CHECK_ENUM(ReceiptResult::Conflict, f.receipt(0));
    Attempt attempt;
    CHECK_ENUM(Poll::Idle, f.core.poll(1000, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    f.durable.receiptCommit = ReceiptCommit::TargetDurable;
    CHECK_ENUM(ReceiptResult::Retained, f.receipt(1000));
}
void test_receipt_conflicts_survive_release_reuse_and_core_reconstruction() {
    Fixture f;
    f.durable.mergeReceipts = true;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    f.verifier.receiptFor(fact(1));
    CHECK_ENUM(ReceiptResult::Released, f.receipt(0));
    CHECK_ENUM(Admission::Queued, f.enqueue(bytes(2)));
    f.verifier.receiptFor(fact(1), Stage::Rejected);
    CHECK_ENUM(ReceiptResult::Conflict, f.receipt(100));
    TEST_ASSERT_TRUE(f.durable.conflict);
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    Core rebuilt(f.verifier, f.durable);
    const auto input = bytes();
    CHECK_ENUM(ReceiptResult::Conflict, rebuilt.receipt(0, input.data(), input.size()));
    Fixture reversed;
    reversed.durable.mergeReceipts = true;
    CHECK_ENUM(Admission::Queued, reversed.enqueue());
    reversed.verifier.receiptFor(fact(1), Stage::Rejected);
    CHECK_ENUM(ReceiptResult::Retained, reversed.receipt(0));
    reversed.verifier.receiptFor(fact(1), Stage::Durable);
    CHECK_ENUM(ReceiptResult::Conflict, reversed.receipt(100));
    TEST_ASSERT_EQUAL_UINT(1, reversed.core.outgoingCount());
}
void test_order_and_duplicates_remain_d1_decisions() {
    Fixture f;
    f.durable.deduplicate = true;
    CHECK_ENUM(Admission::Queued, f.receive(bytes(5)));
    CHECK_ENUM(Admission::Queued, f.receive(bytes(4)));
    auto duplicate = bytes(5);
    duplicate[3] = 8;
    CHECK_ENUM(Admission::Queued, f.receive(duplicate));
    TEST_ASSERT_EQUAL_UINT(0, f.verifier.factCalls);
    IncomingResult result;
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(0, result));
    CHECK_ENUM(Acceptance::Durable, result.acceptance);
    TEST_ASSERT_TRUE(result.fact.id.sequence == 5);
    CHECK_ENUM(Poll::Idle, f.core.processIncoming(99, result));
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(100, result));
    TEST_ASSERT_TRUE(result.fact.id.sequence == 4);
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(200, result));
    CHECK_ENUM(Acceptance::AlreadyDurable, result.acceptance);
    TEST_ASSERT_EQUAL_UINT(3, f.durable.acceptCalls);
    TEST_ASSERT_EQUAL_UINT(2, f.durable.uniqueFacts);
    TEST_ASSERT_EQUAL_UINT(3, f.verifier.factCalls);
    TEST_ASSERT_TRUE(f.durable.order[0] == 5 && f.durable.order[1] == 4 && f.durable.order[2] == 5);
    TEST_ASSERT_EQUAL_UINT(0, f.core.incomingCount());
}
void test_deferred_incoming_has_backoff_and_fairness() {
    Fixture f;
    f.durable.acceptance = Acceptance::Deferred;
    CHECK_ENUM(Admission::Queued, f.receive(bytes(1)));
    CHECK_ENUM(Admission::Queued, f.receive(bytes(2)));
    IncomingResult result;
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(0, result));
    CHECK_ENUM(Acceptance::Deferred, result.acceptance);
    CHECK_ENUM(Poll::Idle, f.core.processIncoming(0, result));
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(100, result));
    TEST_ASSERT_TRUE(result.fact.id.sequence == 2);
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(200, result));
    TEST_ASSERT_TRUE(result.fact.id.sequence == 1);
    TEST_ASSERT_EQUAL_UINT(2, f.core.incomingCount());
    f.durable.acceptance = Acceptance::Durable;
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(300, result));
    TEST_ASSERT_TRUE(result.fact.id.sequence == 2);
    CHECK_ENUM(Poll::Idle, f.core.processIncoming(399, result));
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(400, result));
    TEST_ASSERT_TRUE(result.fact.id.sequence == 1);
    TEST_ASSERT_EQUAL_UINT(0, f.core.incomingCount());
    TEST_ASSERT_EQUAL_UINT(5, f.verifier.factCalls);
}
void test_revocation_rechecked_on_dispatch_and_deferred_acceptance() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    CHECK_ENUM(Admission::Queued, f.receive());
    f.durable.acceptance = Acceptance::Deferred;
    IncomingResult result;
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(0, result));
    f.verifier.decision = Verification::Rejected;
    Attempt attempt;
    CHECK_ENUM(Poll::Blocked, f.core.poll(100, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(0, attempt.size);
    CHECK_ENUM(Poll::Blocked, f.core.processIncoming(100, result));
    CHECK_ENUM(Verification::Rejected, result.verification);
    TEST_ASSERT_EQUAL_UINT(1, f.durable.acceptCalls);
    TEST_ASSERT_EQUAL_UINT(1, f.durable.publishCalls);
    TEST_ASSERT_EQUAL_UINT(0, f.core.incomingCount());
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
    f.verifier.decision = Verification::Verified;
    CHECK_ENUM(Poll::Idle, f.core.poll(1000, Bearer::EspNow, true, attempt));
    CHECK_ENUM(Admission::Unauthorized, f.enqueue());
}
void test_publication_denial_holds_outgoing_and_missing_d1_retains_incoming() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    f.durable.publication = Publication::Denied;
    Attempt attempt;
    CHECK_ENUM(Poll::Blocked, f.core.poll(0, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(2, f.durable.publishCalls);
    CHECK_ENUM(Admission::Unauthorized, f.enqueue(bytes(2)));
    CHECK_ENUM(Admission::Queued, f.receive());
    f.durable.acceptance = Acceptance::Unavailable;
    IncomingResult result;
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(0, result));
    CHECK_ENUM(Acceptance::Unavailable, result.acceptance);
    TEST_ASSERT_EQUAL_UINT(1, f.core.incomingCount());
    f.durable.acceptance = Acceptance::Conflict;
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(100, result));
    CHECK_ENUM(Acceptance::Conflict, result.acceptance);
    TEST_ASSERT_EQUAL_UINT(0, f.core.incomingCount());
}
void test_unavailable_verifier_recovers_without_hot_loop() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    f.verifier.decision = Verification::Unavailable;
    Attempt attempt;
    CHECK_ENUM(Poll::Blocked, f.core.poll(0, Bearer::EspNow, true, attempt));
    TEST_ASSERT_EQUAL_UINT(2, f.verifier.factCalls);
    CHECK_ENUM(Poll::Idle, f.core.poll(99, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(2, f.verifier.factCalls);
    f.verifier.decision = Verification::Verified;
    CHECK_ENUM(Poll::Ready, f.core.poll(100, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
}
void test_unavailable_d1_publication_recovers_without_hot_loop() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    f.durable.publication = Publication::Unavailable;
    Attempt attempt;
    CHECK_ENUM(Poll::Blocked, f.core.poll(0, Bearer::EspNow, true, attempt));
    TEST_ASSERT_EQUAL_UINT(2, f.durable.publishCalls);
    CHECK_ENUM(Poll::Idle, f.core.poll(99, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(2, f.durable.publishCalls);
    f.durable.publication = Publication::Durable;
    CHECK_ENUM(Poll::Ready, f.core.poll(100, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(1, f.core.outgoingCount());
}
void test_receipts_share_incoming_rate_budget_and_reject_bad_time() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.receive());
    f.verifier.receiptFor(fact(1));
    IncomingResult result;
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(0, result));
    CHECK_ENUM(ReceiptResult::RateLimited, f.receipt(0));
    CHECK_ENUM(ReceiptResult::RateLimited, f.receipt(99));
    TEST_ASSERT_EQUAL_UINT(0, f.verifier.receiptCalls);
    CHECK_ENUM(ReceiptResult::Unmatched, f.receipt(100));
    CHECK_ENUM(Admission::Queued, f.receive(bytes(2)));
    CHECK_ENUM(Poll::Idle, f.core.processIncoming(100, result));
    CHECK_ENUM(ReceiptResult::ClockError, f.receipt(99));
    CHECK_ENUM(ReceiptResult::ClockError, f.receipt(std::numeric_limits<std::uint64_t>::max()));
    TEST_ASSERT_EQUAL_UINT(1, f.verifier.receiptCalls);
    TEST_ASSERT_EQUAL_UINT(1, f.core.incomingCount());
    CHECK_ENUM(Poll::Ready, f.core.processIncoming(200, result));
}
void test_clock_rollback_overflow_and_large_time() {
    Fixture f;
    CHECK_ENUM(Admission::Queued, f.enqueue());
    Attempt attempt;
    const std::uint64_t now =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) - 50;
    CHECK_ENUM(Poll::Ready, f.core.poll(now, Bearer::EspNow, true, attempt));
    CHECK_ENUM(Poll::ClockError, f.core.poll(now - 1, Bearer::EspNow, true, attempt));
    TEST_ASSERT_EQUAL_UINT(0, attempt.size);
    CHECK_ENUM(Poll::Ready, f.core.poll(now + 100, Bearer::Wlan, true, attempt));
    CHECK_ENUM(Poll::ClockError,
               f.core.poll(std::numeric_limits<std::uint64_t>::max(), Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(0, attempt.size);
    CHECK_ENUM(Admission::Queued, f.receive());
    IncomingResult result;
    CHECK_ENUM(Poll::ClockError, f.core.processIncoming(now, result));
    CHECK_ENUM(Poll::ClockError,
               f.core.processIncoming(std::numeric_limits<std::uint64_t>::max(), result));
    TEST_ASSERT_EQUAL_UINT(0, f.durable.acceptCalls);
    TEST_ASSERT_EQUAL_UINT(1, f.core.incomingCount());
    Fixture limit;
    CHECK_ENUM(Admission::Queued, limit.enqueue());
    const auto lastSafe = std::numeric_limits<std::uint64_t>::max() - RetryLimitMs;
    CHECK_ENUM(Poll::Ready, limit.core.poll(lastSafe, Bearer::Wlan, true, attempt));
    CHECK_ENUM(Poll::ClockError, limit.core.poll(lastSafe + 1, Bearer::Wlan, true, attempt));
}
void test_fixed_memory_budget_and_idle_outputs() {
    Fixture f;
    Attempt attempt;
    attempt.size = 5;
    attempt.token = 12;
    CHECK_ENUM(Poll::Idle, f.core.poll(0, Bearer::EspNow, true, attempt));
    TEST_ASSERT_EQUAL_UINT(0, attempt.size);
    TEST_ASSERT_TRUE(attempt.token == 0);
    IncomingResult result;
    result.fact = fact(9);
    result.acceptance = Acceptance::Durable;
    CHECK_ENUM(Poll::Idle, f.core.processIncoming(0, result));
    TEST_ASSERT_TRUE(result.fact.id.sequence == 0);
    CHECK_ENUM(Acceptance::Unavailable, result.acceptance);
    static_assert(sizeof(Core) <= 20 * 1024, "Core exceeds fixed memory budget");
    std::printf("D2 Core=%u bytes; Attempt=%u bytes; queues=%u each; message=%u bytes\n",
                static_cast<unsigned>(sizeof(Core)), static_cast<unsigned>(sizeof(Attempt)),
                static_cast<unsigned>(QueueCapacity), static_cast<unsigned>(MessageLimit));
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_missing_ports_never_authorize);
    RUN_TEST(test_invalid_sizes_rejected_before_verification);
    RUN_TEST(test_only_verified_facts_enter_d1);
    RUN_TEST(test_full_queues_do_not_evict);
    RUN_TEST(test_absent_contact_and_bearer_switch_preserve_bytes_and_deadline);
    RUN_TEST(test_retry_backoff_is_capped_and_no_callback_is_required);
    RUN_TEST(test_global_pacing_and_round_robin_fairness);
    RUN_TEST(test_link_results_are_one_shot_and_never_release);
    RUN_TEST(test_receipt_requires_full_reference_and_authority);
    RUN_TEST(test_transport_and_negative_stages_never_release_even_with_bad_d1_result);
    RUN_TEST(test_receipt_reason_and_original_bytes_reach_d1_unchanged);
    RUN_TEST(test_durable_receipts_release_only_volatile_target_slot);
    RUN_TEST(test_late_link_completion_cannot_touch_reused_slot);
    RUN_TEST(test_duplicate_payload_does_not_replace_signature_or_reset_retry);
    RUN_TEST(test_conflicting_fact_quarantines_all_targets_and_new_targets);
    RUN_TEST(test_receipt_conflict_holds_slot_and_stops_retries);
    RUN_TEST(test_receipt_conflicts_survive_release_reuse_and_core_reconstruction);
    RUN_TEST(test_order_and_duplicates_remain_d1_decisions);
    RUN_TEST(test_deferred_incoming_has_backoff_and_fairness);
    RUN_TEST(test_revocation_rechecked_on_dispatch_and_deferred_acceptance);
    RUN_TEST(test_publication_denial_holds_outgoing_and_missing_d1_retains_incoming);
    RUN_TEST(test_unavailable_verifier_recovers_without_hot_loop);
    RUN_TEST(test_unavailable_d1_publication_recovers_without_hot_loop);
    RUN_TEST(test_receipts_share_incoming_rate_budget_and_reject_bad_time);
    RUN_TEST(test_clock_rollback_overflow_and_large_time);
    RUN_TEST(test_fixed_memory_budget_and_idle_outputs);
    return UNITY_END();
}
