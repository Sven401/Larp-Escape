#include "Companion/EventDurableWorker.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <type_traits>
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
Fact fact(std::uint64_t sequence = 1) {
    Fact value;
    value.id.event = id(1);
    value.id.origin = id(2);
    value.id.epoch = id(3);
    value.id.sequence = sequence;
    value.run = id(4);
    value.digest[0] = 5;
    return value;
}
Receipt receipt() {
    Receipt value;
    value.fact = fact();
    value.confirmer = id(6);
    value.recipient = id(2);
    value.receiptId = id(7);
    value.stage = Stage::Deferred;
    value.reason = Reason::Capacity;
    return value;
}
const std::array<std::uint8_t, 6> Input{{0xD2, 0, 255, 17, 91, 34}};
Publication publish(EventDurableWorker &bridge, const Fact &value = fact(),
                    const Id &target = id(6)) {
    return bridge.mayPublish(value, target, Input.data(), Input.size());
}
Acceptance accept(EventDurableWorker &bridge, const Fact &value = fact()) {
    return bridge.accept(value, Input.data(), Input.size());
}
ReceiptCommit record(EventDurableWorker &bridge, const Receipt &value = receipt()) {
    return bridge.recordReceipt(value, Input.data(), Input.size());
}

// Explicit TEST-ONLY semantic outcomes. No flash, crypto, actual application or
// D1 transaction proof is provided by these values or by the selectors above.
class SimulatedSemanticPort : public DurablePort {
  public:
    Publication publication = Publication::Durable;
    Acceptance acceptance = Acceptance::Durable;
    ReceiptCommit receiptResult = ReceiptCommit::Retained;
    std::size_t publishCalls = 0;
    std::size_t acceptCalls = 0;
    std::size_t receiptCalls = 0;
    WorkRequest observed{};
    EventDurableWorker *invalidateDuringCall = nullptr;
    void copy(const std::uint8_t *data, std::size_t size) {
        observed.size = size;
        std::copy_n(data, size, observed.bytes.begin());
        if (invalidateDuringCall != nullptr)
            invalidateDuringCall->invalidate();
    }
    Publication mayPublish(const Fact &value, const Id &target, const std::uint8_t *data,
                           std::size_t size) override {
        ++publishCalls;
        observed.fact = value;
        observed.target = target;
        copy(data, size);
        return publication;
    }
    Acceptance accept(const Fact &value, const std::uint8_t *data, std::size_t size) override {
        ++acceptCalls;
        observed.fact = value;
        copy(data, size);
        return acceptance;
    }
    ReceiptCommit recordReceipt(const Receipt &value, const std::uint8_t *data,
                                std::size_t size) override {
        ++receiptCalls;
        observed.receipt = value;
        copy(data, size);
        return receiptResult;
    }
};
void assertBytes(const WorkRequest &request) {
    TEST_ASSERT_EQUAL_UINT(Input.size(), request.size);
    TEST_ASSERT_EQUAL_MEMORY(Input.data(), request.bytes.data(), Input.size());
}

void test_default_lifecycle_and_semantic_port_deny() {
    EventDurableWorker inactive;
    WorkRequest request;
    CHECK_ENUM(Publication::Unavailable, publish(inactive));
    CHECK_ENUM(Acceptance::Unavailable, accept(inactive));
    CHECK_ENUM(ReceiptCommit::Unavailable, record(inactive));
    CHECK_ENUM(HandoffStatus::Inactive, inactive.status());
    TEST_ASSERT_EQUAL_UINT(0, inactive.outstanding());
    TEST_ASSERT_FALSE(inactive.take(request));
    TEST_ASSERT_FALSE(inactive.invalidate());
    EventDurableWorker bridge(id(90));
    DurablePort absent;
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, absent));
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(HandoffStatus::Completed, bridge.status());
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge));
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, absent));
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge));
    CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, absent));
    CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
    TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
}

