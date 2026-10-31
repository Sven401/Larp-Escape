#pragma once
// Caller-owned actual Program/State/Plan buffers, preserved from the planner audit.
struct ProgramMemory {
    xr::Program program;
    std::vector<xr::Identity> epochs;
    std::vector<Id> allowedResources;
    std::vector<char> strings;
    std::vector<std::uint32_t> offsets;
    std::vector<Node> nodes;
    std::vector<Action> actions;
    std::vector<xr::Rule> rules;
    std::vector<xr::Symbol> values, timers;
    std::vector<xr::Port> ports;
    std::vector<xr::Subject> subjects;
    std::vector<xr::Resource> resources;
    std::vector<Id> triggers;
    ProgramMemory() {
        storage(program.epochs, epochs, 256);
        storage(program.allowedResources, allowedResources, 4096);
        storage(program.strings, strings, 65534);
        storage(program.offsets, offsets, 4096);
        storage(program.nodes, nodes, 512);
        storage(program.actions, actions, 512);
        storage(program.rules, rules, 64);
        storage(program.values, values, 32);
        storage(program.timers, timers, 16);
        storage(program.ports, ports, 128);
        storage(program.subjects, subjects, 128);
        storage(program.resources, resources, 32);
        storage(program.triggers, triggers, 4096);
    }
};
struct StateMemory {
    xr::StateStorage pools;
    std::vector<Id> solved;
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
    std::vector<Id> proofRefs;
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
