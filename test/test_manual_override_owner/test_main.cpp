// Synthetic authority/transaction state-machine tests only. These are NOT
// real GM admission, signing, event installation or durable D1 evidence.
#include "GameExecution/ManualOverrideOwner.h"
#include <cstring>
#include <unity.h>
using namespace larp::execution::rules;
namespace ir = larp::configuration::module_ir;
namespace {
Identity id(unsigned n) {
    Identity v;
    v.bytes[15] = static_cast<std::uint8_t>(n);
    return v;
}
bool equal(const Identity &a, const Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
struct Hash final : larp::configuration::IPackageDigest {
    unsigned pos = 0;
    std::uint8_t value[32] = {};
    bool fail = false;
    bool begin() override {
        pos = 0;
        std::memset(value, 0, 32);
        return !fail;
    }
    bool update(const std::uint8_t *p, std::size_t n) override {
        for (std::size_t i = 0; i < n; ++i) {
            value[pos % 32] = static_cast<std::uint8_t>(value[pos % 32] * 33u + p[i] + pos);
            ++pos;
        }
        return !fail;
    }
    bool finish(std::uint8_t *p, std::size_t &n) override {
        n = 0;
        if (fail)
            return false;
        std::memcpy(p, value, 32);
        n = 32;
        return true;
    }
    bool abort() override {
        return true;
    }
};
struct Clock final : IManualClock {
    std::uint64_t time = 10;
    bool available = true;
    bool now(std::uint64_t &out) override {
        out = time;
        return available;
    }
};
struct Authority final : IManualAuthority {
    OverrideAuthority permitted;
    ManualAuthorityResult result = ManualAuthorityResult::Current;
    ManualAuthorityResult check(const Context &, const OverrideAuthority &a) override {
        if (a.roleEpoch != permitted.roleEpoch || a.connectionToken != permitted.connectionToken ||
            a.lifecycle != permitted.lifecycle)
            return ManualAuthorityResult::Stale;
        return result;
    }
};
struct Identities final : IManualIdentitySource {
    unsigned next = 100;
    bool available = true;
    bool make(Identity &out) override {
        out = id(next++);
        return available;
    }
};
struct Transaction final : IManualTransaction {
    unsigned calls = 0;
    ManualStatus outcome = ManualStatus::Committed;
    ReleaseDisposition release = ReleaseDisposition::Unresolved;
    OverrideRecord captured;
    bool alterRecord = false;
    IManualFinalFence *retained = nullptr;
    ManualResult submit(const OverrideAdmission &admission, IManualFinalFence &fence) override {
        ++calls;
        captured = admission.record();
        ManualResult r;
        r.operation = captured.operation;
        r.release = release;
        retained = &fence;
        OverrideRecord checkedRecord = captured;
        if (alterRecord)
            checkedRecord.confirmedAt++;
        const auto checked = fence.check(checkedRecord);
        r.status = checked == ManualAuthorityResult::Current ? outcome : ManualStatus::Rejected;
        r.error =
            checked == ManualAuthorityResult::Current ? ManualError::None : ManualError::Stale;
        return r;
    }
    ManualResult status(const Context &, const Identity &op) override {
        ManualResult r;
        r.operation = op;
        r.status = outcome;
        return r;
    }
};
struct Fixture {
    Program program;
    State state;
    Rule rules[2];
    ir::Action actions[2];
    Latch latch;
    ManualReplay replays[16];
    std::uint8_t catalogBytes[4096];
    ManualWorkspace work;
    Hash hash;
    Clock clock;
    Authority authority;
    Identities ids;
    Transaction transaction;
    OverrideAuthority actor;
    ManualOverrideOwner owner;
    Fixture() : owner(program, state, id(50), 30, hash, clock, authority, ids, transaction, work) {
        program.language = 2;
        program.manualOverride = true;
        program.ready = true;
        program.context.event = id(1);
        program.context.run = id(2);
        program.context.station = id(3);
        program.context.box = id(4);
        program.context.sourceEpoch = id(5);
        program.context.bindingGeneration = 1;
        state.context = program.context;
        state.mode = Mode::Running;
        state.fence.rights = 1;
        program.rules.data = rules;
        program.rules.count = program.rules.capacity = 2;
        program.actions.data = actions;
        program.actions.count = program.actions.capacity = 2;
        rules[0].stable = rules[1].stable = id(10);
        auto &a = rules[0].ir;
        auto &b = rules[1].ir;
        a.group = b.group = 1;
        a.once = b.once = true;
        a.trigger = 3;
        b.trigger = 4;
        a.policy = 3;
        b.policy = 2;
        a.fallback = 2;
        a.generation = 1;
        b.generation = 7;
        a.actionCount = b.actionCount = 1;
        b.firstAction = 1;
        actions[0].op = ir::Op::Emit;
        actions[1].op = ir::Op::Override;
        actions[0].target = actions[1].target = 1;
        actor.peer = id(9);
        actor.principal = 2;
        actor.roleEpoch = 3;
        actor.connectionToken = 4;
        actor.registration = 5;
        actor.lifecycle = 6;
        actor.registryRevision = 7;
        actor.eventRevision = 1;
        authority.permitted = actor;
        work.replays.data = replays;
        work.replays.capacity = 16;
        work.maximumActive = 4;
        work.catalogBytes.data = catalogBytes;
        work.catalogBytes.capacity = sizeof(catalogBytes);
    }
    ManualPrepareRequest prepareRequest(unsigned phase = 20) {
        ManualView view;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Prepared),
                              static_cast<int>(owner.view(id(10), actor, view).status));
        ManualPrepareRequest r;
        r.ownerBoot = view.ownerBoot;
        r.phase = id(phase);
        r.rule = view.rule;
        r.view = view.digest;
        r.authority = actor;
        return r;
    }
    ManualConfirmRequest confirmRequest(const ManualResult &p, unsigned phase = 30) {
        ManualConfirmRequest r;
        r.ownerBoot = id(50);
        r.token = p.token;
        r.phase = id(phase);
        r.authority = actor;
        return r;
    }
};
void test_prepare_has_no_effect_and_replay_never_refreshes_deadline() {
    Fixture f;
    const auto request = f.prepareRequest();
    const auto first = f.owner.prepare(request);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Prepared), static_cast<int>(first.status));
    TEST_ASSERT_EQUAL(40, first.expiresAt);
    TEST_ASSERT_EQUAL(0, f.transaction.calls);
    TEST_ASSERT_EQUAL(0, f.state.latches.count);
    f.clock.time = 35;
    const auto again = f.owner.prepare(request);
    TEST_ASSERT_TRUE(equal(first.token, again.token));
    TEST_ASSERT_EQUAL(40, again.expiresAt);
    TEST_ASSERT_EQUAL(1, f.work.replays.count);
    auto changed = request;
    changed.view.bytes[0] ^= 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Conflict),
                          static_cast<int>(f.owner.prepare(changed).error));
}
void test_confirm_freezes_operation_time_and_original_authority() {
    Fixture f;
    const auto p = f.owner.prepare(f.prepareRequest());
    auto request = f.confirmRequest(p);
    const auto first = f.owner.confirm(request);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Committed),
                          static_cast<int>(first.status));
    TEST_ASSERT_EQUAL(10, f.transaction.captured.confirmedAt);
    TEST_ASSERT_EQUAL(7, f.transaction.captured.replacementGeneration);
    const auto captured = f.transaction.captured;
    f.clock.time = 20;
    const auto again = f.owner.confirm(request);
    TEST_ASSERT_TRUE(equal(first.operation, again.operation));
    TEST_ASSERT_EQUAL(1, f.transaction.calls);
    TEST_ASSERT_TRUE(sameOverrideRecord(captured, f.transaction.captured));
    request.phase = id(31);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Conflict),
                          static_cast<int>(f.owner.confirm(request).error));
}
void test_revoke_regrant_and_recycled_link_do_not_revive_grant() {
    Fixture f;
    const auto p = f.owner.prepare(f.prepareRequest());
    auto request = f.confirmRequest(p);
    f.authority.permitted.roleEpoch = 8;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Stale),
                          static_cast<int>(f.owner.confirm(request).error));
    request.authority.roleEpoch = 8;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Conflict),
                          static_cast<int>(f.owner.confirm(request).error));
    f.authority.permitted = f.actor;
    f.authority.permitted.lifecycle = 99;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Stale),
                          static_cast<int>(f.owner.confirm(f.confirmRequest(p)).error));
    TEST_ASSERT_EQUAL(0, f.transaction.calls);
}
void test_original_win_requires_exact_context_and_current_authority() {
    Fixture f;
    const auto p = f.owner.prepare(f.prepareRequest());
    auto request = f.confirmRequest(p);
    f.latch.rule = id(10);
    f.state.latches.data = &f.latch;
    f.state.latches.count = 1;
    ++f.state.fence.progress;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::AlreadyReleased),
                          static_cast<int>(f.owner.confirm(request).status));
    TEST_ASSERT_EQUAL(0, f.transaction.calls);
    f.state.context.run = id(88);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Context),
                          static_cast<int>(f.owner.confirm(request).error));
}
void test_deadline_inclusive_and_pending_revocation_terminates_without_new_operation() {
    Fixture f;
    const auto p = f.owner.prepare(f.prepareRequest());
    f.clock.time = 40;
    f.transaction.outcome = ManualStatus::Confirming;
    const auto started = f.owner.confirm(f.confirmRequest(p));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Confirming),
                          static_cast<int>(started.status));
    f.clock.time = 41;
    const auto terminal = f.owner.service();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Rejected),
                          static_cast<int>(terminal.status));
    TEST_ASSERT_TRUE(equal(started.operation, terminal.operation));
    TEST_ASSERT_EQUAL(2, f.transaction.calls);
}
void test_unknown_cannot_mint_retry_and_current_status_resolves_same_operation() {
    Fixture f;
    const auto p = f.owner.prepare(f.prepareRequest());
    const auto request = f.confirmRequest(p);
    f.transaction.outcome = ManualStatus::Unknown;
    const auto unknown = f.owner.confirm(request);
    f.clock.time = 200;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Unknown),
                          static_cast<int>(f.owner.confirm(request).status));
    TEST_ASSERT_EQUAL(1, f.transaction.calls);
    f.transaction.outcome = ManualStatus::Committed;
    const auto receipt = f.owner.status(f.state.context, f.actor, unknown.operation);
    TEST_ASSERT_TRUE(equal(receipt.operation, unknown.operation));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Committed),
                          static_cast<int>(f.owner.confirm(request).status));
}
void test_expired_slots_retire_but_old_request_receipts_remain() {
    Fixture f;
    for (unsigned i = 0; i < 4; ++i)
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Prepared),
                              static_cast<int>(f.owner.prepare(f.prepareRequest(20 + i)).status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Capacity),
                          static_cast<int>(f.owner.prepare(f.prepareRequest(24)).error));
    f.clock.time = 41;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Prepared),
                          static_cast<int>(f.owner.prepare(f.prepareRequest(24)).status));
    TEST_ASSERT_EQUAL(5, f.work.replays.count);
    for (unsigned i = 0; i < 4; ++i)
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Expired),
                              static_cast<int>(f.replays[i].status));
}
void test_unknown_globally_blocks_new_grants_operations_and_worker_submissions() {
    Fixture f;
    const auto one = f.prepareRequest(20), two = f.prepareRequest(21), three = f.prepareRequest(22);
    const auto first = f.owner.prepare(one), second = f.owner.prepare(two);
    f.transaction.outcome = ManualStatus::Unknown;
    const auto unknown = f.owner.confirm(f.confirmRequest(first, 30));
    const auto identities = f.ids.next;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Unknown),
                          static_cast<int>(unknown.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::RecoveryRequired),
                          static_cast<int>(f.owner.prepare(three).error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::RecoveryRequired),
                          static_cast<int>(f.owner.confirm(f.confirmRequest(second, 31)).error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::RecoveryRequired),
                          static_cast<int>(f.owner.service().error));
    TEST_ASSERT_EQUAL(identities, f.ids.next);
    TEST_ASSERT_EQUAL(1, f.transaction.calls);
}
void test_already_released_has_fixed_confirm_identity_and_does_not_bypass_expiry() {
    Fixture f;
    const auto prepared = f.owner.prepare(f.prepareRequest());
    const auto request = f.confirmRequest(prepared);
    f.latch.rule = id(10);
    f.state.latches.data = &f.latch;
    f.state.latches.count = 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::AlreadyReleased),
                          static_cast<int>(f.owner.confirm(request).status));
    TEST_ASSERT_TRUE(equal(f.replays[0].record.confirmId, request.phase));
    f.clock.time = 80;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::AlreadyReleased),
                          static_cast<int>(f.owner.confirm(request).status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Conflict),
                          static_cast<int>(f.owner.confirm(f.confirmRequest(prepared, 31)).error));
    Fixture expired;
    const auto p = expired.owner.prepare(expired.prepareRequest());
    expired.clock.time = 41;
    expired.latch.rule = id(10);
    expired.state.latches.data = &expired.latch;
    expired.state.latches.count = 1;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ManualStatus::Expired),
        static_cast<int>(expired.owner.confirm(expired.confirmRequest(p)).status));
    TEST_ASSERT_EQUAL(0, expired.transaction.calls);
}
void test_complete_catalog_and_exact_submission_scope_fence_the_request() {
    Fixture changed;
    const auto p = changed.owner.prepare(changed.prepareRequest());
    changed.rules[0].ir.priority++;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Stale),
                          static_cast<int>(changed.owner.confirm(changed.confirmRequest(p)).error));
    TEST_ASSERT_EQUAL(0, changed.transaction.calls);
    Fixture altered;
    const auto a = altered.owner.prepare(altered.prepareRequest());
    altered.transaction.alterRecord = true;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ManualStatus::Rejected),
        static_cast<int>(altered.owner.confirm(altered.confirmRequest(a)).status));
    TEST_ASSERT_NOT_NULL(altered.transaction.retained);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ManualAuthorityResult::Stale),
        static_cast<int>(altered.transaction.retained->check(altered.transaction.captured)));
}
void test_durable_clock_only_progress_does_not_rebase_or_expire_the_original_grant() {
    Fixture f;
    const auto originalRequest = f.prepareRequest();
    const auto prepared = f.owner.prepare(originalRequest);
    const auto original = f.replays[0].record;
    f.state.fence.activeTime = 10;
    const auto replay = f.owner.prepare(originalRequest);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Prepared),
                          static_cast<int>(replay.status));
    TEST_ASSERT_TRUE(sameOverrideRecord(original, f.replays[0].record));
    TEST_ASSERT_EQUAL(prepared.expiresAt, replay.expiresAt);
    const auto confirmed = f.owner.confirm(f.confirmRequest(prepared));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Committed),
                          static_cast<int>(confirmed.status));
    TEST_ASSERT_EQUAL(0, f.transaction.captured.expected.activeTime);
    TEST_ASSERT_EQUAL(10, f.state.fence.activeTime);
}
void test_status_unknown_is_latched_and_terminal_status_cannot_be_rewritten() {
    Fixture f;
    const auto prepared = f.owner.prepare(f.prepareRequest());
    f.transaction.outcome = ManualStatus::Confirming;
    const auto pending = f.owner.confirm(f.confirmRequest(prepared));
    f.transaction.outcome = ManualStatus::Unknown;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ManualStatus::Unknown),
        static_cast<int>(f.owner.status(f.state.context, f.actor, pending.operation).status));
    const auto calls = f.transaction.calls;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::RecoveryRequired),
                          static_cast<int>(f.owner.service().error));
    TEST_ASSERT_EQUAL(calls, f.transaction.calls);
    f.transaction.outcome = ManualStatus::Committed;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ManualStatus::Committed),
        static_cast<int>(f.owner.status(f.state.context, f.actor, pending.operation).status));
    f.transaction.outcome = ManualStatus::NotCommitted;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ManualStatus::Committed),
        static_cast<int>(f.owner.status(f.state.context, f.actor, pending.operation).status));
}
void test_committed_receipt_does_not_automatically_claim_replacement_release() {
    Fixture f;
    const auto p = f.owner.prepare(f.prepareRequest());
    f.transaction.release = ReleaseDisposition::NoRelease;
    const auto first = f.owner.confirm(f.confirmRequest(p));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualStatus::Committed),
                          static_cast<int>(first.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReleaseDisposition::NoRelease),
                          static_cast<int>(first.release));
    f.transaction.release = ReleaseDisposition::Released;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReleaseDisposition::NoRelease),
                          static_cast<int>(f.owner.confirm(f.confirmRequest(p)).release));
    TEST_ASSERT_EQUAL(0, f.state.latches.count); // Synthetic no-fire result, no T2/physical claim.
}
void test_target_generation_context_clock_and_crypto_fail_closed() {
    Fixture f;
    const auto p = f.owner.prepare(f.prepareRequest());
    const auto request = f.confirmRequest(p);
    f.rules[1].ir.generation = 8;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Context),
                          static_cast<int>(f.owner.confirm(request).error));
    TEST_ASSERT_EQUAL(0, f.transaction.calls);
    f.rules[1].ir.generation = 7;
    f.clock.time = 9;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Unavailable),
                          static_cast<int>(f.owner.confirm(request).error));
    f.clock.time = 10;
    f.hash.fail = true;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ManualError::Crypto),
                          static_cast<int>(f.owner.confirm(request).error));
    TEST_ASSERT_EQUAL(0, f.transaction.calls);
    TEST_ASSERT_FALSE(equal(f.replays[0].record.operation, id(100)));
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_prepare_has_no_effect_and_replay_never_refreshes_deadline);
    RUN_TEST(test_confirm_freezes_operation_time_and_original_authority);
    RUN_TEST(test_revoke_regrant_and_recycled_link_do_not_revive_grant);
    RUN_TEST(test_original_win_requires_exact_context_and_current_authority);
    RUN_TEST(test_deadline_inclusive_and_pending_revocation_terminates_without_new_operation);
    RUN_TEST(test_unknown_cannot_mint_retry_and_current_status_resolves_same_operation);
    RUN_TEST(test_expired_slots_retire_but_old_request_receipts_remain);
    RUN_TEST(test_target_generation_context_clock_and_crypto_fail_closed);
    RUN_TEST(test_committed_receipt_does_not_automatically_claim_replacement_release);
    RUN_TEST(test_status_unknown_is_latched_and_terminal_status_cannot_be_rewritten);
    RUN_TEST(test_durable_clock_only_progress_does_not_rebase_or_expire_the_original_grant);
    RUN_TEST(test_unknown_globally_blocks_new_grants_operations_and_worker_submissions);
    RUN_TEST(test_already_released_has_fixed_confirm_identity_and_does_not_bypass_expiry);
    RUN_TEST(test_complete_catalog_and_exact_submission_scope_fence_the_request);
    return UNITY_END();
}