void test_dispatch_preserves_all_operations_and_original_evidence() {
    EventDurableWorker bridge(id(90));
    SimulatedSemanticPort semantic;
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge));
    CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
    TEST_ASSERT_EQUAL_UINT(0, semantic.publishCalls + semantic.acceptCalls + semantic.receiptCalls);
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    TEST_ASSERT_EQUAL_UINT(1, semantic.publishCalls);
    TEST_ASSERT_TRUE(semantic.observed.fact.id == fact().id);
    TEST_ASSERT_TRUE(semantic.observed.fact.run == fact().run);
    TEST_ASSERT_TRUE(semantic.observed.fact.digest == fact().digest);
    TEST_ASSERT_TRUE(semantic.observed.target == id(6));
    assertBytes(semantic.observed);
    CHECK_ENUM(Publication::Durable, publish(bridge));
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    TEST_ASSERT_EQUAL_UINT(1, semantic.acceptCalls);
    TEST_ASSERT_TRUE(semantic.observed.fact.id == fact().id);
    assertBytes(semantic.observed);
    CHECK_ENUM(Acceptance::Durable, accept(bridge));
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    TEST_ASSERT_EQUAL_UINT(1, semantic.receiptCalls);
    TEST_ASSERT_TRUE(semantic.observed.receipt.fact.id == receipt().fact.id);
    TEST_ASSERT_TRUE(semantic.observed.receipt.receiptId == receipt().receiptId);
    TEST_ASSERT_TRUE(semantic.observed.receipt.confirmer == receipt().confirmer);
    TEST_ASSERT_TRUE(semantic.observed.receipt.recipient == receipt().recipient);
    CHECK_ENUM(Stage::Deferred, semantic.observed.receipt.stage);
    CHECK_ENUM(Reason::Capacity, semantic.observed.receipt.reason);
    assertBytes(semantic.observed);
    CHECK_ENUM(ReceiptCommit::Retained, record(bridge));
    TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
    CHECK_ENUM(ServiceResult::Idle, serviceOne(bridge, semantic));
}

void test_copies_and_repeated_poll_do_not_duplicate_work() {
    EventDurableWorker bridge(id(90));
    auto data = Input;
    CHECK_ENUM(Publication::Unavailable,
               bridge.mayPublish(fact(), id(6), data.data(), data.size()));
    data.fill(0);
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(HandoffStatus::Pending, bridge.status());
    TEST_ASSERT_EQUAL_UINT(1, bridge.outstanding());
    WorkRequest request;
    TEST_ASSERT_TRUE(bridge.take(request));
    assertBytes(request);
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    TEST_ASSERT_EQUAL_UINT(1, bridge.outstanding());
    const auto ticket = request.ticket;
    request.bytes.fill(0);
    request.fact = fact(99);
    TEST_ASSERT_TRUE(bridge.complete(ticket, Publication::Durable));
    CHECK_ENUM(Publication::Durable, publish(bridge));
    CHECK_ENUM(HandoffStatus::Completed, bridge.status());
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(HandoffStatus::Queued, bridge.status());
    TEST_ASSERT_TRUE(bridge.take(request));
    TEST_ASSERT_TRUE(request.ticket.sequence > ticket.sequence);
    assertBytes(request);
}

void test_all_outcomes_are_typed_and_consumed_once() {
    const std::array<Publication, 3> pubs{
        {Publication::Unavailable, Publication::Denied, Publication::Durable}};
    const std::array<Acceptance, 6> accepts{{Acceptance::Unavailable, Acceptance::Rejected,
                                             Acceptance::Deferred, Acceptance::Conflict,
                                             Acceptance::Durable, Acceptance::AlreadyDurable}};
    const std::array<ReceiptCommit, 5> receipts{
        {ReceiptCommit::Unavailable, ReceiptCommit::Unmatched, ReceiptCommit::Retained,
         ReceiptCommit::Conflict, ReceiptCommit::TargetDurable}};
    EventDurableWorker bridge(id(90));
    WorkRequest request;
    for (const auto result : pubs) {
        CHECK_ENUM(Publication::Unavailable, publish(bridge));
        TEST_ASSERT_TRUE(bridge.take(request));
        TEST_ASSERT_TRUE(bridge.complete(request.ticket, result));
        CHECK_ENUM(result, publish(bridge));
        TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
    }
    for (const auto result : accepts) {
        CHECK_ENUM(Acceptance::Unavailable, accept(bridge));
        TEST_ASSERT_TRUE(bridge.take(request));
        TEST_ASSERT_TRUE(bridge.complete(request.ticket, result));
        CHECK_ENUM(result, accept(bridge));
        TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
    }
    for (const auto result : receipts) {
        CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
        TEST_ASSERT_TRUE(bridge.take(request));
        TEST_ASSERT_TRUE(bridge.complete(request.ticket, result));
        CHECK_ENUM(result, record(bridge));
        TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
    }
}

