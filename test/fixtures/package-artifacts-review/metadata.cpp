#if defined(PACKAGE_ARTIFACT_REVIEW_PSA) && !defined(PACKAGE_MODULE_REVIEW_PSA)
#define PACKAGE_MODULE_REVIEW_PSA
#endif
#include "../rules-planner-review/CompilerSupport.h"
#include "GameConfiguration/PackageMetadata.h"
struct MetadataMemory {
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
    MetadataMemory() {
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
int main() {
    MetadataMemory memory;
    PackageSourceMetadata counter;
    std::string line;
    while (std::getline(std::cin, line)) {
        unsigned count = 0;
        long long contentFailure = -1, moduleFailure = -1;
        std::istringstream header(line);
        header >> count >> contentFailure >> moduleFailure;
        Provider provider;
        provider.records.resize(count);
        for (auto &r : provider.records) {
            std::getline(std::cin, r.id);
            std::getline(std::cin, r.version);
            std::getline(std::cin, r.digest);
            std::getline(std::cin, r.source.bytes);
        }
        Bytes content;
        std::getline(std::cin, content.bytes);
        if (contentFailure >= 0)
            content.failure = static_cast<std::size_t>(contentFailure);
        if (moduleFailure >= 0 && !provider.records.empty())
            provider.records[0].source.failure = static_cast<std::size_t>(moduleFailure);
        Hash hash;
        Sink sink;
        counter.reset();
        const auto result = memory.compiler.compile(content, provider, hash, sink, memory.workspace,
                                                    memory.expanded, nullptr, &counter);
        for (const auto &r : provider.records)
            if (r.opened != r.closed) {
                std::cerr << "Provider source leak\n";
                return 2;
            }
        std::cout << (result.scope == ModuleScope::ExpandedUnauthenticated ? 'A' : 'R') << ' '
                  << errorName(result.error) << ' ' << int(result.canonical.error) << ' '
                  << counter.metadataBytes() << ' ' << counter.rawBytes() << ' '
                  << counter.exceeded() << '\n';
    }
}
