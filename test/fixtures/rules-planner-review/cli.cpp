#include "CompilerSupport.h"
#include "GameExecution/RulesPlanner.h"
#include <map>
namespace xr = larp::execution::rules;
struct CompilerMemory {
    PackageModuleCompiler compiler;
    ModuleCompileWorkspace workspace;
    ExpandedStationWorkspace expanded;
    std::vector<char> strings;
    std::vector<std::uint32_t> offsets;
    std::vector<Node> nodes, enodes;
    std::vector<Symbol> symbols;
    std::vector<Action> actions, eactions;
    std::vector<Rule> rules;
    std::vector<Definition> definitions;
    std::vector<Instance> instances;
    std::vector<Binding> bindings;
    std::vector<Constraint> constraints;
    std::vector<Id> triggers, etriggers;
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
xr::Identity identity(unsigned n) {
    xr::Identity id;
    id.bytes[14] = static_cast<std::uint8_t>(n >> 8);
    id.bytes[15] = static_cast<std::uint8_t>(n);
    return id;
}
xr::Digest fingerprint(unsigned n) {
    xr::Digest d;
    std::memset(d.bytes, n, 32);
    return d;
}
std::string shortName(const char *name) {
    std::string s = name;
    return s.compare(0, 6, "/inst/") == 0 ? s.substr(6) : s;
}
void quote(const std::string &s) {
    std::cout << '"';
    for (char c : s) {
        if (c == '"' || c == '\\')
            std::cout << '\\';
        std::cout << c;
    }
    std::cout << '"';
}
Id findPort(const xr::Program &p, const std::string &name) {
    for (std::size_t i = 0; i < p.ports.count; ++i)
        if (name == xr::name(p, p.ports.data[i].name))
            return static_cast<Id>(i + 1);
    return 0;
}
Id findSubject(const xr::Program &p, const std::string &name) {
    for (std::size_t i = 0; i < p.subjects.count; ++i)
        if (name == xr::name(p, p.subjects.data[i].name))
            return static_cast<Id>(i + 1);
    return 0;
}
struct Trace {
    ProgramMemory memory;
    CompilerMemory compiler;
    StateMemory states[2];
    PlannerMemory runtime;
    xr::State state;
    unsigned active = 0;
    xr::Capacities capacity;
    std::vector<std::string> emissions, intents;
    std::vector<std::vector<std::string>> dependencyHistory, intentHistory;
    xr::Result lastResult;
    int commitFault = 0;
    std::vector<std::string> evidenceNames(xr::Evidence evidence) const {
        std::vector<std::string> names;
        for (std::size_t i = 0; i < evidence.refs.count; ++i)
            for (std::size_t j = 0; j < memory.program.subjects.count; ++j)
                if (!std::memcmp(evidence.refs.data[i].subject.bytes,
                                 memory.program.subjects.data[j].binding.subject.bytes, 16)) {
                    names.push_back(xr::name(memory.program, memory.program.subjects.data[j].name));
                    break;
                }
        return names;
    }
    void histories(const std::vector<std::vector<std::string>> &history) {
        std::cout << '[';
        for (std::size_t i = 0; i < history.size(); ++i) {
            if (i)
                std::cout << ',';
            std::cout << '[';
            for (std::size_t j = 0; j < history[i].size(); ++j) {
                if (j)
                    std::cout << ',';
                quote(history[i][j]);
            }
            std::cout << ']';
        }
        std::cout << ']';
    }
    std::map<std::pair<bool, std::uint64_t>, std::string> operationNames;
    xr::Result apply(const xr::Input &input) {
        xr::Result r = runtime.planner.plan(memory.program, state, input, capacity, runtime.plan,
                                            runtime.workspace);
        lastResult = r;
        if (r.decision != xr::Decision::Ready)
            return r;
        xr::State next;
        xr::State presented = state;
        switch (commitFault) {
        case 1:
            ++presented.fence.progress;
            break;
        case 2:
            ++presented.fence.profile;
            break;
        case 3:
            ++presented.fence.trust;
            break;
        case 4:
            ++presented.fence.facts;
            break;
        case 5:
            ++presented.fence.ledger;
            break;
        case 6:
            ++presented.fence.resources;
            break;
        case 7:
            ++presented.fence.rights;
            break;
        case 8:
            ++presented.fence.capacity;
            break;
        case 9:
            ++presented.fence.lastIssued;
            break;
        case 10:
            ++presented.fence.activeTime;
            break;
        case 11:
            presented.mode = xr::Mode::Paused;
            break;
        case 12:
            presented.context.run = identity(250);
            break;
        }
        auto &staging = states[commitFault == 200 ? active : 1 - active].pools;
        const auto oldCapacity = staging.evidence.capacity;
        if (commitFault == 100)
            staging.evidence.capacity = 0;
        r = xr::Planner::commitRam(memory.program, presented, runtime.plan, staging, next);
        staging.evidence.capacity = oldCapacity;
        if (r.decision == xr::Decision::Ready) {
            state = next;
            for (std::size_t i = 0; i < runtime.plan.emissions.count; ++i)
                dependencyHistory.push_back(
                    evidenceNames(runtime.plan.emissions.data[i].dependencies));
            for (std::size_t i = 0; i < runtime.plan.intents.count; ++i)
                intentHistory.push_back(evidenceNames(runtime.plan.intents.data[i].evidence));
            active = 1 - active;
            for (std::size_t i = 0; i < runtime.plan.emissions.count; ++i)
                emissions.push_back(xr::name(
                    memory.program,
                    memory.program.subjects.data[runtime.plan.emissions.data[i].subject - 1].name));
            for (std::size_t i = 0; i < runtime.plan.intents.count; ++i)
                intents.push_back(xr::name(
                    memory.program,
                    memory.program.ports.data[runtime.plan.intents.data[i].port - 1].name));
        }
        lastResult = r;
        return r;
    }
    bool clock(std::uint64_t time) {
        auto r = xr::Planner::advanceClock(state, time);
        if (r.error != xr::Error::None)
            return false;
        xr::Due data[16];
        xr::Pool<xr::Due> due;
        due.data = data;
        due.capacity = 16;
        r = xr::Planner::due(memory.program, state, due);
        if (r.error != xr::Error::None)
            return false;
        for (std::size_t i = 0; i < due.count; ++i) {
            xr::Input input;
            input.context = memory.program.context;
            input.kind = xr::InputKind::Timer;
            input.timer = due.data[i].timer;
            input.armGeneration = due.data[i].generation;
            r = apply(input);
            if (r.decision != xr::Decision::Ready && r.decision != xr::Decision::StaleTimer)
                return false;
        }
        return true;
    }
    void dump(const std::string &status) {
        const auto &p = memory.program;
        std::cout << "{\"status\":";
        quote(status);
        std::cout << ",\"values\":{";
        for (std::size_t i = 0; i < state.values.count; ++i) {
            if (i)
                std::cout << ',';
            quote(shortName(xr::name(p, p.values.data[i].name)));
            std::cout << ':' << state.values.data[i].value;
        }
        std::cout << "},\"timers\":{";
        bool comma = false;
        for (std::size_t i = 0; i < state.timers.count; ++i)
            if (state.timers.data[i].active) {
                if (comma)
                    std::cout << ',';
                comma = true;
                quote(shortName(xr::name(p, p.timers.data[i].name)));
                std::cout << ":[" << state.timers.data[i].deadline << ','
                          << state.timers.data[i].generation << ']';
            }
        std::cout << "},\"timer_generations\":{";
        comma = false;
        for (std::size_t i = 0; i < state.timers.count; ++i)
            if (state.timers.data[i].generation) {
                if (comma)
                    std::cout << ',';
                comma = true;
                quote(shortName(xr::name(p, p.timers.data[i].name)));
                std::cout << ':' << state.timers.data[i].generation;
            }
        std::cout << "},\"facts\":[";
        for (std::size_t i = 0; i < state.facts.count; ++i) {
            if (i)
                std::cout << ',';
            quote(xr::name(p, p.subjects.data[state.facts.data[i].subject - 1].name));
        }
        std::cout << "],\"latches\":[";
        for (std::size_t i = 0; i < state.latches.count; ++i) {
            if (i)
                std::cout << ',';
            for (std::size_t j = 0; j < p.rules.count; ++j)
                if (!std::memcmp(state.latches.data[i].rule.bytes, p.rules.data[j].stable.bytes,
                                 16)) {
                    quote(shortName(xr::name(p, p.rules.data[j].name)));
                    break;
                }
        }
        std::cout << "],\"operations\":[";
        for (std::size_t i = 0; i < state.ledger.count; ++i) {
            if (i)
                std::cout << ',';
            const auto &input = state.ledger.data[i].input;
            if (input.kind == xr::InputKind::Timer)
                quote("timer:" + shortName(xr::name(p, p.timers.data[input.timer - 1].name)) + ":" +
                      std::to_string(input.armGeneration));
            else
                quote(operationNames[std::make_pair(input.kind == xr::InputKind::Fact,
                                                    input.kind == xr::InputKind::Fact
                                                        ? input.fact.ref.id.sequence
                                                        : input.sequence)]);
        }
        std::cout << "],\"emissions\":[";
        for (std::size_t i = 0; i < emissions.size(); ++i) {
            if (i)
                std::cout << ',';
            quote(emissions[i]);
        }
        std::cout << "],\"intents\":[";
        for (std::size_t i = 0; i < intents.size(); ++i) {
            if (i)
                std::cout << ',';
            quote(intents[i]);
        }
        std::cout << "],\"now\":" << state.fence.activeTime
                  << ",\"audit\":{\"error\":" << int(lastResult.error) << ",\"dependencies\":";
        histories(dependencyHistory);
        std::cout << ",\"intentEvidence\":";
        histories(intentHistory);
        std::cout << ",\"latchEvidence\":";
        std::vector<std::vector<std::string>> latchHistory;
        for (std::size_t i = 0; i < state.latches.count; ++i)
            latchHistory.push_back(evidenceNames(state.latches.data[i].evidence));
        histories(latchHistory);
        std::cout << "}}\n";
    }
};
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        Trace trace;
        Bytes content;
        content.bytes = line;
        Provider provider;
        provider.records.resize(1);
        auto &source = provider.records[0];
        std::getline(std::cin, source.id);
        std::getline(std::cin, source.version);
        std::getline(std::cin, source.digest);
        std::getline(std::cin, source.source.bytes);
        xr::Context context;
        context.event = identity(201);
        context.run = identity(202);
        context.station = identity(203);
        context.box = identity(204);
        context.sourceEpoch = identity(205);
        context.deployment = fingerprint(17);
        context.binding = fingerprint(34);
        Hash hash;
        std::size_t written = 0;
        if (!hash.begin() ||
            !hash.update(reinterpret_cast<const std::uint8_t *>(content.bytes.data()),
                         content.bytes.size()) ||
            !hash.finish(context.content.bytes, written))
            return 1;
        std::getline(std::cin, line);
        std::istringstream settings(line);
        std::size_t proofCapacity = 8192;
        std::size_t receiptCapacity = 1024;
        int expectBuilderRejected = 0;
        settings >> proofCapacity >> receiptCapacity >> expectBuilderRejected;
        trace.capacity.receipts = receiptCapacity;
        trace.runtime.workspace.proofs.capacity = proofCapacity;
        std::getline(std::cin, line);
        unsigned ruleCount = static_cast<unsigned>(std::stoul(line));
        std::vector<std::string> ruleNames(ruleCount);
        std::vector<xr::RuleBinding> ruleBindings(ruleCount);
        for (unsigned i = 0; i < ruleCount; ++i) {
            std::getline(std::cin, ruleNames[i]);
            ruleBindings[i].qualified = ruleNames[i].c_str();
            std::getline(std::cin, line);
            ruleBindings[i].ruleId = identity(static_cast<unsigned>(std::stoul(line)));
        }
        std::getline(std::cin, line);
        unsigned subjectCount = static_cast<unsigned>(std::stoul(line));
        std::vector<std::string> subjectNames(subjectCount);
        std::vector<xr::SubjectBinding> subjectBindings(subjectCount);
        for (unsigned i = 0; i < subjectCount; ++i) {
            std::getline(std::cin, subjectNames[i]);
            std::getline(std::cin, line);
            bool local = line == "1";
            auto &binding = subjectBindings[i];
            binding.name = subjectNames[i].c_str();
            binding.subject = identity(i + 20);
            binding.station = local ? context.station : identity(210);
            binding.box = local ? context.box : identity(211);
            binding.epoch = local ? context.sourceEpoch : identity(212);
            binding.binding = local ? context.binding : fingerprint(51);
        }
        xr::Span<xr::RuleBinding> rb;
        rb.data = ruleBindings.data();
        rb.count = ruleBindings.size();
        xr::Span<xr::SubjectBinding> sb;
        sb.data = subjectBindings.data();
        sb.count = subjectBindings.size();
        std::getline(std::cin, line);
        unsigned portCount = static_cast<unsigned>(std::stoul(line));
        std::vector<std::string> portNames(portCount);
        std::vector<xr::PortBinding> portBindings(portCount);
        std::vector<std::vector<std::string>> resourceNames(portCount);
        std::vector<std::vector<const char *>> resourcePointers(portCount);
        for (unsigned i = 0; i < portCount; ++i) {
            std::getline(std::cin, portNames[i]);
            std::getline(std::cin, line);
            std::istringstream fields(line);
            unsigned exclusive = 0, count = 0;
            fields >> exclusive >> count;
            resourceNames[i].resize(count);
            resourcePointers[i].resize(count);
            for (unsigned j = 0; j < count; ++j) {
                std::getline(std::cin, resourceNames[i][j]);
                resourcePointers[i][j] = resourceNames[i][j].c_str();
            }
            portBindings[i].name = portNames[i].c_str();
            portBindings[i].exclusive = identity(exclusive);
            portBindings[i].resources.data = resourcePointers[i].data();
            portBindings[i].resources.count = count;
        }
        xr::Span<xr::PortBinding> pb;
        pb.data = portBindings.data();
        pb.count = portBindings.size();
        xr::ProgramBuilder builder(trace.memory.program, "s", context, rb, sb, pb);
        Sink sink;
        auto compiled =
            trace.compiler.compiler.compile(content, provider, hash, sink, trace.compiler.workspace,
                                            trace.compiler.expanded, &builder);
        if (compiled.scope != ModuleScope::ExpandedUnauthenticated || !trace.memory.program.ready) {
            if (expectBuilderRejected && !trace.memory.program.ready && !sink.committed &&
                builder.result().error != xr::Error::None) {
                std::getline(std::cin, line);
                const unsigned count = static_cast<unsigned>(std::stoul(line));
                for (unsigned i = 0; i < count; ++i) {
                    std::getline(std::cin, line);
                    std::getline(std::cin, line);
                    std::getline(std::cin, line);
                    std::cout << "{\"status\":\"builder-rejected\",\"audit\":{\"error\":"
                              << int(builder.result().error) << "}}\n";
                }
                continue;
            }
            std::cerr << "Compiler/builder failure " << int(compiled.error) << ' ' << compiled.path
                      << " builder " << int(builder.result().error) << "\n";
            return 2;
        }
        xr::Fence fence;
        fence.progress = fence.profile = fence.trust = fence.facts = fence.ledger =
            fence.resources = fence.rights = fence.capacity = 1;
        auto result = xr::Planner::initialize(trace.memory.program, fence, trace.states[0].pools,
                                              trace.state);
        if (result.error != xr::Error::None)
            return 3;
        result = xr::Planner::control(trace.state, xr::Mode::Running);
        if (result.error != xr::Error::None)
            return 4;
        // Receipt capacity was explicitly supplied by the independent scenario.
        trace.capacity.facts = 256;
        trace.capacity.latches = 64;
        trace.capacity.outbox = 64;
        trace.capacity.intents = 64;
        std::getline(std::cin, line);
        unsigned steps = static_cast<unsigned>(std::stoul(line));
        for (unsigned i = 0; i < steps; ++i) {
            std::getline(std::cin, line);
            std::istringstream fields(line);
            std::uint64_t time = 0, sequence = 0;
            int kind = 0, known = 0, value = 0, admitted = 1;
            int fault = 0;
            fields >> time >> kind >> sequence >> known >> value >> admitted >> fault;
            trace.commitFault = fault;
            std::string target, operation;
            std::getline(std::cin, target);
            std::getline(std::cin, operation);
            if (!trace.clock(time)) {
                std::cerr << "Clock/due failure at" << time << "\n";
                return 5;
            }
            std::string status = "advanced";
            if (kind) {
                xr::Input input;
                input.context = context;
                input.bootGeneration = 1;
                input.sequence = sequence;
                input.type = Type::Integer;
                input.known = known != 0;
                input.value = value;
                trace.operationNames.emplace(std::make_pair(kind == 2, sequence), operation);
                if (kind == 1) {
                    input.kind = xr::InputKind::Local;
                    input.port = findPort(trace.memory.program, target);
                } else {
                    input.kind = xr::InputKind::Fact;
                    input.fact.kind = xr::FactClass::CurrentSolved;
                    input.fact.subject = findSubject(trace.memory.program, target);
                    const auto &binding =
                        trace.memory.program.subjects.data[input.fact.subject - 1].binding;
                    auto &ref = input.fact.ref;
                    ref.id.event = context.event;
                    ref.id.origin = binding.box;
                    ref.id.epoch = binding.epoch;
                    ref.id.sequence = sequence;
                    ref.digest = fingerprint(static_cast<unsigned>(sequence));
                    ref.deployment = context.deployment;
                    ref.binding = binding.binding;
                    ref.run = admitted ? context.run : identity(250);
                    ref.station = binding.station;
                    ref.subject = binding.subject;
                }
                result = trace.apply(input);
                status = result.decision == xr::Decision::Ready       ? "applied"
                         : result.decision == xr::Decision::Duplicate ? "duplicate"
                         : result.decision == xr::Decision::Rejected  ? "rejected"
                         : result.decision == xr::Decision::Deferred  ? "deferred"
                                                                      : "unexpected";
                if (status == "unexpected") {
                    std::cerr << "Unexpected decision " << int(result.decision) << " error "
                              << int(result.error) << "\n";
                    return 6;
                }
            }
            trace.dump(status);
        }
    }
    return 0;
}