void test_fact_target_bytes_and_length_must_match_exactly() {
    for (int changed = 0; changed != 9; ++changed) {
        EventDurableWorker bridge(id(90));
        CHECK_ENUM(Publication::Unavailable, publish(bridge));
        WorkRequest request;
        TEST_ASSERT_TRUE(bridge.take(request));
        TEST_ASSERT_TRUE(bridge.complete(request.ticket, Publication::Durable));
        auto value = fact();
        auto target = id(6);
        auto data = Input;
        auto size = data.size();
        switch (changed) {
        case 0:
            value.id.event = id(10);
            break;
        case 1:
            value.id.origin = id(10);
            break;
        case 2:
            value.id.epoch = id(10);
            break;
        case 3:
            ++value.id.sequence;
            break;
        case 4:
            value.run = id(10);
            break;
        case 5:
            value.digest[0] = 10;
            break;
        case 6:
            target = id(10);
            break;
        case 7:
            data.back() ^= 1;
            break;
        default:
            --size;
            break;
        }
        CHECK_ENUM(Publication::Unavailable, bridge.mayPublish(value, target, data.data(), size));
        TEST_ASSERT_EQUAL_UINT(2, bridge.outstanding());
        CHECK_ENUM(Publication::Durable, publish(bridge));
        TEST_ASSERT_EQUAL_UINT(1, bridge.outstanding());
    }
}

void test_receipt_requires_every_field_and_bytes() {
    for (int changed = 0; changed != 13; ++changed) {
        EventDurableWorker bridge(id(90));
        CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
        WorkRequest request;
        TEST_ASSERT_TRUE(bridge.take(request));
        TEST_ASSERT_TRUE(bridge.complete(request.ticket, ReceiptCommit::TargetDurable));
        auto value = receipt();
        auto data = Input;
        auto size = data.size();
        switch (changed) {
        case 0:
            value.fact.id.event = id(10);
            break;
        case 1:
            value.fact.id.origin = id(10);
            break;
        case 2:
            value.fact.id.epoch = id(10);
            break;
        case 3:
            ++value.fact.id.sequence;
            break;
        case 4:
            value.fact.run = id(10);
            break;
        case 5:
            value.fact.digest[0] = 10;
            break;
        case 6:
            value.confirmer = id(10);
            break;
        case 7:
            value.recipient = id(10);
            break;
        case 8:
            value.receiptId = id(10);
            break;
        case 9:
            value.stage = Stage::Rejected;
            break;
        case 10:
            value.reason = Reason::Gap;
            break;
        case 11:
            data.back() ^= 1;
            break;
        default:
            --size;
            break;
        }
        CHECK_ENUM(ReceiptCommit::Unavailable, bridge.recordReceipt(value, data.data(), size));
        TEST_ASSERT_EQUAL_UINT(2, bridge.outstanding());
        CHECK_ENUM(ReceiptCommit::TargetDurable, record(bridge));
        TEST_ASSERT_EQUAL_UINT(1, bridge.outstanding());
    }
}

