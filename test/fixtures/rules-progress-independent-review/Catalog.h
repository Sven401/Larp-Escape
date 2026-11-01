#pragma once
#include "GameExecution/RulesProgressCodec.h"
#include <cstdlib>
namespace rp = larp::execution::rules_progress;
std::size_t codecChecks = 0;
void requireCodec(bool good, const char *message) {
    ++codecChecks;
    if (!good) {
        std::cerr << "CODECFAIL " << message << '\n';
        std::exit(7);
    }
}
std::string byteHex(const std::uint8_t *p, std::size_t n) {
    const char *d = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < n; ++i) {
        s += d[p[i] >> 4];
        s += d[p[i] & 15];
    }
    return s;
}
std::string N(std::uint64_t v) {
    return std::to_string(v);
}
std::string I(std::int32_t v) {
    return std::to_string(v);
}
std::string B(bool b) {
    return b ? "true" : "false";
}
std::string Y(const void *ptr, std::size_t n) {
    return "{\"b\":\"" + byteHex(static_cast<const std::uint8_t *>(ptr), n) + "\"}";
}
std::string A(std::initializer_list<std::string> values) {
    std::string s = "[";
    bool first = true;
    for (const auto &v : values) {
        if (!first)
            s += ',';
        first = false;
        s += v;
    }
    return s + "]";
}
template <class T, class F> std::string P(const Pool<T> &v, F convert) {
    std::string s = "[";
    for (std::size_t i = 0; i < v.count; ++i) {
        if (i)
            s += ',';
        s += convert(v.data[i]);
    }
    return s + "]";
}
std::string modelContext(const xr::Context &c) {
    return A({Y(c.event.bytes, 16), Y(c.run.bytes, 16), Y(c.station.bytes, 16), Y(c.box.bytes, 16),
              Y(c.sourceEpoch.bytes, 16), Y(c.deployment.bytes, 32), Y(c.binding.bytes, 32),
              Y(c.content.bytes, 32), N(c.bindingGeneration)});
}
std::string modelLocation(const Location &v) {
    return A({N(v.offset), N(v.source), N(v.station), N(v.index), N(v.subindex), N(v.role)});
}
std::string modelNode(const Node &v) {
    std::string children = "[";
    for (unsigned i = 0; i < 8; ++i) {
        if (i)
            children += ',';
        children += N(v.children[i]);
    }
    children += ']';
    return A({modelLocation(v.location), N(static_cast<unsigned>(v.op)),
              N(static_cast<unsigned>(v.type)), I(v.value), N(v.ref), children, N(v.count)});
}
std::string modelAction(const larp::configuration::module_ir::Action &v) {
    return A({modelLocation(v.location), N(static_cast<unsigned>(v.op)), N(v.target), N(v.resource),
              N(v.expr)});
}
std::string modelRule(const xr::Rule &v) {
    const auto &r = v.ir;
    return A({A({modelLocation(r.location), N(r.owner), N(r.id), N(r.guard), N(r.firstAction),
                 N(r.firstTrigger), N(r.timer), N(r.fallback), N(r.group), N(r.generation),
                 N(r.priority), N(r.actionCount), N(r.triggerCount), N(r.trigger), N(r.policy),
                 B(r.once)}),
              N(v.name), Y(v.stable.bytes, 16)});
}
std::string modelSymbol(const xr::Symbol &v) {
    return A({N(v.name), N(static_cast<unsigned>(v.type)), I(v.initial)});
}
std::string modelPort(const xr::Port &v) {
    return A({N(v.name), N(static_cast<unsigned>(v.type)), B(v.input), Y(v.exclusive.bytes, 16),
              N(v.firstResource), N(v.resourceCount)});
}
std::string modelSubject(const xr::Subject &v, const xr::Program &p) {
    const auto &b = v.binding;
    std::size_t start = 0;
    if (b.epochs.count)
        start = (reinterpret_cast<std::uintptr_t>(b.epochs.data) -
                 reinterpret_cast<std::uintptr_t>(p.epochs.data)) /
                sizeof(xr::Identity);
    return A({N(v.name), Y(b.subject.bytes, 16), Y(b.station.bytes, 16), Y(b.box.bytes, 16),
              Y(b.epoch.bytes, 16), Y(b.binding.bytes, 32), A({N(start), N(b.epochs.count)}),
              N(b.factTypes), B(b.epochsComplete)});
}
std::string catalogModel(const xr::Program &p) {
    std::vector<std::string> rows = {
        "\"larp.rules.catalog.v1\"",
        modelContext(p.context),
        A({N(p.language), B(p.manualOverride)}),
        Y(p.strings.data, p.strings.count),
        P(p.offsets, [](std::uint32_t v) { return N(v); }),
        P(p.nodes, modelNode),
        P(p.actions, modelAction),
        P(p.rules, modelRule),
        P(p.values, modelSymbol),
        P(p.timers, modelSymbol),
        P(p.ports, modelPort),
        P(p.epochs, [](const xr::Identity &v) { return Y(v.bytes, 16); }),
        P(p.subjects, [&p](const xr::Subject &v) { return modelSubject(v, p); }),
        P(p.resources,
          [](const xr::Resource &v) { return A({N(v.name), N(static_cast<unsigned>(v.type))}); }),
        P(p.allowedResources, [](xr::Id v) { return N(v); }),
        P(p.triggers, [](xr::Id v) { return N(v); })};
    std::string s = "{";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i)
            s += ',';
        s += '"' + N(i) + '"' + ':' + rows[i];
    }
    return s + '}';
}
xr::Span<std::uint8_t> view(const std::vector<std::uint8_t> &v, std::size_t n) {
    xr::Span<std::uint8_t> s;
    s.data = v.data();
    s.count = n;
    return s;
}
#include "Capacity.h"
void codecCatalog(xr::Program &p) {
    if (!p.ready) {
        std::cout << "{\"skipped\":true}\n";
        return;
    }
    const auto typed = catalogModel(p);
    std::vector<std::uint8_t> bytes(32768), again(32768);
    Pool<std::uint8_t> out;
    out.data = bytes.data();
    out.capacity = bytes.size();
    requireCodec(rp::encodeCatalog(p, out).scope == rp::Scope::EncodedUnpublished,
                 "actual compiled catalog encode");
    const auto size = out.count;
    const auto frozen = bytes;
    catalogCapacity(frozen, size, p);
    ProgramMemory decoded;
    requireCodec(rp::decodeCatalog(view(bytes, size), decoded.program).scope ==
                         rp::Scope::DecodedUnauthenticated &&
                     decoded.program.ready,
                 "actual compiled catalog decode");
    // Destroy both caller-encoded bytes and the original compiler-derived pool data.
    std::fill(bytes.begin(), bytes.end(), 0xa5);
    std::fill(p.strings.data, p.strings.data + p.strings.count, static_cast<char>(0x7f));
    for (std::size_t i = 0; i < p.epochs.count; ++i)
        p.epochs.data[i] = xr::Identity{};
    for (std::size_t i = 0; i < p.nodes.count; ++i)
        p.nodes.data[i] = Node{};
    p.ready = false;
    requireCodec(catalogModel(decoded.program) == typed,
                 "all reconstructed data/epoch spans are owned");
    Pool<std::uint8_t> re;
    re.data = again.data();
    re.capacity = again.size();
    requireCodec(rp::encodeCatalog(decoded.program, re).scope == rp::Scope::EncodedUnpublished &&
                     re.count == size &&
                     std::equal(again.begin(), again.begin() + size, frozen.begin()),
                 "decoded catalog exact byte roundtrip after source destruction");
    const auto capacity = re.capacity;
    re.capacity = size - 1;
    requireCodec(rp::encodeCatalog(decoded.program, re).scope == rp::Scope::Rejected &&
                     re.count == 0,
                 "actual catalog capacity -1");
    re.capacity = size;
    requireCodec(rp::encodeCatalog(decoded.program, re).scope == rp::Scope::EncodedUnpublished &&
                     re.count == size,
                 "actual catalog exact capacity");
    re.capacity = capacity;
    std::cout << "{\"catalog\":\"" << byteHex(frozen.data(), size) << "\",\"model\":" << typed
              << ",\"checks\":" << codecChecks << "}\n";
}
int decodeCatalogOnly() {
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto encoded = unhex(line);
        std::vector<std::uint8_t> bytes(encoded.begin(), encoded.end());
        ProgramMemory inactive;
        inactive.program.ready = true;
        const auto result = rp::decodeCatalog(view(bytes, bytes.size()), inactive.program);
        std::cout << static_cast<unsigned>(result.scope) << ' '
                  << static_cast<unsigned>(result.error) << ' ' << inactive.program.ready << ' '
                  << inactive.program.rules.count << ' ' << inactive.program.nodes.count << ' '
                  << inactive.program.subjects.count << '\n';
    }
    return 0;
}

