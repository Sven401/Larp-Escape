#include "CompanionIntegration/ManualOverrideAuthority.h"
#include <cstring>
#include <limits>
namespace larp {
namespace integration {
namespace {
using namespace execution::rules;
using devices::event_authority::Phase;
using EventSnapshot = devices::event_authority::Snapshot;
struct Operation {
    bool &busy;
    explicit Operation(bool &b) : busy(b) {
        busy = true;
    }
    ~Operation() {
        busy = false;
    }
};
bool equal(const Identity &a, const Identity &b) {
    return !std::memcmp(a.bytes, b.bytes, 16);
}
bool equal(const Identity &a, const companion::Identity &b) {
    return !std::memcmp(a.bytes, b.bytes.data(), 16);
}
bool equal(const Digest &a, const Digest &b) {
    return !std::memcmp(a.bytes, b.bytes, 32);
}
bool equal(const OverrideAuthority &a, const OverrideAuthority &b) {
    return equal(a.peer, b.peer) && a.principal == b.principal && a.roleEpoch == b.roleEpoch &&
           a.connectionToken == b.connectionToken && a.connectionHandle == b.connectionHandle &&
           a.registration == b.registration && a.lifecycle == b.lifecycle &&
           a.registryRevision == b.registryRevision && a.eventRevision == b.eventRevision;
}
bool equal(const EventSnapshot &a, const EventSnapshot &b) {
    return equal(a.device, b.device) && equal(a.event, b.event) && a.generation == b.generation &&
           a.registryGeneration == b.registryGeneration &&
           equal(a.registryDigest, b.registryDigest) &&
           equal(a.originalAuthorizationDigest, b.originalAuthorizationDigest) &&
           a.phase == b.phase;
}
bool contains(const ManualAuthorityWorkspace &w, const void *p, std::size_t count) {
    const auto at = reinterpret_cast<std::uintptr_t>(p),
               begin = reinterpret_cast<std::uintptr_t>(w.entries.data);
    const auto bytes = w.entries.capacity * sizeof(ManualAuthorityEntry);
    return count > std::numeric_limits<std::uintptr_t>::max() - at ||
           (at < begin + bytes && begin < at + count);
}
} // namespace
bool ManualOverrideAuthority::valid() const {
    if (!work_.connections.data || work_.connections.count > work_.connections.capacity ||
        work_.connections.capacity > 65534 ||
        reinterpret_cast<std::uintptr_t>(work_.connections.data) %
            alignof(companion::AcceptedPeerProof))
        return false;
    return work_.entries.data && work_.entries.count <= work_.entries.capacity &&
           work_.entries.capacity <= 65534 &&
           work_.entries.capacity <=
               std::numeric_limits<std::size_t>::max() / sizeof(ManualAuthorityEntry) &&
           work_.entries.capacity * sizeof(ManualAuthorityEntry) <=
               std::numeric_limits<std::uintptr_t>::max() -
                   reinterpret_cast<std::uintptr_t>(work_.entries.data) &&
           reinterpret_cast<std::uintptr_t>(work_.entries.data) % alignof(ManualAuthorityEntry) ==
               0;
}
bool ManualOverrideAuthority::rememberConnection(companion::PeerProofOwner &owner,
                                                 const companion::PeerProofBinding &requested) {
    const auto original = requested;
    if (operating_)
        return false;
    Operation operation(operating_);
    if (!valid())
        return false;
    companion::AcceptedPeerProof evidence;
    if (!owner.readVerifiedEvidence(original, evidence) || !proof_.originalLive(original) ||
        !proof_.credentialCurrent(evidence.verified.credential))
        return false;
    for (std::size_t i = 0; i < work_.connections.count; ++i) {
        const auto &old = work_.connections.data[i];
        if (!(old.verified.original == original))
            continue;
        return old.transcript == evidence.transcript && old.signature == evidence.signature &&
               old.verified.credential == evidence.verified.credential &&
               old.verified.sequence == evidence.verified.sequence &&
               old.verified.issuedAt == evidence.verified.issuedAt &&
               old.verified.verifiedAt == evidence.verified.verifiedAt;
    }
    if (work_.connections.count == work_.connections.capacity)
        return false;
    work_.connections.data[work_.connections.count++] = evidence;
    return true;
}
bool ManualOverrideAuthority::registryDigest(const companion::PeerRegistrySnapshot &snapshot,
                                             companion::PeerRegistryRecord &record,
                                             Digest &digest) {
    digest = {};
    if (!companion::encodePeerRegistry(snapshot, record))
        return false;
    bool ok = hash_.abort();
    if (ok)
        ok = hash_.begin();
    if (ok)
        ok = hash_.update(record.bytes.data(), record.bytes.size());
    std::size_t count = 0;
    if (ok)
        ok = hash_.finish(digest.bytes, count) && count == 32;
    const bool closed = hash_.abort();
    if (!ok || !closed) {
        digest = {};
        return false;
    }
    return true;
}
ManualAuthorityResult ManualOverrideAuthority::current(const ManualAuthorityEntry &entry) {
    const auto &original = entry.evidence.original;
    const auto &actor = entry.authority;
    if (!original.connection.valid() || !original.actor.valid() || !source_.current(original) ||
        !proof_.originalLive(original.connection) ||
        !proof_.credentialCurrent(entry.evidence.credential))
        return ManualAuthorityResult::Stale;
    companion::TrustedAuthority actual;
    if (!access_.route(original.connection.connection, actual) || !actual.gameMaster ||
        !actual.principal || !(actual == original.actor.authority) ||
        actual.principal != actor.principal || actual.epoch != actor.roleEpoch)
        return ManualAuthorityResult::Closed;
    companion::PeerRegistrySnapshot registry;
    if (!registry_.snapshot(registry) || !registry_.credentialCurrent(entry.evidence.credential))
        return ManualAuthorityResult::Unavailable;
    if (!equal(entry.context.box, registry.device) || registry.generation != actor.registryRevision)
        return ManualAuthorityResult::Stale;
    bool assigned = false;
    for (unsigned i = 0; i < registry.count; ++i) {
        const auto &peer = registry.peers[i];
        assigned |= peer.gameMaster && peer.principal == actor.principal &&
                    peer.credential == entry.evidence.credential;
    }
    if (!assigned)
        return ManualAuthorityResult::Closed;
    EventSnapshot event;
    if (!event_.snapshot(event))
        return ManualAuthorityResult::Unavailable;
    if (event.phase != Phase::Bound || !equal(event, entry.evidence.event) ||
        event.generation != actor.eventRevision || !equal(event.event, entry.context.event) ||
        !equal(event.device, entry.context.box) || event.registryGeneration != registry.generation)
        return ManualAuthorityResult::Stale;
    companion::PeerRegistryRecord encoded;
    Digest digest;
    if (!registryDigest(registry, encoded, digest))
        return ManualAuthorityResult::Unavailable;
    if (!equal(digest, event.registryDigest) || encoded.bytes != entry.evidence.registry.bytes)
        return ManualAuthorityResult::Stale;
    companion::PeerRegistrySnapshot after;
    EventSnapshot afterEvent;
    companion::TrustedAuthority afterAuthority;
    if (!registry_.snapshot(after) || !(registry == after) || !event_.snapshot(afterEvent) ||
        !equal(event, afterEvent) ||
        !access_.route(original.connection.connection, afterAuthority) ||
        !(actual == afterAuthority) || !source_.current(original) ||
        !proof_.originalLive(original.connection) ||
        !proof_.credentialCurrent(entry.evidence.credential))
        return ManualAuthorityResult::Stale;
    return ManualAuthorityResult::Current;
}
ManualAuthorityResult ManualOverrideAuthority::capture(const Context &requestedContext,
                                                       const ManualOriginalIngress &requestedOrigin,
                                                       OverrideAuthority &output) {
    const Context context = requestedContext;
    const ManualOriginalIngress original = requestedOrigin;
    if (operating_)
        return ManualAuthorityResult::Unavailable;
    Operation operation(operating_);
    if (!valid() || contains(work_, &output, sizeof(output)))
        return ManualAuthorityResult::Unavailable;
    output = {};
    if (!original.connection.valid() || !original.actor.valid() ||
        !(original.connection.device == original.actor.device) ||
        original.connection.registration != original.actor.registration ||
        original.connection.interface != original.actor.interface ||
        !equal(context.box, original.actor.device) || !original.actor.authority.gameMaster ||
        !source_.current(original) || !proof_.originalLive(original.connection))
        return ManualAuthorityResult::Closed;
    companion::TrustedAuthority authority;
    if (!access_.route(original.connection.connection, authority) ||
        !(authority == original.actor.authority) || !authority.gameMaster)
        return ManualAuthorityResult::Closed;
    companion::PeerRegistrySnapshot registry;
    if (!registry_.snapshot(registry))
        return ManualAuthorityResult::Unavailable;
    ManualAuthorityEntry entry;
    entry.context = context;
    entry.evidence.original = original;
    unsigned matches = 0;
    for (unsigned i = 0; i < registry.count; ++i)
        if (registry.peers[i].gameMaster && registry.peers[i].principal == authority.principal) {
            entry.evidence.credential = registry.peers[i].credential;
            ++matches;
        }
    if (matches != 1 || !equal(context.box, registry.device) ||
        !registry_.credentialCurrent(entry.evidence.credential) ||
        !proof_.credentialCurrent(entry.evidence.credential))
        return ManualAuthorityResult::Closed;
    bool possession = false;
    for (std::size_t i = 0; i < work_.connections.count; ++i) {
        const auto &proof = work_.connections.data[i];
        if (proof.verified.original == original.connection &&
            proof.verified.credential == entry.evidence.credential) {
            if (possession)
                return ManualAuthorityResult::Stale;
            entry.evidence.possession = proof;
            possession = true;
        }
    }
    if (!possession)
        return ManualAuthorityResult::Unavailable;
    if (!event_.snapshot(entry.evidence.event))
        return ManualAuthorityResult::Unavailable;
    Digest digest;
    if (!registryDigest(registry, entry.evidence.registry, digest))
        return ManualAuthorityResult::Unavailable;
    const auto &event = entry.evidence.event;
    if (event.phase != Phase::Bound || !equal(event.event, context.event) ||
        !equal(event.device, context.box) || event.registryGeneration != registry.generation ||
        !equal(event.registryDigest, digest))
        return ManualAuthorityResult::Closed;
    auto &a = entry.authority;
    std::memcpy(a.peer.bytes, entry.evidence.credential.peer.bytes.data(), 16);
    a.principal = authority.principal;
    a.roleEpoch = authority.epoch;
    a.connectionToken = original.connection.connection.token;
    a.connectionHandle = original.connection.connection.handle;
    a.registration = original.actor.registration;
    a.lifecycle = original.actor.lifecycle;
    a.registryRevision = registry.generation;
    a.eventRevision = event.generation;
    const auto allowed = current(entry);
    if (allowed != ManualAuthorityResult::Current)
        return allowed;
    for (std::size_t i = 0; i < work_.entries.count; ++i) {
        const auto &prior = work_.entries.data[i];
        if (sameContext(prior.context, context) && equal(prior.authority, a)) {
            if (!(prior.evidence.original.connection == original.connection) ||
                !(prior.evidence.original.actor == original.actor))
                return ManualAuthorityResult::Stale;
            output = a;
            return ManualAuthorityResult::Current;
        }
    }
    if (work_.entries.count == work_.entries.capacity)
        return ManualAuthorityResult::Unavailable;
    work_.entries.data[work_.entries.count++] = entry;
    output = a;
    return ManualAuthorityResult::Current;
}
ManualAuthorityResult ManualOverrideAuthority::check(const Context &requestedContext,
                                                     const OverrideAuthority &requestedAuthority) {
    const Context context = requestedContext;
    const OverrideAuthority authority = requestedAuthority;
    if (operating_)
        return ManualAuthorityResult::Unavailable;
    Operation operation(operating_);
    if (!valid())
        return ManualAuthorityResult::Unavailable;
    const ManualAuthorityEntry *entry = nullptr;
    for (std::size_t i = 0; i < work_.entries.count; ++i)
        if (sameContext(work_.entries.data[i].context, context) &&
            equal(work_.entries.data[i].authority, authority)) {
            if (entry)
                return ManualAuthorityResult::Stale;
            entry = &work_.entries.data[i];
        }
    return entry ? current(*entry) : ManualAuthorityResult::Stale;
}
bool ManualOverrideAuthority::evidence(const Context &requestedContext,
                                       const OverrideAuthority &requestedAuthority,
                                       ManualAuthorityEvidence &output,
                                       configuration::module_ir::Pool<std::uint8_t> &bytes) {
    const Context context = requestedContext;
    const OverrideAuthority authority = requestedAuthority;
    if (operating_)
        return false;
    Operation operation(operating_);
    if (!valid() || contains(work_, &output, sizeof(output)) ||
        contains(work_, bytes.data, bytes.capacity))
        return false;
    output = {};
    bytes.count = 0;
    for (std::size_t i = 0; i < work_.entries.count; ++i) {
        const auto &entry = work_.entries.data[i];
        if (!sameContext(entry.context, context) || !equal(entry.authority, authority))
            continue;
        if (current(entry) != ManualAuthorityResult::Current || !event_.evidence(bytes))
            return false;
        if (current(entry) != ManualAuthorityResult::Current) {
            bytes.count = 0;
            return false;
        }
        output = entry.evidence;
        return true;
    }
    return false;
}
} // namespace integration
} // namespace larp
