// Explicit typed-consumer seam: byte JSON expansion does not preserve DAG sharing.
// Successful source/hash byte-chain evidence remains in cli.cpp, not this seam.
#define main unused_reference_driver_main
#include "cli.cpp"
#undef main
#include <cstdio>
int main() {
    for (unsigned limit : {4096u, 4097u}) {
        Trace trace;
        auto &source = trace.compiler.workspace;
        auto &expanded = trace.compiler.expanded;
        auto intern = [&](const char *text) -> Id {
            const Id id = static_cast<Id>(source.stringOffsets.count + 1);
            source.stringOffsets.data[source.stringOffsets.count++] =
                static_cast<std::uint32_t>(source.strings.count);
            const auto n = std::strlen(text) + 1;
            std::memcpy(source.strings.data + source.strings.count, text, n);
            source.strings.count += n;
            return id;
        };
        const Id station = intern("s"), port = intern("fire");
        source.definitions.count = 1;
        source.definitions.data[0].id = station;
        source.symbols.count = 1;
        source.symbols.data[0].owner = 1;
        source.symbols.data[0].id = port;
        source.symbols.data[0].kind = SymbolKind::Port;
        source.symbols.data[0].type = Type::Integer;
        source.symbols.data[0].input = true;
        expanded.nodes.count = 512;
        for (std::size_t i = 0; i < 512; ++i)
            expanded.nodes.data[i] = {};
        expanded.nodes.data[0].op = Op::Literal;
        expanded.nodes.data[0].type = Type::Boolean;
        expanded.nodes.data[0].value = 1;
        for (unsigned i = 1; i <= 447; ++i) {
            auto &node = expanded.nodes.data[i];
            node.op = Op::All;
            node.count = 8;
            for (auto &child : node.children)
                child = 1;
        }
        std::vector<std::string> names(64);
        std::vector<xr::RuleBinding> bindings(64);
        unsigned cursor = 0;
        const unsigned extras = limit == 4096 ? 8 : 9;
        expanded.rules.count = 64;
        expanded.triggers.count = 64;
        for (unsigned i = 0; i < 64; ++i) {
            char label[8];
            std::snprintf(label, sizeof(label), "r%02u", i);
            const Id id = intern(label);
            names[i] = "/" + std::string(label);
            bindings[i].qualified = names[i].c_str();
            bindings[i].ruleId = identity(i + 1);
            auto &node = expanded.nodes.data[448 + i];
            node.op = Op::All;
            node.count = static_cast<std::uint8_t>(i < extras ? 8 : 7);
            for (unsigned j = 0; j < node.count; ++j)
                node.children[j] = static_cast<Id>(2 + (cursor++ % 447));
            auto &rule = expanded.rules.data[i];
            rule = {};
            rule.name.count = 1;
            rule.name.segments[0] = id;
            rule.rule.owner = 1;
            rule.rule.id = id;
            rule.rule.guard = static_cast<Id>(449 + i);
            rule.rule.generation = 1;
            rule.rule.once = true;
            rule.rule.trigger = 1;
            rule.rule.policy = 2;
            rule.rule.firstTrigger = static_cast<Id>(i);
            rule.rule.triggerCount = 1;
            expanded.triggers.data[i] = port;
        }
        xr::Context context;
        context.event = identity(201);
        context.run = identity(202);
        context.station = identity(203);
        context.box = identity(204);
        context.sourceEpoch = identity(205);
        context.deployment = fingerprint(17);
        context.binding = fingerprint(34);
        context.content = fingerprint(77);
        xr::Span<xr::RuleBinding> rb;
        rb.data = bindings.data();
        rb.count = bindings.size();
        xr::Span<xr::SubjectBinding> sb;
        xr::Span<xr::PortBinding> pb;
        xr::ProgramBuilder builder(trace.memory.program, "s", context, rb, sb, pb);
        if (!builder.begin(context.content.bytes) || !builder.station(source, expanded, 1)) {
            std::cerr << "Typed DAG begin/station failed " << int(builder.result().error) << "\n";
            return 1;
        }
        const bool prepared = builder.prepare();
        if (limit == 4097) {
            if (prepared || builder.result().error != xr::Error::OperatingProfile ||
                trace.memory.program.ready) {
                std::cerr << "4097 static upper accepted or misclassified\n";
                return 2;
            }
            builder.abort();
            continue;
        }
        if (!prepared) {
            std::cerr << "4096 static upper rejected " << int(builder.result().error) << "\n";
            return 3;
        }
        builder.commit();
        xr::Fence fence;
        fence.progress = fence.profile = fence.trust = fence.facts = fence.ledger =
            fence.resources = fence.rights = fence.capacity = 1;
        auto result = xr::Planner::initialize(trace.memory.program, fence, trace.states[0].pools,
                                              trace.state);
        if (result.error != xr::Error::None)
            return 4;
        result = xr::Planner::control(trace.state, xr::Mode::Running);
        if (result.error != xr::Error::None)
            return 5;
        trace.capacity.receipts = 1024;
        trace.capacity.facts = 256;
        trace.capacity.latches = 64;
        trace.capacity.outbox = 64;
        trace.capacity.intents = 64;
        xr::Input input;
        input.context = context;
        input.kind = xr::InputKind::Local;
        input.bootGeneration = 1;
        input.sequence = 1;
        input.port = 1;
        input.type = Type::Integer;
        input.value = 1;
        result = trace.apply(input);
        if (result.decision != xr::Decision::Ready || trace.state.latches.count != 64 ||
            trace.state.ledger.count != 1) {
            std::cerr << "4096 runtime not applied " << int(result.decision) << ' '
                      << int(result.error) << "\n";
            return 6;
        }
    }
    std::cout << "Typed DAG visit bound4096 accepted/applied;4097 rejected prepublication as "
                 "OperatingProfile\n";
    return 0;
}