void test_global_capacity_covers_running_and_completed_slots() {
    EventDurableWorker bridge(id(90));
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge));
    CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
    CHECK_ENUM(Publication::Unavailable, publish(bridge, fact(2)));
    CHECK_ENUM(Publication::Unavailable, publish(bridge, fact(3)));
    CHECK_ENUM(HandoffStatus::Full, bridge.status());
    TEST_ASSERT_EQUAL_UINT(WorkerCapacity, bridge.outstanding());
    std::array<WorkRequest, WorkerCapacity> requests;
    for (auto &request : requests)
        TEST_ASSERT_TRUE(bridge.take(request));
    WorkRequest empty;
    TEST_ASSERT_FALSE(bridge.take(empty));
    TEST_ASSERT_TRUE(empty.ticket.sequence == 0);
    TEST_ASSERT_EQUAL_UINT(0, empty.size);
    CHECK_ENUM(WorkKind::Publish, requests[0].kind);
    CHECK_ENUM(WorkKind::Accept, requests[1].kind);
    CHECK_ENUM(WorkKind::Receipt, requests[2].kind);
    TEST_ASSERT_TRUE(requests[3].fact.id.sequence == 2);
    TEST_ASSERT_TRUE(bridge.complete(requests[0].ticket, Publication::Durable));
    CHECK_ENUM(Publication::Unavailable, publish(bridge, fact(3)));
    CHECK_ENUM(HandoffStatus::Full, bridge.status());
    CHECK_ENUM(Publication::Durable, publish(bridge));
    CHECK_ENUM(Publication::Unavailable, publish(bridge, fact(3)));
    CHECK_ENUM(HandoffStatus::Queued, bridge.status());
    TEST_ASSERT_EQUAL_UINT(WorkerCapacity, bridge.outstanding());
    TEST_ASSERT_TRUE(bridge.take(empty));
    TEST_ASSERT_TRUE(empty.fact.id.sequence == 3);
}

void test_wrong_kind_unknown_enum_and_duplicate_completion_rejected() {
    EventDurableWorker bridge(id(90));
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge));
    CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
    WorkRequest request;
    TEST_ASSERT_TRUE(bridge.take(request));
    TEST_ASSERT_FALSE(bridge.complete(request.ticket, Acceptance::Durable));
    TEST_ASSERT_FALSE(bridge.complete(request.ticket, ReceiptCommit::TargetDurable));
    TEST_ASSERT_FALSE(bridge.complete(request.ticket, static_cast<Publication>(99)));
    TEST_ASSERT_TRUE(bridge.isCurrent(request.ticket));
    TEST_ASSERT_TRUE(bridge.complete(request.ticket, Publication::Durable));
    TEST_ASSERT_FALSE(bridge.isCurrent(request.ticket));
    TEST_ASSERT_FALSE(bridge.complete(request.ticket, Publication::Denied));
    CHECK_ENUM(Publication::Durable, publish(bridge));
    TEST_ASSERT_TRUE(bridge.take(request));
    TEST_ASSERT_FALSE(bridge.complete(request.ticket, static_cast<Acceptance>(99)));
    TEST_ASSERT_TRUE(bridge.complete(request.ticket, Acceptance::Durable));
    CHECK_ENUM(Acceptance::Durable, accept(bridge));
    TEST_ASSERT_TRUE(bridge.take(request));
    TEST_ASSERT_FALSE(bridge.complete(request.ticket, static_cast<ReceiptCommit>(99)));
    TEST_ASSERT_TRUE(bridge.complete(request.ticket, ReceiptCommit::Retained));
    CHECK_ENUM(ReceiptCommit::Retained, record(bridge));
}

void test_generation_fences_pending_running_and_completed_results() {
    EventDurableWorker bridge(id(90));
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge));
    CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
    WorkRequest completed;
    WorkRequest running;
    TEST_ASSERT_TRUE(bridge.take(completed));
    TEST_ASSERT_TRUE(bridge.complete(completed.ticket, Publication::Durable));
    TEST_ASSERT_TRUE(bridge.take(running));
    TEST_ASSERT_TRUE(bridge.isCurrent(running.ticket));
    TEST_ASSERT_TRUE(bridge.invalidate());
    TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
    TEST_ASSERT_FALSE(bridge.isCurrent(running.ticket));
    TEST_ASSERT_FALSE(bridge.complete(running.ticket, Acceptance::Durable));
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    WorkRequest fresh;
    TEST_ASSERT_TRUE(bridge.take(fresh));
    TEST_ASSERT_TRUE(fresh.ticket.generation > completed.ticket.generation);
    TEST_ASSERT_TRUE(fresh.ticket.sequence > running.ticket.sequence);
    TEST_ASSERT_FALSE(bridge.complete(completed.ticket, Publication::Durable));
    TEST_ASSERT_TRUE(bridge.complete(fresh.ticket, Publication::Denied));
    CHECK_ENUM(Publication::Denied, publish(bridge));
}

