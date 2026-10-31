#include "GameExecution/RulesPlanner.h"
#include <cstring>
#include <limits>
#include <unity.h>
using namespace larp::execution::rules;
namespace ir = larp::configuration::module_ir;
using larp::configuration::ExpandedStationWorkspace;
using larp::configuration::ModuleCompileWorkspace;
namespace {
template <typename T, std::size_t N> void pool(Pool<T> &p, T (&data)[N]) {
    p.data = data;
    p.capacity = N;
    p.count = 0;
}
Identity identity(unsigned n) {
    Identity id;
    id.bytes[15] = static_cast<std::uint8_t>(n);
    return id;
}
struct ProgramMemory {
    char text[8192];
    std::uint32_t offsets[512];
    ir::Node nodes[512];
    ir::Action actions[512];
    Rule rules[64];
    Symbol values[32], timers[16];
    Port ports[64];
    Subject subjects[128];
    Resource resources[32];
    Id triggers[4096], allowed[256];
    Identity epochs[128];
    Program program;
    ProgramMemory() {
        pool(program.strings, text);
        pool(program.offsets, offsets);
        pool(program.nodes, nodes);
        pool(program.actions, actions);
        pool(program.rules, rules);
        pool(program.values, values);
        pool(program.timers, timers);
        pool(program.ports, ports);
        pool(program.subjects, subjects);
        pool(program.resources, resources);
        pool(program.triggers, triggers);
        pool(program.allowedResources, allowed);
        pool(program.epochs, epochs);
    }
} memory;
struct StateMemory {
    Value values[32];
    Timer timers[16];
    Fact facts[128];
    Latch latches[128];
    Receipt ledger[256];
    Id solved[128];
    FactRef refs[4096];
    StateStorage storage;
    StateMemory() {
        pool(storage.values, values);
        pool(storage.timers, timers);
        pool(storage.facts, facts);
        pool(storage.latches, latches);
        pool(storage.ledger, ledger);
        pool(storage.solved, solved);
        pool(storage.evidence, refs);
    }
} first, second;
struct WorkMemory {
    ValueWrite values[32];
    TimerWrite timers[16];
    Latch latches[64];
    Emission emissions[8];
    Intent intents[8];
    FactRef evidence[4096], catalog[512];
    Proof proofs[4096];
    Id refs[32768];
    Plan plan;
    PlannerWorkspace work;
    WorkMemory() {
        pool(plan.values, values);
        pool(plan.timers, timers);
        pool(plan.latches, latches);
        pool(plan.emissions, emissions);
        pool(plan.intents, intents);
        pool(plan.evidence, evidence);
        pool(work.catalog, catalog);
        pool(work.proofs, proofs);
        pool(work.proofRefs, refs);
    }
} scratch;
Planner planner;
State state;
Context context() {
    Context c;
    c.event = identity(1);
    c.run = identity(2);
    c.station = identity(3);
    c.box = identity(4);
    c.sourceEpoch = identity(5);
    std::memset(c.content.bytes, 0x42, 32);
    std::memset(c.deployment.bytes, 0x43, 32);
    std::memset(c.binding.bytes, 0x44, 32);
    return c;
}
// Typed-consumer seam only. Independent end-to-end fixtures exercise actual
// canonical bytes and production PSA before this same builder/runtime path.
bool build(bool twoRules = false, Op operation = Op::Add, bool timerTrigger = false,
           bool guard = true, std::int32_t initial = 0, bool extraRule = false,
           bool extraSubject = false, unsigned epochMode = 0) {
    static char strings[] = "station\0p\0out\0res\0n\0r\0t\0solved\0z";
    static std::uint32_t offsets[] = {0, 8, 10, 14, 18, 20, 22, 24, 31};
    ir::Definition definitions[2];
    definitions[0].id = 1;
    definitions[1].id = 9;
    ir::Symbol symbols[4];
    symbols[0].owner = 1;
    symbols[0].id = 2;
    symbols[0].kind = ir::SymbolKind::Port;
    symbols[0].input = true;
    symbols[0].type = Type::Integer;
    symbols[1].owner = 1;
    symbols[1].id = 3;
    symbols[1].kind = ir::SymbolKind::Port;
    symbols[1].type = Type::Light;
    symbols[2].id = 4;
    symbols[2].kind = ir::SymbolKind::Resource;
    symbols[2].type = Type::Light;
    symbols[3].id = 8;
    symbols[3].auxiliary = epochMode >= 2 ? 9 : 1;
    symbols[3].kind = ir::SymbolKind::Subject;
    symbols[3].type = Type::Solved;
    ir::Node nodes[2];
    nodes[0].op = Op::Literal;
    nodes[0].type = Type::Boolean;
    nodes[0].value = guard ? 1 : 0;
    nodes[1].op = Op::Literal;
    nodes[1].type = Type::Integer;
    nodes[1].value = 1;
    ir::Action actions[1];
    actions[0].op = operation;
    actions[0].target = operation == Op::Emit ? 8 : 1;
    actions[0].expr = operation == Op::Add || operation == Op::Set ? 2 : 0;
    ir::ExpandedRule rules[2];
    for (unsigned i = 0; i < 2; ++i) {
        rules[i].name.count = 1;
        rules[i].name.segments[0] = i ? 9 : 6;
        auto &r = rules[i].rule;
        r.generation = 1;
        r.guard = 1;
        r.actionCount = 1;
        r.trigger = timerTrigger ? 2 : 1;
        r.timer = timerTrigger ? 1 : 0;
        r.triggerCount = timerTrigger ? 0 : 1;
        r.policy = 2;
        r.once = operation == Op::Emit;
    }
    ir::ExpandedSymbol values[1], timers[1];
    values[0].name.count = 1;
    values[0].name.segments[0] = 5;
    values[0].type = Type::Integer;
    values[0].initial.type = Type::Integer;
    values[0].initial.value = initial;
    timers[0].name.count = 1;
    timers[0].name.segments[0] = 7;
    timers[0].type = Type::Integer;
    timers[0].initial.type = Type::Integer;
    timers[0].initial.value = 3;
    Id triggers[1] = {2};
    ModuleCompileWorkspace source;
    pool(source.strings, strings);
    source.strings.count = sizeof(strings);
    pool(source.stringOffsets, offsets);
    source.stringOffsets.count = 9;
    pool(source.definitions, definitions);
    source.definitions.count = epochMode >= 2 ? 2 : 1;
    pool(source.symbols, symbols);
    source.symbols.count = 4;
    ExpandedStationWorkspace expanded;
    pool(expanded.nodes, nodes);
    expanded.nodes.count = 2;
    pool(expanded.actions, actions);
    expanded.actions.count = 1;
    pool(expanded.rules, rules);
    expanded.rules.count = twoRules ? 2 : 1;
    pool(expanded.values, values);
    expanded.values.count = 1;
    pool(expanded.timers, timers);
    expanded.timers.count = 1;
    pool(expanded.triggers, triggers);
    expanded.triggers.count = timerTrigger ? 0 : 1;
    RuleBinding bindings[2];
    bindings[0].qualified = "/r";
    bindings[0].ruleId = identity(8);
    bindings[1].qualified = "/z";
    bindings[1].ruleId = identity(7);
    SubjectBinding subject;
    subject.name = "solved";
    subject.subject = identity(6);
    subject.station = identity(3);
    subject.box = identity(4);
    subject.epoch = identity(5);
    Identity allowedEpochs[] = {identity(12)};
    if (epochMode == 1)
        subject.epochsComplete = true;
    if (epochMode >= 2) {
        subject.station = identity(10);
        subject.box = identity(11);
        subject.epoch = identity(12);
        subject.epochsComplete = epochMode == 2;
        if (epochMode == 4) {
            subject.epoch = identity(13); // Nonempty spans already override convenience values.
            subject.epochs.data = allowedEpochs;
            subject.epochs.count = 1;
        }
    }
    std::memset(subject.binding.bytes, 0x44, 32);
    SubjectBinding subjects[2] = {subject, subject};
    subjects[1].name = "unknown";
    subjects[1].subject = identity(13);
    const char *resources[] = {"res"};
    PortBinding port;
    port.name = "out";
    port.exclusive = identity(9);
    port.resources.data = resources;
    port.resources.count = 1;
    Span<RuleBinding> rb;
    rb.data = bindings;
    rb.count = twoRules || extraRule ? 2 : 1;
    Span<SubjectBinding> sb;
    sb.data = subjects;
    sb.count = extraSubject ? 2 : 1;
    Span<PortBinding> pb;
    pb.data = &port;
    pb.count = 1;
    const Context c = context();
    ProgramBuilder builder(memory.program, "station", c, rb, sb, pb);
    if (!builder.begin(c.content.bytes) || !builder.station(source, expanded, 1) ||
        !builder.prepare())
        return false;
    builder.commit();
    return true;
}
void start() {
    Fence fence;
    const auto r = Planner::initialize(memory.program, fence, first.storage, state);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready), static_cast<int>(r.decision));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::None),
                          static_cast<int>(Planner::control(state, Mode::Running).error));
}
Input local(std::uint64_t sequence = 1) {
    Input input;
    input.context = memory.program.context;
    input.bootGeneration = 1;
    input.sequence = sequence;
    input.port = 1;
    input.type = Type::Integer;
    input.value = 1;
    return input;
}
Capacities capacity() {
    Capacities c;
    c.receipts = 256;
    c.facts = 128;
    c.latches = 128;
    c.outbox = 8;
    c.intents = 8;
    return c;
}
Result plan(const Input &i, Capacities c = capacity()) {
    return planner.plan(memory.program, state, i, c, scratch.plan, scratch.work);
}
void test_complete_plan_owned_commit_and_duplicate() {
    TEST_ASSERT_TRUE(build());
    start();
    const auto input = local();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(input).decision));
    TEST_ASSERT_EQUAL(0, state.values.data[0].value);
    TEST_ASSERT_EQUAL(1, scratch.plan.values.data[0].value.value);
    State next;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(Planner::commitRam(memory.program, state, scratch.plan,
                                                              second.storage, next)
                                               .decision));
    state = next;
    TEST_ASSERT_EQUAL(1, state.values.data[0].value);
    TEST_ASSERT_EQUAL(1, state.ledger.count);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Duplicate),
                          static_cast<int>(plan(input).decision));
    Input changed = input;
    changed.value = 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Conflict), static_cast<int>(plan(changed).error));
}
void test_all_rules_conflict_rolls_back() {
    TEST_ASSERT_TRUE(build(true));
    start();
    const auto result = plan(local());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Conflict), static_cast<int>(result.error));
    TEST_ASSERT_EQUAL(1, result.rule);
    TEST_ASSERT_FALSE(scratch.plan.ready);
    TEST_ASSERT_EQUAL(0, state.values.data[0].value);
    TEST_ASSERT_EQUAL(0, state.ledger.count);
}
void test_overflow_and_capacity_fail_without_partial_state() {
    TEST_ASSERT_TRUE(build(false, Op::Add, false, true, std::numeric_limits<std::int32_t>::max()));
    start();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Overflow), static_cast<int>(plan(local()).error));
    TEST_ASSERT_EQUAL(std::numeric_limits<std::int32_t>::max(), state.values.data[0].value);
    TEST_ASSERT_TRUE(build());
    start();
    auto cap = capacity();
    cap.receipts = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Deferred),
                          static_cast<int>(plan(local(), cap).decision));
    TEST_ASSERT_FALSE(scratch.plan.ready);
    TEST_ASSERT_EQUAL(0, state.values.data[0].value);
}
void test_rights_capacity_and_mode_fences() {
    TEST_ASSERT_TRUE(build());
    start();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(local()).decision));
    ++state.fence.rights;
    State next;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Error::Stale),
        static_cast<int>(
            Planner::commitRam(memory.program, state, scratch.plan, second.storage, next).error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(local()).decision));
    ++state.fence.capacity;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Error::Stale),
        static_cast<int>(
            Planner::commitRam(memory.program, state, scratch.plan, second.storage, next).error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(local()).decision));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::None),
                          static_cast<int>(Planner::control(state, Mode::Paused).error));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Error::Stale),
        static_cast<int>(
            Planner::commitRam(memory.program, state, scratch.plan, second.storage, next).error));
}
void test_state_storage_aliases_fail_before_mutation() {
    TEST_ASSERT_TRUE(build());
    start();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(local()).decision));
    State next;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Error::InvalidSnapshot),
        static_cast<int>(
            Planner::commitRam(memory.program, state, scratch.plan, first.storage, next).error));
    TEST_ASSERT_EQUAL(0, state.values.data[0].value);
    auto *saved = scratch.plan.values.data;
    const auto cap = scratch.plan.values.capacity;
    scratch.plan.values.data = reinterpret_cast<ValueWrite *>(first.values);
    scratch.plan.values.capacity = 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::InvalidSnapshot),
                          static_cast<int>(plan(local()).error));
    scratch.plan.values.data = saved;
    scratch.plan.values.capacity = cap;
    TEST_ASSERT_EQUAL(0, state.values.data[0].value);
}
void test_false_guard_needs_no_proof_workspace() {
    TEST_ASSERT_TRUE(build(false, Op::Add, false, false));
    start();
    auto *saved = scratch.work.proofs.data;
    const auto cap = scratch.work.proofs.capacity;
    scratch.work.proofs.data = nullptr;
    scratch.work.proofs.capacity = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(local()).decision));
    scratch.work.proofs.data = saved;
    scratch.work.proofs.capacity = cap;
    TEST_ASSERT_EQUAL(0, scratch.plan.values.count);
    TEST_ASSERT_EQUAL(0, scratch.plan.latches.count);
    State next;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(Planner::commitRam(memory.program, state, scratch.plan,
                                                              second.storage, next)
                                               .decision));
    TEST_ASSERT_EQUAL(1, next.ledger.count);
}
void test_consumed_timer_rearm_and_stale_candidate() {
    TEST_ASSERT_TRUE(build(false, Op::Arm, true));
    start();
    first.timers[0].active = true;
    first.timers[0].deadline = 10;
    first.timers[0].generation = 1;
    state.fence.activeTime = 10;
    Input input;
    input.context = memory.program.context;
    input.kind = InputKind::Timer;
    input.timer = 1;
    input.armGeneration = 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(input).decision));
    TEST_ASSERT_EQUAL(1, scratch.plan.consumedTimer);
    TEST_ASSERT_EQUAL(2, scratch.plan.timers.data[0].value.generation);
    TEST_ASSERT_EQUAL(13, scratch.plan.timers.data[0].value.deadline);
    State next;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(Planner::commitRam(memory.program, state, scratch.plan,
                                                              second.storage, next)
                                               .decision));
    state = next;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Duplicate),
                          static_cast<int>(plan(input).decision));
    TEST_ASSERT_EQUAL(2, state.timers.data[0].generation);
    start();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::StaleTimer),
                          static_cast<int>(plan(input).decision));
}
void test_clock_and_recovery_do_not_auto_resume() {
    TEST_ASSERT_TRUE(build());
    start();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::None),
                          static_cast<int>(Planner::advanceClock(state, 5).error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Overflow),
                          static_cast<int>(Planner::advanceClock(state, 4).error));
    TEST_ASSERT_EQUAL(5, state.fence.activeTime);
    state.mode = Mode::RecoveryRequired;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::RecoveryRequired),
                          static_cast<int>(Planner::control(state, Mode::Running).error));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::RecoveryRequired),
                          static_cast<int>(plan(local()).error));
}
Fact ownFact(std::uint64_t sequence) {
    Fact f;
    f.subject = 1;
    f.kind = FactClass::CurrentSolved;
    const auto &c = memory.program.context;
    f.ref.id.event = c.event;
    f.ref.id.origin = c.box;
    f.ref.id.epoch = c.sourceEpoch;
    f.ref.id.sequence = sequence;
    f.ref.run = c.run;
    f.ref.station = c.station;
    f.ref.subject = memory.program.subjects.data[0].binding.subject;
    f.ref.binding = c.binding;
    f.ref.deployment = c.deployment;
    std::memset(f.ref.digest.bytes, 0x91, 32);
    return f;
}
void test_own_fact_later_admission_and_all_provenance_boundaries() {
    TEST_ASSERT_TRUE(build(false, Op::Emit));
    start();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(local()).decision));
    State next;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(Planner::commitRam(memory.program, state, scratch.plan,
                                                              second.storage, next)
                                               .decision));
    state = next;
    TEST_ASSERT_EQUAL(1, state.solved.count);
    TEST_ASSERT_EQUAL(0, state.facts.count);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(plan(local(2)).decision));
    TEST_ASSERT_EQUAL(0, scratch.plan.emissions.count);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(Decision::Ready),
        static_cast<int>(
            Planner::commitRam(memory.program, state, scratch.plan, first.storage, next).decision));
    state = next;
    Input echo;
    echo.context = memory.program.context;
    echo.kind = InputKind::Fact;
    echo.fact = ownFact(1);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready), static_cast<int>(plan(echo).decision));
    TEST_ASSERT_TRUE(scratch.plan.admitsFact);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(Planner::commitRam(memory.program, state, scratch.plan,
                                                              second.storage, next)
                                               .decision));
    state = next;
    TEST_ASSERT_EQUAL(1, state.facts.count);
    echo.fact = ownFact(2);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::InvalidSnapshot),
                          static_cast<int>(plan(echo).error));
    start();
    state.fence.lastIssued = 3;
    echo.fact = ownFact(1);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::InvalidSnapshot),
                          static_cast<int>(plan(echo).error));
    first.values[0].evidence.refs.data = &echo.fact.ref;
    first.values[0].evidence.refs.count = 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::InvalidSnapshot),
                          static_cast<int>(plan(local()).error));
}
void test_expected_content_digest_is_not_silently_rebound() {
    Context expected = context();
    std::memset(expected.content.bytes, 0, 32);
    ProgramBuilder builder(memory.program, "station", expected, {}, {}, {});
    const Context actual = context();
    TEST_ASSERT_FALSE(builder.begin(actual.content.bytes));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Error::Context),
                          static_cast<int>(builder.result().error));
    TEST_ASSERT_FALSE(memory.program.ready);
}
void test_extra_deployment_bindings_are_rejected() {
    TEST_ASSERT_FALSE(build(false, Op::Add, false, true, 0, true, false));
    TEST_ASSERT_FALSE(memory.program.ready);
    TEST_ASSERT_FALSE(build(false, Op::Add, false, true, 0, false, true));
    TEST_ASSERT_FALSE(memory.program.ready);
    TEST_ASSERT_TRUE(build());
    TEST_ASSERT_TRUE(memory.program.ready);
}
void test_complete_empty_remote_epochs_and_old_convenience_are_distinct() {
    TEST_ASSERT_FALSE(build(false, Op::Add, false, true, 0, false, false, 1));
    TEST_ASSERT_FALSE(memory.program.ready); // Explicit local current epoch is mandatory.
    for (unsigned mode = 2; mode <= 4; ++mode) {
        TEST_ASSERT_TRUE(build(false, Op::Add, false, true, 0, false, false, mode));
        const auto &binding = memory.program.subjects.data[0].binding;
        TEST_ASSERT_TRUE(binding.epochsComplete);
        TEST_ASSERT_EQUAL(mode == 2 ? 0 : 1, binding.epochs.count);
        start();
        Input incoming;
        incoming.context = state.context;
        incoming.kind = InputKind::Fact;
        incoming.fact = ownFact(1);
        incoming.fact.ref.id.origin = binding.box;
        incoming.fact.ref.id.epoch = identity(12);
        incoming.fact.ref.station = binding.station;
        const auto result = plan(incoming);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(mode == 2 ? Decision::Rejected : Decision::Ready),
                              static_cast<int>(result.decision));
        TEST_ASSERT_FALSE(mode == 2 && scratch.plan.ready);
        incoming.fact.ref.id.epoch = identity(13);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Rejected),
                              static_cast<int>(plan(incoming).decision));
    }
    TEST_ASSERT_TRUE(build()); // Original one-epoch convenience API remains accepted.
}
void test_historical_override_replay_is_inactive_and_never_a_live_input() {
    // Typed historical semantics seam only, not a proof of GM authority/D1.
    TEST_ASSERT_TRUE(build(false, Op::Emit));
    auto &p = memory.program;
    p.language = 2;
    p.manualOverride = true;
    const char replacement[] = "/replacement";
    const Id label = static_cast<Id>(++p.offsets.count);
    p.offsets.data[label - 1] = static_cast<std::uint32_t>(p.strings.count);
    std::memcpy(p.strings.data + p.strings.count, replacement, sizeof(replacement));
    p.strings.count += sizeof(replacement);
    p.nodes.data[0].op = Op::Fact;
    p.nodes.data[0].ref = 1;
    p.nodes.data[0].type = Type::Invalid;
    p.nodes.data[1].op = Op::Literal;
    p.nodes.data[1].type = Type::Boolean;
    p.nodes.data[1].value = 1;
    p.rules.data[1] = p.rules.data[0];
    p.rules.count = 2;
    p.rules.data[1].name = label;
    p.actions.data[1] = p.actions.data[0];
    p.actions.data[1].op = Op::Override;
    p.actions.count = 2;
    auto &original = p.rules.data[0].ir;
    auto &fallback = p.rules.data[1].ir;
    original.trigger = 3;
    original.policy = 3;
    original.fallback = 2;
    fallback.trigger = 4;
    fallback.policy = 2;
    fallback.fallback = 0;
    fallback.firstAction = 1;
    fallback.guard = 2;
    fallback.triggerCount = 0;
    fallback.firstTrigger = 1;
    fallback.generation = 7;
    p.triggers.data[0] = 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready),
                          static_cast<int>(validateOwnedProgram(p).decision));
    p.ready = true;
    start();
    OverrideRecord record;
    record.context = state.context;
    record.expected = state.fence;
    record.original = 1;
    record.replacement = 2;
    record.originalGeneration = 1;
    record.replacementGeneration = 7;
    record.rule = p.rules.data[0].stable;
    record.ownerBoot = identity(20);
    record.token = identity(21);
    record.prepareId = identity(22);
    record.confirmId = identity(23);
    record.operation = identity(24);
    record.authority.peer = identity(30);
    record.authority.principal = 1;
    record.authority.roleEpoch = 1;
    record.authority.connectionToken = 1;
    record.authority.registration = 1;
    record.authority.lifecycle = 1;
    record.authority.registryRevision = 1;
    record.authority.eventRevision = 1;
    record.preparedAt = 1;
    record.confirmedAt = 2;
    record.expiresAt = 30;
    Input decoded;
    decoded.context = state.context;
    decoded.kind = InputKind::Override;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(OverrideAdmissionScope::Empty),
                          static_cast<int>(decoded.overrideAdmission.scope()));
    State restored;
    const auto replay =
        planner.replayHistorical(p, state, decoded, &record, capacity(), scratch.plan, scratch.work,
                                 second.storage, restored);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReplayScope::ReplayedInactive),
                          static_cast<int>(replay.scope));
    TEST_ASSERT_FALSE(scratch.plan.ready);
    TEST_ASSERT_EQUAL(1, scratch.plan.emissions.count);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FactType::LocalOverride),
                          static_cast<int>(scratch.plan.emissions.data[0].type));
    TEST_ASSERT_EQUAL(0, state.latches.count);
    TEST_ASSERT_EQUAL(1, restored.latches.count);
    const Input historical = restored.ledger.data[0].input;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(OverrideAdmissionScope::Historical),
                          static_cast<int>(historical.overrideAdmission.scope()));
    state = restored;
    // Rejection happens before the matching historical ledger could yield Duplicate.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Rejected),
                          static_cast<int>(plan(historical).decision));
    TEST_ASSERT_FALSE(scratch.plan.ready);
    TEST_ASSERT_EQUAL(1, state.latches.count);
}
void test_persistent_catalog_revalidation_checks_owned_bindings_before_semantics() {
    for (unsigned mode = 0; mode < 6; ++mode) {
        TEST_ASSERT_TRUE(build());
        auto &p = memory.program;
        if (mode == 0)
            p.ports.data[0].firstResource =
                static_cast<std::uint32_t>(p.allowedResources.count + 1);
        if (mode == 1)
            p.offsets.data[0] = static_cast<std::uint32_t>(p.strings.count);
        if (mode == 2)
            p.rules.data[0].ir.firstAction = static_cast<Id>(p.actions.count + 1);
        if (mode == 3)
            p.subjects.data[0].binding.epochs.data = nullptr;
        if (mode == 4)
            p.subjects.data[0].binding.name = "borrowed";
        if (mode == 5)
            p.rules.data[0].stable = {};
        const auto checked = validateOwnedProgram(p);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Rejected),
                              static_cast<int>(checked.decision));
        TEST_ASSERT_FALSE(p.ready);
    }
    TEST_ASSERT_TRUE(build());
    const auto checked = validateOwnedProgram(memory.program);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Ready), static_cast<int>(checked.decision));
    TEST_ASSERT_FALSE(memory.program.ready); // Consumer publication remains separate.
    TEST_ASSERT_TRUE(build());
}
void test_default_override_admission_cannot_manufacture_an_input() {
    TEST_ASSERT_TRUE(build());
    start();
    Input incoming;
    incoming.context = state.context;
    incoming.kind = InputKind::Override;
    const auto result = plan(incoming);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Decision::Rejected), static_cast<int>(result.decision));
    TEST_ASSERT_FALSE(scratch.plan.ready);
    TEST_ASSERT_EQUAL(0, state.latches.count);
    TEST_ASSERT_EQUAL(0, scratch.plan.emissions.count);
}
} // namespace
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_complete_plan_owned_commit_and_duplicate);
    RUN_TEST(test_all_rules_conflict_rolls_back);
    RUN_TEST(test_overflow_and_capacity_fail_without_partial_state);
    RUN_TEST(test_rights_capacity_and_mode_fences);
    RUN_TEST(test_state_storage_aliases_fail_before_mutation);
    RUN_TEST(test_false_guard_needs_no_proof_workspace);
    RUN_TEST(test_consumed_timer_rearm_and_stale_candidate);
    RUN_TEST(test_clock_and_recovery_do_not_auto_resume);
    RUN_TEST(test_own_fact_later_admission_and_all_provenance_boundaries);
    RUN_TEST(test_expected_content_digest_is_not_silently_rebound);
    RUN_TEST(test_extra_deployment_bindings_are_rejected);
    RUN_TEST(test_complete_empty_remote_epochs_and_old_convenience_are_distinct);
    RUN_TEST(test_default_override_admission_cannot_manufacture_an_input);
    RUN_TEST(test_persistent_catalog_revalidation_checks_owned_bindings_before_semantics);
    RUN_TEST(test_historical_override_replay_is_inactive_and_never_a_live_input);
    return UNITY_END();
}
