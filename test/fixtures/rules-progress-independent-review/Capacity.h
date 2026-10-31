#pragma once
bool catalogCleared(const xr::Program &p) {
    return !p.ready && !p.strings.count && !p.offsets.count && !p.nodes.count && !p.actions.count &&
           !p.rules.count && !p.values.count && !p.timers.count && !p.ports.count &&
           !p.epochs.count && !p.subjects.count && !p.resources.count &&
           !p.allowedResources.count && !p.triggers.count;
}
void catalogCapacity(const std::vector<std::uint8_t> &source, std::size_t n,
                     const xr::Program &original) {
#define CAP(field)                                                                                 \
    if (original.field.count) {                                                                    \
        ProgramMemory m;                                                                           \
        const auto full = m.program.field.capacity;                                                \
        m.program.field.capacity = original.field.count - 1;                                       \
        requireCodec(rp::decodeCatalog(view(source, n), m.program).scope == rp::Scope::Rejected && \
                         catalogCleared(m.program),                                                \
                     "catalog pool capacity -1 clears complete inactive result");                  \
        m.program.field.capacity = full;                                                           \
        requireCodec(rp::decodeCatalog(view(source, n), m.program).scope ==                        \
                             rp::Scope::DecodedUnauthenticated &&                                  \
                         m.program.ready,                                                          \
                     "catalog pool capacity failure healthy reuse");                               \
    }
    CAP(strings);
    CAP(offsets);
    CAP(nodes);
    CAP(actions);
    CAP(rules);
    CAP(values);
    CAP(timers);
    CAP(ports);
    CAP(epochs);
    CAP(subjects);
    CAP(resources);
    CAP(allowedResources);
    CAP(triggers);
#undef CAP
    ProgramMemory alias;
    alias.program.ready = true;
    alias.program.strings.data =
        reinterpret_cast<char *>(const_cast<std::uint8_t *>(source.data()));
    alias.program.strings.capacity = source.size();
    alias.program.strings.count = 13;
    auto r = rp::decodeCatalog(view(source, n), alias.program);
    requireCodec(r.scope == rp::Scope::Rejected && r.error == rp::Error::Alias &&
                     alias.program.ready && alias.program.strings.count == 13,
                 "catalog alias preflight preserves complete old metadata");
    ProgramMemory reuse;
    requireCodec(rp::decodeCatalog(view(source, n), reuse.program).scope ==
                     rp::Scope::DecodedUnauthenticated,
                 "catalog reuse good seed");
    auto bad = source;
    bad.resize(n);
    bad.push_back(0);
    requireCodec(rp::decodeCatalog(view(bad, n + 1), reuse.program).scope == rp::Scope::Rejected &&
                     catalogCleared(reuse.program),
                 "catalog malformed complete input clears prior decoded pools");
    requireCodec(rp::decodeCatalog(view(source, n), reuse.program).scope ==
                     rp::Scope::DecodedUnauthenticated,
                 "catalog malformed healthy reuse");
    Pool<std::uint8_t> destination;
    destination.data = reinterpret_cast<std::uint8_t *>(reuse.program.strings.data);
    destination.capacity = reuse.program.strings.capacity;
    destination.count = 17;
    r = rp::encodeCatalog(reuse.program, destination);
    requireCodec(r.scope == rp::Scope::Rejected && r.error == rp::Error::Alias &&
                     destination.count == 17 && reuse.program.ready,
                 "catalog encode alias preserves output count and source flags");
}