void test_new_incarnation_rejects_old_instance_ticket() {
    WorkTicket old;
    {
        EventDurableWorker bridge(id(90));
        CHECK_ENUM(Publication::Unavailable, publish(bridge));
        WorkRequest request;
        TEST_ASSERT_TRUE(bridge.take(request));
        old = request.ticket;
        // No access to this instance survives destruction; only a copied token.
    }
    EventDurableWorker next(id(91));
    CHECK_ENUM(Publication::Unavailable, publish(next));
    WorkRequest request;
    TEST_ASSERT_TRUE(next.take(request));
    TEST_ASSERT_TRUE(request.ticket.sequence == old.sequence);
    TEST_ASSERT_TRUE(request.ticket.generation == old.generation);
    TEST_ASSERT_FALSE(next.isCurrent(old));
    TEST_ASSERT_FALSE(next.complete(old, Publication::Durable));
    TEST_ASSERT_TRUE(next.complete(request.ticket, Publication::Denied));
    CHECK_ENUM(Publication::Denied, publish(next));
}

void test_reordered_results_stay_bound_to_their_requests() {
    EventDurableWorker bridge(id(90));
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge, fact(1)));
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge, fact(2)));
    WorkRequest first;
    WorkRequest second;
    TEST_ASSERT_TRUE(bridge.take(first));
    TEST_ASSERT_TRUE(bridge.take(second));
    TEST_ASSERT_TRUE(bridge.complete(second.ticket, Acceptance::AlreadyDurable));
    CHECK_ENUM(Acceptance::Unavailable, accept(bridge, fact(1)));
    CHECK_ENUM(Acceptance::AlreadyDurable, accept(bridge, fact(2)));
    TEST_ASSERT_TRUE(bridge.complete(first.ticket, Acceptance::Conflict));
    CHECK_ENUM(Acceptance::Conflict, accept(bridge, fact(1)));
    TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
}

void test_semantic_conflict_invalidation_drops_prepared_positive_receipt() {
    EventDurableWorker bridge(id(90));
    SimulatedSemanticPort semantic;
    semantic.receiptResult = ReceiptCommit::TargetDurable;
    CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    // The external owner fences before publishing its new semantic conflict.
    TEST_ASSERT_TRUE(bridge.invalidate());
    semantic.receiptResult = ReceiptCommit::Conflict;
    CHECK_ENUM(ReceiptCommit::Unavailable, record(bridge));
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    CHECK_ENUM(ReceiptCommit::Conflict, record(bridge));
}

void test_invalid_lengths_and_exact_message_limit() {
    EventDurableWorker bridge(id(90));
    std::array<std::uint8_t, MessageLimit + 1> input{};
    CHECK_ENUM(Publication::Unavailable, bridge.mayPublish(fact(), id(6), nullptr, 1));
    CHECK_ENUM(HandoffStatus::Invalid, bridge.status());
    CHECK_ENUM(Acceptance::Unavailable, bridge.accept(fact(), input.data(), 0));
    CHECK_ENUM(ReceiptCommit::Unavailable,
               bridge.recordReceipt(receipt(), input.data(), input.size()));
    TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
    input.front() = 23;
    input[MessageLimit - 1] = 94;
    CHECK_ENUM(Acceptance::Unavailable, bridge.accept(fact(), input.data(), MessageLimit));
    WorkRequest request;
    TEST_ASSERT_TRUE(bridge.take(request));
    TEST_ASSERT_EQUAL_UINT(MessageLimit, request.size);
    TEST_ASSERT_EQUAL_MEMORY(input.data(), request.bytes.data(), MessageLimit);
    TEST_ASSERT_TRUE(bridge.complete(request.ticket, Acceptance::AlreadyDurable));
    CHECK_ENUM(Acceptance::AlreadyDurable, bridge.accept(fact(), input.data(), MessageLimit));
}