void captureTraceCatalog(const xr::Program &p) {
    std::vector<std::uint8_t> bytes(32768), again(32768);
    Pool<std::uint8_t> output;
    output.data = bytes.data();
    output.capacity = bytes.size();
    const auto typed = catalogModel(p);
    requireCodec(rp::encodeCatalog(p, output).scope == rp::Scope::EncodedUnpublished,
                 "actual trace catalog encode");
    const auto n = output.count;
    const auto raw = bytes;
    catalogCapacity(raw, n, p);
    ProgramMemory decoded;
    requireCodec(rp::decodeCatalog(view(bytes, n), decoded.program).scope ==
                     rp::Scope::DecodedUnauthenticated,
                 "actual trace catalog decode");
    std::fill(bytes.begin(), bytes.end(), 0xa5);
    requireCodec(catalogModel(decoded.program) == typed,
                 "actual trace values/timers/epochs owned after source loss");
    Pool<std::uint8_t> back;
    back.data = again.data();
    back.capacity = again.size();
    requireCodec(rp::encodeCatalog(decoded.program, back).scope == rp::Scope::EncodedUnpublished &&
                     back.count == n && std::equal(again.begin(), again.begin() + n, raw.begin()),
                 "actual trace catalog exact reencode");
    std::cout << "{\"codecCatalog\":\"" << byteHex(raw.data(), n) << "\",\"catalogModel\":" << typed
              << "}\n";
}
