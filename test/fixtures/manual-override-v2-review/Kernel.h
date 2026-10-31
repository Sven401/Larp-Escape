#pragma once
// Negative kernel misuse ports only; no owner or private admission construction.
struct StateMemory {
    xr::StateStorage pools;
    std::vector<larp::configuration::module_ir::Id> solved;
    std::vector<xr::Value> values;
    std::vector<xr::Timer> timers;
    std::vector<xr::Fact> facts;
    std::vector<xr::Latch> latches;
    std::vector<xr::Receipt> receipts;
    std::vector<xr::FactRef> evidence;
    StateMemory() {
        storage(pools.solved, solved, 128);
        storage(pools.values, values, 32);
        storage(pools.timers, timers, 16);
        storage(pools.facts, facts, 256);
        storage(pools.latches, latches, 64);
        storage(pools.ledger, receipts, 1024);
        storage(pools.evidence, evidence, 8192);
    }
};
struct PlannerMemory {
    xr::Planner planner;
    xr::Plan plan;
    xr::PlannerWorkspace workspace;
    std::vector<xr::ValueWrite> values;
    std::vector<xr::TimerWrite> timers;
    std::vector<xr::Latch> latches;
    std::vector<xr::Emission> emissions;
    std::vector<xr::Intent> intents;
    std::vector<xr::FactRef> evidence, catalog;
    std::vector<xr::Proof> proofs;
    std::vector<larp::configuration::module_ir::Id> proofRefs;
    PlannerMemory() {
        storage(plan.values, values, 32);
        storage(plan.timers, timers, 16);
        storage(plan.latches, latches, 64);
        storage(plan.emissions, emissions, 8);
        storage(plan.intents, intents, 8);
        storage(plan.evidence, evidence, 8192);
        storage(workspace.catalog, catalog, 512);
        storage(workspace.proofs, proofs, 8192);
        storage(workspace.proofRefs, proofRefs, 65534);
    }
};

struct NegativeKernelResult {
    bool defaultAdmissionRejected = false;
    unsigned emptyEpochRejects = 0, emptyEpochAttempts = 0;
};
NegativeKernelResult negativeKernel(const xr::Program &program) {
    NegativeKernelResult output;
    StateMemory memory;
    PlannerMemory planner;
    xr::State state;
    xr::Fence fence;
    fence.progress = fence.profile = fence.trust = fence.facts = fence.ledger = fence.resources =
        fence.rights = fence.capacity = 1;
    const auto initialized = xr::Planner::initialize(program, fence, memory.pools, state);
    if (initialized.decision != xr::Decision::Ready)
        return output;
    xr::Planner::control(state, xr::Mode::Running);
    xr::Capacities caps;
    caps.receipts = caps.facts = caps.latches = 128;
    caps.outbox = caps.intents = 8;
    auto rejected = [&](const xr::Input &input) {
        const auto r =
            planner.planner.plan(program, state, input, caps, planner.plan, planner.workspace);
        return r.decision == xr::Decision::Rejected && !planner.plan.ready &&
               !planner.plan.admitsFact && !planner.plan.emissions.count &&
               !planner.plan.intents.count && !planner.plan.latches.count;
    };
    xr::Input overrideInput;
    overrideInput.context = program.context;
    overrideInput.kind = xr::InputKind::Override;
    overrideInput.bootGeneration = 1;
    overrideInput.sequence = 1;
    output.defaultAdmissionRejected = rejected(overrideInput);
    for (std::size_t i = 0; i < program.subjects.count; ++i) {
        const auto &subject = program.subjects.data[i];
        const auto &b = subject.binding;
        if (!b.epochsComplete || b.epochs.count || hex(b.station) == hex(program.context.station))
            continue;
        for (unsigned k = 0; k < 4; ++k) {
            xr::Input input;
            input.context = program.context;
            input.kind = xr::InputKind::Fact;
            input.bootGeneration = 1;
            input.sequence = 1;
            input.fact.subject = static_cast<larp::configuration::module_ir::Id>(i + 1);
            input.fact.kind = xr::FactClass::CurrentSolved;
            auto &ref = input.fact.ref;
            ref.id.event = program.context.event;
            ref.id.origin = b.box;
            ref.id.sequence = 1;
            ref.id.epoch = program.context.sourceEpoch;
            if (k < 2) {
                ref.id.epoch = {};
                ref.id.epoch.bytes[14] = 0xd0;
                ref.id.epoch.bytes[15] = static_cast<std::uint8_t>(k);
            } else if (k == 3) {
                for (auto &byte : ref.id.epoch.bytes)
                    byte = 0xff;
            }
            for (auto &byte : ref.digest.bytes)
                byte = 0x91;
            ref.deployment = program.context.deployment;
            ref.binding = b.binding;
            ref.run = program.context.run;
            ref.station = b.station;
            ref.subject = b.subject;
            ref.type = xr::FactType::TaskSolved;
            ++output.emptyEpochAttempts;
            if (rejected(input))
                ++output.emptyEpochRejects;
        }
    }
    return output;
}