void test_dispatch_invalidation_drops_positive_completion() {
    EventDurableWorker bridge(id(90));
    SimulatedSemanticPort semantic;
    semantic.invalidateDuringCall = &bridge;
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(ServiceResult::CompletionRejected, serviceOne(bridge, semantic));
    TEST_ASSERT_EQUAL_UINT(1, semantic.publishCalls);
    TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
    CHECK_ENUM(Publication::Unavailable, publish(bridge));
    CHECK_ENUM(ServiceResult::CompletionRejected, serviceOne(bridge, bridge));
    TEST_ASSERT_EQUAL_UINT(1, bridge.outstanding());
    semantic.invalidateDuringCall = nullptr;
    semantic.publication = Publication::Denied;
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    CHECK_ENUM(Publication::Denied, publish(bridge));
}

class SimulatedVerifier : public OriginVerifier {
  public:
    Verification verifyFact(const std::uint8_t *, std::size_t, Fact &value) override {
        value = fact(); // Test-only D0 verification decision, not actual COSE.
        return Verification::Verified;
    }
    Verification verifyReceipt(const std::uint8_t *, std::size_t, Receipt &value) override {
        value = receipt();
        value.stage = Stage::Durable;
        value.reason = Reason::None;
        return Verification::Verified;
    }
};
void test_actual_d2_core_waits_for_worker_on_each_publication() {
    EventDurableWorker bridge(id(90));
    SimulatedVerifier verifier;
    SimulatedSemanticPort semantic;
    Core core(verifier, bridge);
    CHECK_ENUM(Admission::Unavailable, core.enqueue(Input.data(), Input.size(), id(6)));
    TEST_ASSERT_EQUAL_UINT(0, core.outgoingCount());
    TEST_ASSERT_EQUAL_UINT(0, semantic.publishCalls);
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    CHECK_ENUM(Admission::Queued, core.enqueue(Input.data(), Input.size(), id(6)));
    Attempt attempt;
    CHECK_ENUM(Poll::Blocked, core.poll(0, Bearer::EspNow, true, attempt));
    TEST_ASSERT_EQUAL_UINT(0, attempt.size);
    TEST_ASSERT_EQUAL_UINT(1, semantic.publishCalls);
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    CHECK_ENUM(Poll::Ready, core.poll(100, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_MEMORY(Input.data(), attempt.bytes.data(), Input.size());
    CHECK_ENUM(Poll::Blocked, core.poll(300, Bearer::EspNow, true, attempt));
    TEST_ASSERT_EQUAL_UINT(2, semantic.publishCalls);
    semantic.publication = Publication::Denied;
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    CHECK_ENUM(Poll::Blocked, core.poll(700, Bearer::Wlan, true, attempt));
    TEST_ASSERT_EQUAL_UINT(0, attempt.size);
    CHECK_ENUM(Poll::Idle, core.poll(2000, Bearer::Wlan, true, attempt));
}

void test_actual_d2_incoming_stays_pending_until_worker_acceptance() {
    EventDurableWorker bridge(id(90));
    SimulatedVerifier verifier;
    SimulatedSemanticPort semantic;
    Core core(verifier, bridge);
    CHECK_ENUM(Admission::Queued, core.receive(Input.data(), Input.size()));
    IncomingResult result;
    CHECK_ENUM(Poll::Ready, core.processIncoming(0, result));
    CHECK_ENUM(Acceptance::Unavailable, result.acceptance);
    TEST_ASSERT_EQUAL_UINT(1, core.incomingCount());
    TEST_ASSERT_EQUAL_UINT(0, semantic.acceptCalls);
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    CHECK_ENUM(Poll::Ready, core.processIncoming(100, result));
    CHECK_ENUM(Acceptance::Durable, result.acceptance);
    TEST_ASSERT_EQUAL_UINT(0, core.incomingCount());
}

void test_actual_d2_receipt_waits_for_exact_worker_confirmation() {
    EventDurableWorker bridge(id(90));
    SimulatedVerifier verifier;
    SimulatedSemanticPort semantic;
    Core core(verifier, bridge);
    CHECK_ENUM(Admission::Unavailable, core.enqueue(Input.data(), Input.size(), id(6)));
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    CHECK_ENUM(Admission::Queued, core.enqueue(Input.data(), Input.size(), id(6)));
    semantic.receiptResult = ReceiptCommit::TargetDurable;
    const std::array<std::uint8_t, 3> originalReceipt{{49, 51, 53}};
    CHECK_ENUM(ReceiptResult::Unavailable,
               core.receipt(0, originalReceipt.data(), originalReceipt.size()));
    TEST_ASSERT_EQUAL_UINT(1, core.outgoingCount());
    TEST_ASSERT_EQUAL_UINT(0, semantic.receiptCalls);
    CHECK_ENUM(ServiceResult::Completed, serviceOne(bridge, semantic));
    TEST_ASSERT_EQUAL_MEMORY(originalReceipt.data(), semantic.observed.bytes.data(),
                             originalReceipt.size());
    CHECK_ENUM(ReceiptResult::Released,
               core.receipt(100, originalReceipt.data(), originalReceipt.size()));
    TEST_ASSERT_EQUAL_UINT(0, core.outgoingCount());
}

void test_bounded_reuse_and_stale_token_stress() {
    EventDurableWorker bridge(id(90));
    WorkTicket previous;
    for (std::uint64_t i = 1; i <= 4096; ++i) {
        CHECK_ENUM(Acceptance::Unavailable, accept(bridge, fact(i)));
        TEST_ASSERT_EQUAL_UINT(1, bridge.outstanding());
        WorkRequest request;
        TEST_ASSERT_TRUE(bridge.take(request));
        TEST_ASSERT_TRUE(request.ticket.sequence > previous.sequence);
        TEST_ASSERT_FALSE(bridge.complete(previous, Acceptance::Durable));
        TEST_ASSERT_TRUE(bridge.complete(request.ticket, Acceptance::Durable));
        CHECK_ENUM(Acceptance::Durable, accept(bridge, fact(i)));
        TEST_ASSERT_EQUAL_UINT(0, bridge.outstanding());
        previous = request.ticket;
    }
    TEST_ASSERT_FALSE(bridge.exhausted());
    static_assert(!std::is_copy_constructible<EventDurableWorker>::value, "No bridge copies");
    static_assert(!std::is_move_constructible<EventDurableWorker>::value, "No bridge moves");
    std::printf("EventDurableWorker=%u bytes; WorkRequest=%u bytes; capacity=%u shared slots\n",
                static_cast<unsigned>(sizeof(EventDurableWorker)),
                static_cast<unsigned>(sizeof(WorkRequest)), static_cast<unsigned>(WorkerCapacity));
}
} // namespace

void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_default_lifecycle_and_semantic_port_deny);
    RUN_TEST(test_dispatch_preserves_all_operations_and_original_evidence);
    RUN_TEST(test_copies_and_repeated_poll_do_not_duplicate_work);
    RUN_TEST(test_all_outcomes_are_typed_and_consumed_once);
    RUN_TEST(test_fact_target_bytes_and_length_must_match_exactly);
    RUN_TEST(test_receipt_requires_every_field_and_bytes);
    RUN_TEST(test_global_capacity_covers_running_and_completed_slots);
    RUN_TEST(test_wrong_kind_unknown_enum_and_duplicate_completion_rejected);
    RUN_TEST(test_generation_fences_pending_running_and_completed_results);
    RUN_TEST(test_new_incarnation_rejects_old_instance_ticket);
    RUN_TEST(test_reordered_results_stay_bound_to_their_requests);
    RUN_TEST(test_semantic_conflict_invalidation_drops_prepared_positive_receipt);
    RUN_TEST(test_invalid_lengths_and_exact_message_limit);
    RUN_TEST(test_dispatch_invalidation_drops_positive_completion);
    RUN_TEST(test_actual_d2_core_waits_for_worker_on_each_publication);
    RUN_TEST(test_actual_d2_incoming_stays_pending_until_worker_acceptance);
    RUN_TEST(test_actual_d2_receipt_waits_for_exact_worker_confirmation);
    RUN_TEST(test_bounded_reuse_and_stale_token_stress);
    return UNITY_END();
}
