#pragma once
#if defined(PACKAGE_ARTIFACT_REVIEW_PSA) && !defined(PACKAGE_MODULE_REVIEW_PSA)
#define PACKAGE_MODULE_REVIEW_PSA
#endif
#include "../rules-planner-review/CompilerSupport.h"
#include "GameExecution/PackageArtifacts.h"
namespace xr = larp::execution::rules;
namespace xa = larp::execution::artifacts;
struct CompilerMemory {
    PackageModuleCompiler compiler;
    ModuleCompileWorkspace workspace;
    ExpandedStationWorkspace expanded;
    std::vector<char> strings;
    std::vector<std::uint32_t> offsets;
    std::vector<Node> nodes, enodes;
    std::vector<Symbol> symbols;
    std::vector<larp::configuration::module_ir::Action> actions, eactions;
    std::vector<Rule> rules;
    std::vector<Definition> definitions;
    std::vector<Instance> instances;
    std::vector<Binding> bindings;
    std::vector<Constraint> constraints;
    std::vector<larp::configuration::module_ir::Id> triggers, etriggers;
    std::vector<ExpandedRule> erules;
    std::vector<ExpandedSymbol> values, timers;
    CompilerMemory() {
        storage(workspace.strings, strings, 65534);
        storage(workspace.stringOffsets, offsets, 4096);
        storage(workspace.nodes, nodes, 8192);
        storage(workspace.symbols, symbols, 4096);
        storage(workspace.actions, actions, 4096);
        storage(workspace.rules, rules, 2048);
        storage(workspace.definitions, definitions, 32);
        storage(workspace.instances, instances, 1024);
        storage(workspace.bindings, bindings, 8192);
        storage(workspace.constraints, constraints, 1024);
        storage(workspace.triggers, triggers, 8192);
        storage(expanded.nodes, enodes, 512);
        storage(expanded.actions, eactions, 512);
        storage(expanded.rules, erules, 64);
        storage(expanded.values, values, 32);
        storage(expanded.timers, timers, 16);
        storage(expanded.triggers, etriggers, 4096);
    }
};
struct ProgramMemory {
    xr::Program program;
    std::vector<xr::Identity> epochs;
    std::vector<larp::configuration::module_ir::Id> allowedResources;
    std::vector<char> strings;
    std::vector<std::uint32_t> offsets;
    std::vector<Node> nodes;
    std::vector<larp::configuration::module_ir::Action> actions;
    std::vector<xr::Rule> rules;
    std::vector<xr::Symbol> values, timers;
    std::vector<xr::Port> ports;
    std::vector<xr::Subject> subjects;
    std::vector<xr::Resource> resources;
    std::vector<larp::configuration::module_ir::Id> triggers;
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
