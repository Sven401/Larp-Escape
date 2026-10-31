#include "GameExecution/RulesPlanner.h"
#include <cstring>
#include <limits>

namespace larp {
namespace execution {
namespace rules {
namespace {
using configuration::module_ir::SymbolKind;
bool produces(Op op) {
    return op == Op::Emit || op == Op::Override;
}
template <typename T> bool valid(const Pool<T> &p) {
    return p.count <= p.capacity && (!p.count || p.data);
}
template <typename T> bool valid(Span<T> p) {
    return !p.count || p.data;
}
template <typename T> Id append(Pool<T> &p) {
    if (!p.data || p.count >= p.capacity || p.count >= 65534)
        return 0;
    p.data[p.count] = T{};
    return static_cast<Id>(++p.count);
}
template <std::size_t N> bool bytesEqual(const std::uint8_t (&a)[N], const std::uint8_t (&b)[N]) {
    return std::memcmp(a, b, N) == 0;
}
bool equal(const Identity &a, const Identity &b) {
    return bytesEqual(a.bytes, b.bytes);
}
bool equal(const Digest &a, const Digest &b) {
    return bytesEqual(a.bytes, b.bytes);
}
bool nonzero(const Identity &id) {
    for (auto b : id.bytes)
        if (b)
            return true;
    return false;
}
bool equal(const FactId &a, const FactId &b) {
    return equal(a.event, b.event) && equal(a.origin, b.origin) && equal(a.epoch, b.epoch) &&
           a.sequence == b.sequence;
}
bool equal(const FactRef &a, const FactRef &b) {
    return a.type == b.type && equal(a.id, b.id) && equal(a.digest, b.digest) &&
           equal(a.deployment, b.deployment) && equal(a.binding, b.binding) &&
           equal(a.run, b.run) && equal(a.station, b.station) && equal(a.subject, b.subject);
}
bool equal(const Context &a, const Context &b) {
    return a.bindingGeneration == b.bindingGeneration && equal(a.event, b.event) &&
           equal(a.run, b.run) && equal(a.station, b.station) && equal(a.box, b.box) &&
           equal(a.sourceEpoch, b.sourceEpoch) && equal(a.deployment, b.deployment) &&
           equal(a.binding, b.binding) && equal(a.content, b.content);
}
bool equal(const Fence &a, const Fence &b) {
    return a.progress == b.progress && a.profile == b.profile && a.trust == b.trust &&
           a.facts == b.facts && a.ledger == b.ledger && a.resources == b.resources &&
           a.rights == b.rights && a.capacity == b.capacity && a.lastIssued == b.lastIssued &&
           a.activeTime == b.activeTime;
}
bool equal(const OverrideAuthority &a, const OverrideAuthority &b) {
    return equal(a.peer, b.peer) && a.principal == b.principal && a.roleEpoch == b.roleEpoch &&
           a.connectionToken == b.connectionToken && a.connectionHandle == b.connectionHandle &&
           a.registration == b.registration && a.lifecycle == b.lifecycle &&
           a.registryRevision == b.registryRevision && a.eventRevision == b.eventRevision;
}
int compare(const FactRef &a, const FactRef &b) {
    int n = std::memcmp(a.id.event.bytes, b.id.event.bytes, 16);
    if (n)
        return n;
    n = std::memcmp(a.id.origin.bytes, b.id.origin.bytes, 16);
    if (n)
        return n;
    n = std::memcmp(a.id.epoch.bytes, b.id.epoch.bytes, 16);
    if (n)
        return n;
    if (a.id.sequence != b.id.sequence)
        return a.id.sequence < b.id.sequence ? -1 : 1;
    return std::memcmp(a.digest.bytes, b.digest.bytes, 32);
}
const char *sourceName(const configuration::ModuleCompileWorkspace &s, Id id) {
    if (!id || id > s.stringOffsets.count || !s.stringOffsets.data || !s.strings.data)
        return "";
    const auto start = s.stringOffsets.data[id - 1];
    if (start >= s.strings.count ||
        !std::memchr(s.strings.data + start, 0, s.strings.count - start))
        return "";
    return s.strings.data + start;
}
template <typename T> Id lookup(const Pool<T> &p, Id id) {
    for (std::size_t i = 0; i < p.count; ++i)
        if (p.data[i].name == id)
            return static_cast<Id>(i + 1);
    return 0;
}
bool positive(const Program &p, Id index) {
    if (!index || index > p.nodes.count)
        return false;
    const auto &n = p.nodes.data[index - 1];
    if (n.op != Op::Fact && n.op != Op::All && n.op != Op::Any)
        return false;
    for (unsigned i = 0; i < n.count; ++i)
        if (!positive(p, n.children[i]))
            return false;
    return true;
}
bool safePlan(const Program &program, const State &state, const Plan &plan,
              const PlannerWorkspace &work);
Type expressionType(const Program &p, Id index, const Rule &r, unsigned depth,
                    std::uint8_t active[512], std::size_t &visits, Error &error) {
    if (!index || index > p.nodes.count || depth > 8 || ++visits > 4096) {
        error = Error::Budget;
        return Type::Invalid;
    }
    if (active[index - 1]) {
        error = Error::InvalidProgram;
        return Type::Invalid;
    }
    active[index - 1] = 1;
    const auto &n = p.nodes.data[index - 1];
    if (n.count > 8 || (n.op < Op::Not && n.count)) {
        error = Error::InvalidProgram;
        active[index - 1] = 0;
        return Type::Invalid;
    }
    Type out = Type::Invalid;
    if (n.op == Op::Literal) {
        out = n.type;
        if (out != Type::Boolean && out != Type::Integer)
            error = Error::Type;
        if (out == Type::Boolean && n.value != 0 && n.value != 1)
            error = Error::Type;
    } else if (n.op == Op::Value) {
        if (!n.ref || n.ref > p.values.count)
            error = Error::Reference;
        else
            out = p.values.data[n.ref - 1].type;
    } else if (n.op == Op::Fact) {
        if (!n.ref || n.ref > p.subjects.count)
            error = Error::Reference;
        out = Type::Boolean;
    } else if (n.op == Op::Input) {
        if (r.ir.trigger != 1 || !r.ir.triggerCount)
            error = Error::Type;
        else
            out = p.ports.data[p.triggers.data[r.ir.firstTrigger] - 1].type;
    } else if (n.op == Op::PortIs) {
        if (r.ir.trigger != 1 || !n.ref || n.ref > p.ports.count ||
            !p.ports.data[n.ref - 1].input ||
            (p.ports.data[n.ref - 1].type != Type::Integer &&
             p.ports.data[n.ref - 1].type != Type::Boolean))
            error = Error::Type;
        out = Type::Boolean;
    } else {
        if (n.count > 8 || (n.op == Op::Not                        ? n.count != 1
                            : (n.op == Op::All || n.op == Op::Any) ? n.count < 2
                                                                   : n.count != 2))
            error = Error::InvalidProgram;
        Type first = Type::Invalid;
        for (unsigned i = 0; i < n.count && i < 8; ++i) {
            const Type t = expressionType(p, n.children[i], r, depth + 1, active, visits, error);
            if (!i)
                first = t;
            if (n.op == Op::Not || n.op == Op::All || n.op == Op::Any) {
                if (t != Type::Boolean)
                    error = Error::Type;
            } else if (n.op == Op::Eq) {
                if (t != first || (t != Type::Boolean && t != Type::Integer))
                    error = Error::Type;
            } else if (t != Type::Integer)
                error = Error::Type;
        }
        if (n.op < Op::Not || n.op > Op::Sub)
            error = Error::InvalidProgram;
        out = n.op == Op::Add || n.op == Op::Sub ? Type::Integer : Type::Boolean;
    }
    active[index - 1] = 0;
    return out;
}
bool boundVisit(const Program &p, Id index, bool visited[512], std::size_t &count) {
    if (++count > 4096)
        return false;
    if (visited[index - 1])
        return true;
    visited[index - 1] = true;
    const auto &n = p.nodes.data[index - 1];
    for (unsigned i = 0; i < n.count; ++i)
        if (!boundVisit(p, n.children[i], visited, count))
            return false;
    return true;
}
bool inputBound(const Program &p, unsigned kind, Id target) {
    bool visited[512] = {};
    std::size_t visits = 0;
    for (std::size_t ri = 0; ri < p.rules.count; ++ri) {
        const auto &r = p.rules.data[ri].ir;
        if (r.trigger != kind)
            continue;
        bool match = (kind == 2 && r.timer == target) || (kind == 4 && ri + 1 == target);
        if (kind != 2 && kind != 4)
            for (unsigned j = 0; j < r.triggerCount; ++j)
                match |= p.triggers.data[r.firstTrigger + j] == target;
        if (!match)
            continue;
        if (!boundVisit(p, r.guard, visited, visits))
            return false;
        for (unsigned j = 0; j < r.actionCount; ++j) {
            if (++visits > 4096)
                return false;
            const auto &a = p.actions.data[r.firstAction + j];
            if (a.expr && !boundVisit(p, a.expr, visited, visits))
                return false;
        }
    }
    return true;
}

// Decoder input is untrusted too: no semantic check below may first dereference
// a span/index that has only been asserted by the original compiler builder.
template <typename T> bool ownedPool(const Pool<T> &p) {
    return valid(p) && p.count <= 65534 &&
           p.capacity <= std::numeric_limits<std::size_t>::max() / sizeof(T) &&
           (!p.capacity || (p.data && reinterpret_cast<std::uintptr_t>(p.data) % alignof(T) == 0));
}
bool named(const Program &p, Id id) {
    return id && id <= p.offsets.count && *name(p, id);
}
template <typename T> bool uniqueNames(const Program &p, const Pool<T> &pool) {
    for (std::size_t i = 0; i < pool.count; ++i) {
        if (!named(p, pool.data[i].name))
            return false;
        for (std::size_t j = 0; j < i; ++j)
            if (pool.data[i].name == pool.data[j].name)
                return false;
    }
    return true;
}
bool ownedRepresentation(const Program &p) {
    if (!ownedPool(p.strings) || !ownedPool(p.offsets) || !ownedPool(p.nodes) ||
        !ownedPool(p.actions) || !ownedPool(p.rules) || !ownedPool(p.values) ||
        !ownedPool(p.timers) || !ownedPool(p.ports) || !ownedPool(p.subjects) ||
        !ownedPool(p.resources) || !ownedPool(p.epochs) || !ownedPool(p.allowedResources) ||
        !ownedPool(p.triggers) || (p.language != 1 && p.language != 2) ||
        (p.language == 2 && !p.manualOverride) || !nonzero(p.context.event) ||
        !nonzero(p.context.run) || !nonzero(p.context.station) || !nonzero(p.context.box) ||
        !nonzero(p.context.sourceEpoch))
        return false;
    for (std::size_t i = 0; i < p.offsets.count; ++i) {
        const auto offset = p.offsets.data[i];
        if (offset >= p.strings.count ||
            !std::memchr(p.strings.data + offset, 0, p.strings.count - offset))
            return false;
        for (std::size_t j = 0; j < i; ++j)
            if (!std::strcmp(name(p, static_cast<Id>(j + 1)), name(p, static_cast<Id>(i + 1))))
                return false;
    }
    if (!uniqueNames(p, p.rules) || !uniqueNames(p, p.values) || !uniqueNames(p, p.timers) ||
        !uniqueNames(p, p.ports) || !uniqueNames(p, p.subjects) || !uniqueNames(p, p.resources))
        return false;
    for (std::size_t i = 0; i < p.subjects.count; ++i) {
        const auto &b = p.subjects.data[i].binding;
        if (b.name || !nonzero(b.subject) || !nonzero(b.station) || !nonzero(b.box) ||
            !b.epochsComplete || (b.factTypes & ~6u) || !valid(b.epochs))
            return false;
        const auto first = reinterpret_cast<std::uintptr_t>(p.epochs.data);
        const auto at = reinterpret_cast<std::uintptr_t>(b.epochs.data);
        if (b.epochs.count > p.epochs.count || at < first ||
            at - first > p.epochs.count * sizeof(Identity) || (at - first) % sizeof(Identity) ||
            b.epochs.count > (p.epochs.count - (at - first) / sizeof(Identity)))
            return false;
        bool current = false;
        for (std::size_t j = 0; j < b.epochs.count; ++j) {
            if (!nonzero(b.epochs.data[j]))
                return false;
            current |= equal(b.epochs.data[j], p.context.sourceEpoch);
            for (std::size_t k = 0; k < j; ++k)
                if (equal(b.epochs.data[j], b.epochs.data[k]))
                    return false;
        }
        if (equal(b.station, p.context.station) &&
            (!equal(b.box, p.context.box) || !current || !equal(b.binding, p.context.binding)))
            return false;
    }
    for (std::size_t i = 0; i < p.ports.count; ++i) {
        const auto &v = p.ports.data[i];
        if (v.type != (v.input ? Type::Boolean : Type::Light) &&
            v.type != (v.input ? Type::Integer : Type::Audio))
            return false;
        if (v.firstResource > p.allowedResources.count ||
            v.resourceCount > p.allowedResources.count - v.firstResource)
            return false;
        if (!v.input && !nonzero(v.exclusive))
            return false;
        for (unsigned j = 0; j < v.resourceCount; ++j) {
            Id id = p.allowedResources.data[v.firstResource + j];
            if (!id || id > p.resources.count || p.resources.data[id - 1].type != v.type)
                return false;
            for (unsigned k = 0; k < j; ++k)
                if (p.allowedResources.data[v.firstResource + k] == id)
                    return false;
        }
    }
    for (std::size_t i = 0; i < p.resources.count; ++i)
        if (p.resources.data[i].type != Type::Light && p.resources.data[i].type != Type::Audio)
            return false;
    for (std::size_t i = 0; i < p.timers.count; ++i)
        if (p.timers.data[i].type != Type::Integer)
            return false;
    for (std::size_t i = 0; i < p.nodes.count; ++i) {
        const auto &n = p.nodes.data[i];
        if (n.count > 8 || n.op < Op::Literal || n.op > Op::Sub || n.op == Op::Parameter)
            return false;
        for (unsigned j = 0; j < n.count; ++j)
            if (!n.children[j] || n.children[j] > p.nodes.count)
                return false;
    }
    for (std::size_t i = 0; i < p.rules.count; ++i) {
        const auto &rule = p.rules.data[i];
        const auto &r = rule.ir;
        if (!nonzero(rule.stable) || r.owner || r.id || r.firstAction > p.actions.count ||
            r.actionCount > p.actions.count - r.firstAction || r.firstTrigger > p.triggers.count ||
            r.triggerCount > p.triggers.count - r.firstTrigger)
            return false;
    }
    // All actions are checked before pair comparison and cross-rule conflict
    // scans, including later rules whose index/type validation has not run yet.
    for (std::size_t i = 0; i < p.actions.count; ++i) {
        const auto &a = p.actions.data[i];
        if (a.op == Op::Set || a.op == Op::Add) {
            if (!a.target || a.target > p.values.count)
                return false;
        } else if (a.op == Op::Arm || a.op == Op::Cancel) {
            if (!a.target || a.target > p.timers.count)
                return false;
        } else if (produces(a.op)) {
            if (!a.target || a.target > p.subjects.count)
                return false;
        } else if (a.op == Op::Effect) {
            if (!a.target || a.target > p.ports.count || !a.resource ||
                a.resource > p.resources.count)
                return false;
        } else
            return false;
    }
    return true;
}
} // namespace
bool sameContext(const Context &a, const Context &b) {
    return equal(a, b);
}
bool sameFence(const Fence &a, const Fence &b) {
    return equal(a, b);
}
bool overridePlanningFence(const Fence &original, const Fence &planning) {
    if (planning.activeTime < original.activeTime)
        return false;
    Fence copy = planning;
    copy.activeTime = original.activeTime;
    return sameFence(original, copy);
}
bool sameOverrideRecord(const OverrideRecord &a, const OverrideRecord &b) {
    return equal(a.context, b.context) && equal(a.program, b.program) &&
           equal(a.expected, b.expected) && equal(a.authority, b.authority) &&
           equal(a.ownerBoot, b.ownerBoot) && equal(a.token, b.token) &&
           equal(a.prepareId, b.prepareId) && equal(a.confirmId, b.confirmId) &&
           equal(a.operation, b.operation) && equal(a.rule, b.rule) &&
           equal(a.prepareDigest, b.prepareDigest) && equal(a.confirmDigest, b.confirmDigest) &&
           a.original == b.original && a.replacement == b.replacement &&
           a.originalGeneration == b.originalGeneration &&
           a.replacementGeneration == b.replacementGeneration && a.preparedAt == b.preparedAt &&
           a.expiresAt == b.expiresAt && a.confirmedAt == b.confirmedAt;
}
const char *name(const Program &p, Id id) {
    if (!id || id > p.offsets.count || !p.offsets.data || !p.strings.data)
        return "";
    const auto pos = p.offsets.data[id - 1];
    return pos < p.strings.count ? p.strings.data + pos : "";
}
ProgramBuilder::ProgramBuilder(Program &p, const char *stationName, const Context &context,
                               Span<RuleBinding> rules, Span<SubjectBinding> subjects,
                               Span<PortBinding> ports)
    : program_(p), station_(stationName), context_(context), bindings_(rules),
      subjectBindings_(subjects), portBindings_(ports) {}
bool ProgramBuilder::begin(const std::uint8_t digest[32]) {
    result_ = {};
    found_ = prepared_ = false;
    program_.ready = false;
    program_.language = 1;
    program_.manualOverride = false;
    program_.context = context_;
    if (std::memcmp(context_.content.bytes, digest, 32)) {
        result_.error = Error::Context;
        return false;
    }
    program_.strings.count = program_.offsets.count = program_.nodes.count =
        program_.actions.count = program_.rules.count = program_.values.count =
            program_.timers.count = program_.ports.count = program_.subjects.count =
                program_.resources.count = program_.triggers.count = program_.epochs.count =
                    program_.allowedResources.count = 0;
    if (!station_ || !valid(bindings_) || !valid(subjectBindings_) || !valid(portBindings_) ||
        !nonzero(context_.event) || !nonzero(context_.run) || !nonzero(context_.station) ||
        !nonzero(context_.box) || !nonzero(context_.sourceEpoch)) {
        result_.error = Error::Context;
        return false;
    }
    return true;
}
Id ProgramBuilder::intern(const char *s) {
    if (!s) {
        result_.error = Error::Reference;
        return 0;
    }
    for (std::size_t i = 0; i < program_.offsets.count; ++i)
        if (!std::strcmp(name(program_, static_cast<Id>(i + 1)), s))
            return static_cast<Id>(i + 1);
    const std::size_t count = std::strlen(s) + 1;
    if (!program_.strings.data || program_.strings.count + count > program_.strings.capacity) {
        result_.error = Error::OperatingProfile;
        return 0;
    }
    const Id id = append(program_.offsets);
    if (!id) {
        result_.error = Error::OperatingProfile;
        return 0;
    }
    program_.offsets.data[id - 1] = static_cast<std::uint32_t>(program_.strings.count);
    std::memcpy(program_.strings.data + program_.strings.count, s, count);
    program_.strings.count += count;
    return id;
}
Id ProgramBuilder::qualified(const configuration::module_ir::Name &n,
                             const configuration::ModuleCompileWorkspace &s) {
    char text[486];
    std::size_t used = 0;
    if (!n.count || n.count > 5) {
        result_.error = Error::Reference;
        return 0;
    }
    for (unsigned i = 0; i < n.count; ++i) {
        const char *segment = sourceName(s, n.segments[i]);
        if (!*segment || std::strlen(segment) > 48) {
            result_.error = Error::Reference;
            return 0;
        }
        text[used++] = '/';
        for (; *segment; ++segment) {
            if (*segment == '/' || *segment == '~') {
                text[used++] = '~';
                text[used++] = *segment == '/' ? '1' : '0';
            } else
                text[used++] = *segment;
        }
    }
    text[used] = 0;
    return intern(text);
}
bool ProgramBuilder::station(const configuration::ModuleCompileWorkspace &s,
                             const configuration::ExpandedStationWorkspace &e, Id stationId) {
    if (result_.error != Error::None)
        return false;
    if (!valid(s.definitions) || !stationId || stationId > s.definitions.count) {
        result_.error = Error::Reference;
        return false;
    }
    if (std::strcmp(sourceName(s, s.definitions.data[stationId - 1].id), station_))
        return true;
    if (found_) {
        result_.error = Error::Conflict;
        return false;
    }
    found_ = true;
    program_.language = s.language;
    program_.manualOverride = s.manualOverride;
    if (s.language != e.language || (s.language != 1 && s.language != 2) ||
        (s.language == 2 && !s.manualOverride)) {
        result_.error = Error::Unsupported;
        return false;
    }
    if (!valid(s.symbols) || !valid(e.values) || !valid(e.timers) || !valid(e.nodes) ||
        !valid(e.actions) || !valid(e.rules) || !valid(e.triggers)) {
        result_.error = Error::Reference;
        return false;
    }
    for (std::size_t i = 0; i < s.symbols.count; ++i) {
        const auto &v = s.symbols.data[i];
        Id out = 0;
        if (v.kind == SymbolKind::Port && v.owner == stationId) {
            out = append(program_.ports);
            if (out) {
                auto &p = program_.ports.data[out - 1];
                p.name = intern(sourceName(s, v.id));
                p.type = v.type;
                p.input = v.input;
            }
        } else if (v.kind == SymbolKind::Resource && v.owner == 0) {
            out = append(program_.resources);
            if (out) {
                auto &r = program_.resources.data[out - 1];
                r.name = intern(sourceName(s, v.id));
                r.type = v.type;
            }
        } else if (v.kind == SymbolKind::Subject && v.owner == 0) {
            out = append(program_.subjects);
            if (out) {
                auto &subject = program_.subjects.data[out - 1];
                subject.name = intern(sourceName(s, v.id));
                unsigned matches = 0;
                for (std::size_t j = 0; j < subjectBindings_.count; ++j) {
                    const auto &binding = subjectBindings_.data[j];
                    if (binding.name && !std::strcmp(binding.name, sourceName(s, v.id))) {
                        subject.binding = binding;
                        subject.binding.name = nullptr;
                        ++matches;
                    }
                }
                if (matches != 1 || !nonzero(subject.binding.subject) ||
                    !nonzero(subject.binding.station) || !nonzero(subject.binding.box) ||
                    !valid(subject.binding.epochs))
                    result_.error = Error::Reference;
                const std::size_t start = program_.epochs.count,
                                  count = (subject.binding.epochsComplete ||
                                           subject.binding.epochs.count)
                                              ? subject.binding.epochs.count
                                              : 1;
                bool ownEpoch = false;
                for (std::size_t k = 0; k < count; ++k) {
                    const Identity epoch = subject.binding.epochs.count
                                               ? subject.binding.epochs.data[k]
                                               : subject.binding.epoch;
                    if (!nonzero(epoch))
                        result_.error = Error::Reference;
                    Id id = append(program_.epochs);
                    if (!id) {
                        result_.error = Error::OperatingProfile;
                        break;
                    }
                    program_.epochs.data[id - 1] = epoch;
                    ownEpoch |= equal(epoch, program_.context.sourceEpoch);
                }
                subject.binding.epochs.data =
                    program_.epochs.data ? program_.epochs.data + start : nullptr;
                subject.binding.epochs.count = program_.epochs.count - start;
                subject.binding.epochsComplete = true;
                if (!std::strcmp(sourceName(s, v.auxiliary), station_) &&
                    !equal(subject.binding.station, program_.context.station))
                    result_.error = Error::Context;
                if (equal(subject.binding.station, program_.context.station) &&
                    (!equal(subject.binding.box, program_.context.box) || !ownEpoch ||
                     !equal(subject.binding.binding, program_.context.binding)))
                    result_.error = Error::Context;
            }
        } else
            continue;
        if (!out)
            result_.error = Error::OperatingProfile;
    }
    for (std::size_t i = 0; i < program_.ports.count; ++i) {
        auto &port = program_.ports.data[i];
        if (port.input)
            continue;
        unsigned matches = 0;
        port.firstResource = static_cast<std::uint32_t>(program_.allowedResources.count);
        for (std::size_t j = 0; j < portBindings_.count; ++j) {
            const auto &binding = portBindings_.data[j];
            if (!binding.name || std::strcmp(binding.name, name(program_, port.name)))
                continue;
            ++matches;
            port.exclusive = binding.exclusive;
            if (!valid(binding.resources) || binding.resources.count > 65534) {
                result_.error = Error::Reference;
                break;
            }
            for (std::size_t k = 0; k < binding.resources.count; ++k) {
                const Id resource = lookup(program_.resources, intern(binding.resources.data[k]));
                if (!resource || program_.resources.data[resource - 1].type != port.type) {
                    result_.error = Error::Reference;
                    break;
                }
                for (std::size_t l = port.firstResource; l < program_.allowedResources.count; ++l)
                    if (program_.allowedResources.data[l] == resource)
                        result_.error = Error::Conflict;
                const Id out = append(program_.allowedResources);
                if (!out) {
                    result_.error = Error::OperatingProfile;
                    break;
                }
                program_.allowedResources.data[out - 1] = resource;
                ++port.resourceCount;
            }
        }
        if (matches != 1 || !nonzero(port.exclusive))
            result_.error = Error::Reference;
    }
    for (std::size_t i = 0; i < portBindings_.count; ++i) {
        bool found = false;
        for (std::size_t j = 0; j < program_.ports.count; ++j)
            found |= portBindings_.data[i].name && !program_.ports.data[j].input &&
                     !std::strcmp(portBindings_.data[i].name,
                                  name(program_, program_.ports.data[j].name));
        if (!found)
            result_.error = Error::Reference;
    }
    for (std::size_t i = 0; i < e.values.count; ++i) {
        Id out = append(program_.values);
        if (!out) {
            result_.error = Error::OperatingProfile;
            break;
        }
        auto &v = program_.values.data[out - 1];
        v.name = qualified(e.values.data[i].name, s);
        v.type = e.values.data[i].type;
        v.initial = e.values.data[i].initial.value;
    }
    for (std::size_t i = 0; i < e.timers.count; ++i) {
        Id out = append(program_.timers);
        if (!out) {
            result_.error = Error::OperatingProfile;
            break;
        }
        auto &t = program_.timers.data[out - 1];
        t.name = qualified(e.timers.data[i].name, s);
        t.type = Type::Integer;
        t.initial = e.timers.data[i].initial.value;
    }
    for (std::size_t i = 0; i < e.nodes.count; ++i) {
        Id out = append(program_.nodes);
        if (!out) {
            result_.error = Error::OperatingProfile;
            break;
        }
        auto &n = program_.nodes.data[out - 1];
        n = e.nodes.data[i];
        if (n.op == Op::Fact)
            n.ref = lookup(program_.subjects, intern(sourceName(s, n.ref)));
        if (n.op == Op::PortIs)
            n.ref = lookup(program_.ports, intern(sourceName(s, n.ref)));
    }
    for (std::size_t i = 0; i < e.actions.count; ++i) {
        Id out = append(program_.actions);
        if (!out) {
            result_.error = Error::OperatingProfile;
            break;
        }
        auto &a = program_.actions.data[out - 1];
        a = e.actions.data[i];
        if (produces(a.op))
            a.target = lookup(program_.subjects, intern(sourceName(s, a.target)));
        if (a.op == Op::Effect) {
            a.target = lookup(program_.ports, intern(sourceName(s, a.target)));
            a.resource = lookup(program_.resources, intern(sourceName(s, a.resource)));
        }
    }
    for (std::size_t i = 0; i < e.rules.count; ++i) {
        Id out = append(program_.rules);
        if (!out) {
            result_.error = Error::OperatingProfile;
            break;
        }
        auto &r = program_.rules.data[out - 1];
        r.ir = e.rules.data[i].rule;
        r.ir.group = 0;
        r.ir.fallback = r.ir.policy == 3 ? qualified(e.rules.data[i].fallbackName, s) : 0;
        r.name = qualified(e.rules.data[i].name, s);
        unsigned matches = 0;
        for (std::size_t j = 0; j < bindings_.count; ++j)
            if (bindings_.data[j].qualified &&
                !std::strcmp(bindings_.data[j].qualified, name(program_, r.name))) {
                r.stable = bindings_.data[j].ruleId;
                ++matches;
            }
        if (matches != 1 || !nonzero(r.stable))
            result_.error = Error::Reference;
        r.ir.owner = r.ir.id = 0;
        r.ir.firstTrigger = static_cast<Id>(program_.triggers.count);
        if (static_cast<std::size_t>(e.rules.data[i].rule.firstTrigger) + r.ir.triggerCount >
            e.triggers.count) {
            result_.error = Error::Reference;
            break;
        }
        for (std::size_t j = 0; j < r.ir.triggerCount; ++j) {
            Id t = append(program_.triggers);
            if (!t) {
                result_.error = Error::OperatingProfile;
                break;
            }
            const Id original = e.triggers.data[e.rules.data[i].rule.firstTrigger + j];
            program_.triggers.data[t - 1] =
                r.ir.trigger == 3 ? lookup(program_.subjects, intern(sourceName(s, original)))
                                  : lookup(program_.ports, intern(sourceName(s, original)));
        }
    }
    for (std::size_t i = 0; i < program_.rules.count; ++i) {
        auto &r = program_.rules.data[i].ir;
        if (r.policy != 3)
            continue;
        const Id wanted = r.fallback;
        r.fallback = 0;
        for (std::size_t j = 0; j < program_.rules.count; ++j)
            if (program_.rules.data[j].name == wanted)
                r.fallback = static_cast<Id>(j + 1);
        if (!r.fallback)
            result_.error = Error::Reference;
    }
    return result_.error == Error::None;
}
Result validateOwnedProgram(Program &program_) {
    program_.ready = false;
    Result result_;
    const auto check = [&]() -> bool {
        if (!ownedRepresentation(program_)) {
            result_.error = Error::InvalidProgram;
            return false;
        }
        if (program_.nodes.count > 512 || program_.rules.count > 64 ||
            program_.actions.count > 512 || program_.values.count > 32 ||
            program_.timers.count > 16) {
            result_.error = Error::Budget;
            return false;
        }
        for (std::size_t i = 0; i < program_.values.count; ++i) {
            const auto &v = program_.values.data[i];
            if ((v.type != Type::Boolean && v.type != Type::Integer) ||
                (v.type == Type::Boolean && v.initial != 0 && v.initial != 1)) {
                result_.error = Error::Type;
                return false;
            }
        }
        for (std::size_t i = 0; i < program_.timers.count; ++i)
            if (program_.timers.data[i].initial < 1 || program_.timers.data[i].initial > 86400000) {
                result_.error = Error::InvalidProgram;
                return false;
            }
        for (std::size_t i = 0; i < program_.subjects.count; ++i)
            for (std::size_t j = 0; j < i; ++j)
                if (equal(program_.subjects.data[i].binding.subject,
                          program_.subjects.data[j].binding.subject)) {
                    result_.error = Error::Conflict;
                    return false;
                }
        for (std::size_t i = 0; i < program_.rules.count; ++i)
            program_.rules.data[i].ir.group = 0;
        for (std::size_t i = 0; i < program_.rules.count; ++i) {
            auto &original = program_.rules.data[i];
            auto &r = original.ir;
            if (r.policy != 3)
                continue;
            if (program_.language != 2 || !program_.manualOverride) {
                result_.error = Error::Unsupported;
                return false;
            }
            if (!r.fallback || r.fallback > program_.rules.count) {
                result_.error = Error::Reference;
                return false;
            }
            auto &replacement = program_.rules.data[r.fallback - 1];
            if (r.group || replacement.ir.group || !equal(original.stable, replacement.stable) ||
                !configuration::module_ir::manualReleasePair(program_.actions, r, replacement.ir)) {
                result_.error = Error::Conflict;
                return false;
            }
            r.group = replacement.ir.group = static_cast<Id>(i + 1);
        }
        for (std::size_t i = 0; i < program_.rules.count; ++i) {
            const auto &r = program_.rules.data[i];
            for (std::size_t j = 0; j < i; ++j)
                if (equal(r.stable, program_.rules.data[j].stable) &&
                    (!r.ir.group || r.ir.group != program_.rules.data[j].ir.group)) {
                    result_.error = Error::Conflict;
                    return false;
                }
            if ((r.ir.policy == 3 || r.ir.trigger == 4 || r.ir.fallback) &&
                program_.language != 2) {
                result_.error = Error::Unsupported;
                return false;
            }
            if (!r.ir.generation || r.ir.trigger < 1 ||
                r.ir.trigger > (program_.language == 2 ? 4 : 3) ||
                (r.ir.trigger != 3 && r.ir.policy != 2) ||
                (r.ir.trigger == 3 && r.ir.policy != 1 && r.ir.policy != 2 && r.ir.policy != 3) ||
                (r.ir.trigger == 4 && (!r.ir.group || r.ir.triggerCount || r.ir.timer)) ||
                r.ir.actionCount > 8 ||
                static_cast<std::size_t>(r.ir.firstAction) + r.ir.actionCount >
                    program_.actions.count ||
                static_cast<std::size_t>(r.ir.firstTrigger) + r.ir.triggerCount >
                    program_.triggers.count) {
                result_.error = Error::InvalidProgram;
                return false;
            }
            Type inputType = Type::Invalid;
            for (std::size_t j = 0; j < r.ir.triggerCount; ++j) {
                Id id = program_.triggers.data[r.ir.firstTrigger + j];
                if (!id || (r.ir.trigger == 3 ? id > program_.subjects.count
                                              : id > program_.ports.count)) {
                    result_.error = Error::Reference;
                    return false;
                }
                if (r.ir.trigger == 1) {
                    const auto &p = program_.ports.data[id - 1];
                    if (!p.input || (p.type != Type::Boolean && p.type != Type::Integer) ||
                        (inputType != Type::Invalid && inputType != p.type)) {
                        result_.error = Error::Type;
                        return false;
                    }
                    inputType = p.type;
                }
            }
            if (r.ir.trigger == 2 && (!r.ir.timer || r.ir.timer > program_.timers.count)) {
                result_.error = Error::Reference;
                return false;
            }
            std::uint8_t active[512] = {};
            std::size_t visits = 0;
            if (expressionType(program_, r.ir.guard, r, 1, active, visits, result_.error) !=
                Type::Boolean) {
                result_.error = Error::Type;
                return false;
            }
            if (result_.error != Error::None)
                return false;
            if (r.ir.trigger == 3 && (!r.ir.once || !positive(program_, r.ir.guard))) {
                result_.error = Error::Type;
                return false;
            }
            for (std::size_t j = 0; j < r.ir.actionCount; ++j) {
                const auto &a = program_.actions.data[r.ir.firstAction + j];
                ++visits;
                result_.action = static_cast<Id>(j + 1);
                if (a.op != Op::Set && a.op != Op::Add && a.expr) {
                    result_.error = Error::InvalidProgram;
                    return false;
                }
                if (a.op == Op::Set || a.op == Op::Add) {
                    if (!a.target || a.target > program_.values.count) {
                        result_.error = Error::Reference;
                        return false;
                    }
                    const Type t =
                        expressionType(program_, a.expr, r, 1, active, visits, result_.error);
                    if (t != program_.values.data[a.target - 1].type ||
                        (a.op == Op::Add && t != Type::Integer))
                        result_.error = Error::Type;
                } else if (a.op == Op::Arm || a.op == Op::Cancel) {
                    if (!a.target || a.target > program_.timers.count)
                        result_.error = Error::Reference;
                } else if (produces(a.op)) {
                    if (a.op == Op::Override &&
                        (program_.language != 2 || r.ir.trigger != 4 || !r.ir.group)) {
                        result_.error = Error::Unsupported;
                        return false;
                    }
                    if (!a.target || a.target > program_.subjects.count)
                        result_.error = Error::Reference;
                    else if (!equal(program_.subjects.data[a.target - 1].binding.station,
                                    program_.context.station) ||
                             !r.ir.once)
                        result_.error = Error::InvalidProgram;
                    else if (!(program_.subjects.data[a.target - 1].binding.factTypes &
                               (1u << static_cast<unsigned>(a.op == Op::Override
                                                                ? FactType::LocalOverride
                                                                : FactType::TaskSolved))))
                        result_.error = Error::Reference;
                    for (std::size_t k = 0; k < i; ++k) {
                        const auto &other = program_.rules.data[k].ir;
                        for (unsigned l = 0; l < other.actionCount; ++l) {
                            const auto &b = program_.actions.data[other.firstAction + l];
                            if (produces(b.op) && b.target == a.target &&
                                (!r.ir.group || r.ir.group != program_.rules.data[k].ir.group))
                                result_.error = Error::Conflict;
                        }
                    }
                } else if (a.op == Op::Effect) {
                    if (!a.target || a.target > program_.ports.count || !a.resource ||
                        a.resource > program_.resources.count)
                        result_.error = Error::Reference;
                    else {
                        const auto &port = program_.ports.data[a.target - 1];
                        if (port.input || port.type != program_.resources.data[a.resource - 1].type)
                            result_.error = Error::Type;
                        bool allowed = false;
                        for (unsigned k = 0; k < port.resourceCount; ++k)
                            allowed |= program_.allowedResources.data[port.firstResource + k] ==
                                       a.resource;
                        if (!allowed)
                            result_.error = Error::Reference;
                    }
                } else
                    result_.error = Error::InvalidProgram;
                if (r.ir.trigger == 3 && a.op != Op::Emit && a.op != Op::Effect)
                    result_.error = Error::Type;
                if (result_.error != Error::None)
                    return false;
                for (std::size_t k = 0; k <= i; ++k) {
                    const auto &other = program_.rules.data[k];
                    const unsigned end = k == i ? static_cast<unsigned>(j) : other.ir.actionCount;
                    for (unsigned l = 0; l < end; ++l) {
                        const auto &b = program_.actions.data[other.ir.firstAction + l];
                        const bool sameRule = k == i;
                        const bool values = (a.op == Op::Set || a.op == Op::Add) &&
                                            (b.op == Op::Set || b.op == Op::Add);
                        const bool timers = (a.op == Op::Arm || a.op == Op::Cancel) &&
                                            (b.op == Op::Arm || b.op == Op::Cancel);
                        if (sameRule && a.target == b.target &&
                            (values || timers || (produces(a.op) && produces(b.op))))
                            result_.error = Error::Conflict;
                        if (a.op == Op::Effect && b.op == Op::Effect &&
                            (sameRule || r.ir.trigger == 3 || other.ir.trigger == 3) &&
                            (sameRule || !r.ir.group || r.ir.group != other.ir.group) &&
                            equal(program_.ports.data[a.target - 1].exclusive,
                                  program_.ports.data[b.target - 1].exclusive))
                            result_.error = Error::Conflict;
                    }
                }
                if (result_.error != Error::None)
                    return false;
            }
            if (visits > 4096) {
                result_.error = Error::Budget;
                return false;
            }
        }
        // Conservative complete-input bound with the same per-input memoization as
        // evaluation. Mutually exclusive guards may make this bound larger than a
        // reachable plan: failure is an operating-profile exclusion, not P0 invalidity.
        for (std::size_t i = 0; i < program_.ports.count; ++i)
            if (program_.ports.data[i].input && !inputBound(program_, 1, static_cast<Id>(i + 1))) {
                result_.error = Error::OperatingProfile;
                return false;
            }
        for (std::size_t i = 0; i < program_.timers.count; ++i)
            if (!inputBound(program_, 2, static_cast<Id>(i + 1))) {
                result_.error = Error::OperatingProfile;
                return false;
            }
        for (std::size_t i = 0; i < program_.subjects.count; ++i)
            if (!inputBound(program_, 3, static_cast<Id>(i + 1))) {
                result_.error = Error::OperatingProfile;
                return false;
            }
        for (std::size_t i = 0; i < program_.rules.count; ++i)
            if (program_.rules.data[i].ir.trigger == 4 &&
                !inputBound(program_, 4, static_cast<Id>(i + 1))) {
                result_.error = Error::OperatingProfile;
                return false;
            }
        return true;
    };
    if (check())
        result_.decision = Decision::Ready;
    return result_;
}
bool ProgramBuilder::validate() {
    result_ = validateOwnedProgram(program_);
    return result_.error == Error::None;
}
bool ProgramBuilder::prepare() {
    if (!found_) {
        result_.error = Error::Reference;
        return false;
    }
    // station() already requires exactly one matching entry for every copied
    // rule/subject. Equal cardinality makes the maps complete bijections and
    // excludes unused entries from other stations or old deployments.
    if (result_.error == Error::None && (bindings_.count != program_.rules.count ||
                                         subjectBindings_.count != program_.subjects.count))
        result_.error = Error::Reference;
    prepared_ = result_.error == Error::None && validate();
    return prepared_;
}
void ProgramBuilder::commit() {
    program_.ready = prepared_;
    if (prepared_)
        result_.decision = Decision::Ready;
}
void ProgramBuilder::abort() {
    program_.ready = false;
    prepared_ = false;
}
void Planner::fail(Error error, bool deferred) {
    if (result_.error != Error::None)
        return;
    result_.error = error;
    result_.decision = deferred ? Decision::Deferred : Decision::Rejected;
}
bool Planner::validFact(const Fact &f) const {
    if (f.kind != FactClass::CurrentSolved || !f.subject || f.subject > program_->subjects.count ||
        (f.ref.type != FactType::TaskSolved && f.ref.type != FactType::LocalOverride))
        return false;
    const auto &binding = program_->subjects.data[f.subject - 1].binding;
    if (!(binding.factTypes & (1u << static_cast<unsigned>(f.ref.type))))
        return false;
    const auto &r = f.ref;
    bool epoch = false;
    for (std::size_t i = 0; i < binding.epochs.count; ++i)
        epoch |= equal(r.id.epoch, binding.epochs.data[i]);
    return r.id.sequence && epoch && equal(r.id.event, program_->context.event) &&
           equal(r.run, program_->context.run) &&
           equal(r.deployment, program_->context.deployment) && equal(r.id.origin, binding.box) &&
           equal(r.station, binding.station) && equal(r.subject, binding.subject) &&
           equal(r.binding, binding.binding);
}
Id Planner::admit(const FactRef &r) {
    bool matched = false;
    Id subject = 0;
    for (std::size_t i = 0; i < program_->subjects.count; ++i)
        if (equal(r.subject, program_->subjects.data[i].binding.subject)) {
            Fact fact;
            fact.subject = static_cast<Id>(i + 1);
            subject = fact.subject;
            fact.ref = r;
            fact.kind = FactClass::CurrentSolved;
            matched = validFact(fact);
            break;
        }
    if (!matched) {
        fail(Error::Context);
        return 0;
    }
    if (equal(r.station, program_->context.station)) {
        bool solved = false, producer = false;
        for (std::size_t i = 0; i < state_->solved.count; ++i)
            solved |= state_->solved.data[i] == subject;
        for (std::size_t i = 0; i < program_->rules.count; ++i) {
            const auto &rule = program_->rules.data[i];
            for (unsigned j = 0; j < rule.ir.actionCount; ++j) {
                const auto &a = program_->actions.data[rule.ir.firstAction + j];
                if (!produces(a.op) || a.target != subject)
                    continue;
                for (std::size_t k = 0; k < state_->latches.count; ++k)
                    producer |= equal(state_->latches.data[k].rule, rule.stable);
            }
        }
        if (!solved || !producer) {
            fail(Error::InvalidSnapshot);
            return 0;
        }
    }
    if (equal(r.id.origin, program_->context.box) &&
        equal(r.id.epoch, program_->context.sourceEpoch) &&
        r.id.sequence > state_->fence.lastIssued) {
        fail(Error::InvalidSnapshot);
        return 0;
    }
    for (std::size_t i = 0; i < work_->catalog.count; ++i)
        if (equal(r.id, work_->catalog.data[i].id)) {
            if (!equal(r, work_->catalog.data[i]))
                fail(Error::Conflict);
            return static_cast<Id>(i + 1);
        }
    if (work_->catalog.count == 512) {
        fail(Error::OperatingProfile, true);
        return 0;
    }
    const Id id = append(work_->catalog);
    if (!id) {
        fail(Error::OperatingProfile, true);
        return 0;
    }
    work_->catalog.data[id - 1] = r;
    return id;
}
bool Planner::snapshot() {
    const auto &s = *state_;
    if (!equal(s.context, program_->context) || !valid(s.values) || !valid(s.timers) ||
        !valid(s.facts) || !valid(s.latches) || !valid(s.ledger) || !valid(s.solved) ||
        s.values.count != program_->values.count || s.timers.count != program_->timers.count) {
        fail(Error::InvalidSnapshot);
        return false;
    }
    for (std::size_t i = 0; i < s.values.count; ++i) {
        const Value &v = s.values.data[i];
        if (v.type != program_->values.data[i].type ||
            (v.type == Type::Boolean && v.value != 0 && v.value != 1) || !valid(v.evidence.refs)) {
            fail(Error::InvalidSnapshot);
            return false;
        }
        for (std::size_t j = 0; j < v.evidence.refs.count; ++j)
            admit(v.evidence.refs.data[j]);
    }
    for (std::size_t i = 0; i < s.timers.count; ++i) {
        const Timer &t = s.timers.data[i];
        if ((t.active && !t.generation) || !valid(t.evidence.refs) ||
            (!t.active && t.evidence.refs.count)) {
            fail(Error::InvalidSnapshot);
            return false;
        }
        for (std::size_t j = 0; j < t.evidence.refs.count; ++j)
            admit(t.evidence.refs.data[j]);
    }
    for (std::size_t i = 0; i < s.facts.count; ++i) {
        const Fact &f = s.facts.data[i];
        if (!validFact(f)) {
            fail(Error::Context);
            return false;
        }
        admit(f.ref);
        for (std::size_t j = 0; j < i; ++j)
            if (f.subject == s.facts.data[j].subject) {
                fail(Error::InvalidSnapshot);
                return false;
            }
    }
    for (std::size_t i = 0; i < s.latches.count; ++i) {
        const Latch &l = s.latches.data[i];
        if (!nonzero(l.rule) || !l.generation || !valid(l.evidence.refs)) {
            fail(Error::InvalidSnapshot);
            return false;
        }
        for (std::size_t j = 0; j < l.evidence.refs.count; ++j)
            admit(l.evidence.refs.data[j]);
        for (std::size_t j = 0; j < i; ++j)
            if (equal(l.rule, s.latches.data[j].rule)) {
                fail(Error::InvalidSnapshot);
                return false;
            }
    }
    for (std::size_t i = 0; i < s.solved.count; ++i) {
        Id subject = s.solved.data[i];
        if (!subject || subject > program_->subjects.count ||
            !equal(program_->subjects.data[subject - 1].binding.station,
                   program_->context.station)) {
            fail(Error::InvalidSnapshot);
            return false;
        }
        for (std::size_t j = 0; j < i; ++j)
            if (subject == s.solved.data[j]) {
                fail(Error::InvalidSnapshot);
                return false;
            }
    }
    for (std::size_t ri = 0; ri < program_->rules.count; ++ri) {
        const auto &r = program_->rules.data[ri];
        bool latched = false;
        for (std::size_t i = 0; i < s.latches.count; ++i)
            latched |= equal(s.latches.data[i].rule, r.stable);
        for (unsigned ai = 0; ai < r.ir.actionCount; ++ai) {
            const auto &a = program_->actions.data[r.ir.firstAction + ai];
            if (!produces(a.op))
                continue;
            bool solved = false;
            for (std::size_t i = 0; i < s.solved.count; ++i)
                solved |= s.solved.data[i] == a.target;
            if (solved != latched) {
                fail(Error::InvalidSnapshot);
                return false;
            }
            for (std::size_t i = 0; i < s.facts.count; ++i)
                if (s.facts.data[i].subject == a.target && !solved) {
                    fail(Error::InvalidSnapshot);
                    return false;
                }
        }
    }
    for (std::size_t i = 0; i < work_->catalog.count; ++i) {
        const auto &r = work_->catalog.data[i];
        if (equal(r.id.origin, program_->context.box) &&
            equal(r.id.epoch, program_->context.sourceEpoch) &&
            r.id.sequence > s.fence.lastIssued) {
            fail(Error::InvalidSnapshot);
            return false;
        }
    }
    return result_.error == Error::None;
}
Evaluation Planner::evaluate(Id index) {
    if (++visits_ > 4096) {
        fail(Error::Budget);
        return {};
    }
    if (!index || index > program_->nodes.count) {
        fail(Error::InvalidProgram);
        return {};
    }
    Evaluation &cached = work_->evaluations[index - 1];
    if (cached.evaluated)
        return cached;
    cached.evaluated = true;
    const auto &n = program_->nodes.data[index - 1];
    Evaluation out;
    out.evaluated = true;
    result_.node = index;
    if (n.op == Op::Literal) {
        out.type = n.type;
        out.known = true;
        out.value = n.value;
    } else if (n.op == Op::Value) {
        const Value &v = state_->values.data[n.ref - 1];
        out.type = v.type;
        out.known = true;
        out.value = v.value;
    } else if (n.op == Op::Input) {
        out.type = input_->type;
        out.known = input_->known;
        out.value = input_->value;
    } else if (n.op == Op::PortIs) {
        out.type = Type::Boolean;
        out.known = true;
        out.value = input_->kind == InputKind::Local && input_->port == n.ref;
    } else if (n.op == Op::Fact) {
        out.type = Type::Boolean;
        for (std::size_t i = 0; i < state_->facts.count; ++i)
            if (state_->facts.data[i].subject == n.ref) {
                out.known = true;
                out.value = 1;
            }
        if (input_->kind == InputKind::Fact && input_->fact.subject == n.ref) {
            out.known = true;
            out.value = 1;
        }
    } else {
        Evaluation args[8];
        for (unsigned i = 0; i < n.count; ++i)
            args[i] = evaluate(n.children[i]);
        out.type = n.op == Op::Add || n.op == Op::Sub ? Type::Integer : Type::Boolean;
        if (n.op == Op::Not) {
            out.known = args[0].known;
            out.value = !args[0].value;
        } else if (n.op == Op::All || n.op == Op::Any) {
            bool decisive = false, allKnown = true;
            for (unsigned i = 0; i < n.count; ++i) {
                allKnown &= args[i].known;
                if (args[i].known && (n.op == Op::All ? !args[i].value : args[i].value))
                    decisive = true;
            }
            out.known = decisive || allKnown;
            out.value = n.op == Op::All ? !decisive : decisive;
        } else {
            out.known = args[0].known && args[1].known;
            if (out.known) {
                const std::int64_t a = args[0].value, b = args[1].value;
                switch (n.op) {
                case Op::Eq:
                    out.value = a == b;
                    break;
                case Op::Lt:
                    out.value = a < b;
                    break;
                case Op::Le:
                    out.value = a <= b;
                    break;
                case Op::Gt:
                    out.value = a > b;
                    break;
                case Op::Ge:
                    out.value = a >= b;
                    break;
                case Op::Add:
                case Op::Sub: {
                    const std::int64_t v = n.op == Op::Add ? a + b : a - b;
                    if (v < -2147483647LL - 1 || v > 2147483647)
                        fail(Error::Overflow);
                    else
                        out.value = static_cast<std::int32_t>(v);
                    break;
                }
                default:
                    fail(Error::InvalidProgram);
                    break;
                }
            }
        }
    }
    cached = out;
    return out;
}
std::uint32_t Planner::insert(std::uint32_t family, const Id *refs, std::size_t count) {
    if (result_.error != Error::None)
        return family;
    auto subset = [&](const Id *a, std::size_t ac, const Id *b, std::size_t bc) {
        std::size_t i = 0, j = 0;
        while (i < ac && j < bc) {
            if (++proofWork_ > work_->proofWorkLimit) {
                fail(Error::OperatingProfile, true);
                return false;
            }
            const int n = compare(work_->catalog.data[a[i] - 1], work_->catalog.data[b[j] - 1]);
            if (n == 0) {
                ++i;
                ++j;
            } else if (n > 0)
                ++j;
            else
                return false;
        }
        return i == ac;
    };
    for (std::uint32_t p = family; p; p = work_->proofs.data[p - 1].next) {
        const auto &existing = work_->proofs.data[p - 1];
        if (subset(work_->proofRefs.data + existing.first, existing.count, refs, count))
            return family;
    }
    std::uint32_t *link = &family;
    while (*link) {
        auto &existing = work_->proofs.data[*link - 1];
        if (subset(refs, count, work_->proofRefs.data + existing.first, existing.count))
            *link = existing.next;
        else
            link = &existing.next;
    }
    if (count > 512 || (!work_->proofRefs.data && count) ||
        work_->proofRefs.count + count > work_->proofRefs.capacity ||
        work_->proofRefs.count + count > 65534) {
        fail(Error::OperatingProfile, true);
        return family;
    }
    const Id id = append(work_->proofs);
    if (!id) {
        fail(Error::OperatingProfile, true);
        return family;
    }
    auto &proof = work_->proofs.data[id - 1];
    proof.first = static_cast<std::uint32_t>(work_->proofRefs.count);
    proof.count = static_cast<std::uint16_t>(count);
    proof.next = family;
    for (std::size_t i = 0; i < count; ++i)
        work_->proofRefs.data[work_->proofRefs.count++] = refs[i];
    return id;
}
std::uint32_t Planner::singleton(Evidence evidence) {
    if (!valid(evidence.refs) || evidence.refs.count > 512) {
        fail(Error::OperatingProfile, true);
        return 0;
    }
    Id refs[512];
    std::size_t count = 0;
    for (std::size_t i = 0; i < evidence.refs.count; ++i) {
        Id id = admit(evidence.refs.data[i]);
        if (!id)
            return 0;
        std::size_t at = 0;
        while (at < count &&
               compare(work_->catalog.data[refs[at] - 1], work_->catalog.data[id - 1]) < 0)
            ++at;
        if (at < count && refs[at] == id)
            continue;
        for (std::size_t j = count; j > at; --j)
            refs[j] = refs[j - 1];
        refs[at] = id;
        ++count;
    }
    return insert(0, refs, count);
}
std::uint32_t Planner::combine(std::uint32_t left, std::uint32_t right, bool alternatives) {
    std::uint32_t out = 0;
    if (alternatives) {
        for (unsigned pass = 0; pass < 2; ++pass)
            for (std::uint32_t p = pass ? right : left; p; p = work_->proofs.data[p - 1].next) {
                const auto &entry = work_->proofs.data[p - 1];
                out = insert(out, work_->proofRefs.data + entry.first, entry.count);
            }
        return out;
    }
    for (std::uint32_t a = left; a; a = work_->proofs.data[a - 1].next)
        for (std::uint32_t b = right; b; b = work_->proofs.data[b - 1].next) {
            if (++proofWork_ > work_->proofWorkLimit) {
                fail(Error::OperatingProfile, true);
                return 0;
            }
            const Proof &pa = work_->proofs.data[a - 1];
            const Proof &pb = work_->proofs.data[b - 1];
            Id refs[512];
            std::size_t i = 0, j = 0, count = 0;
            while (i < pa.count || j < pb.count) {
                Id id = 0;
                if (i == pa.count)
                    id = work_->proofRefs.data[pb.first + j++];
                else if (j == pb.count)
                    id = work_->proofRefs.data[pa.first + i++];
                else {
                    Id x = work_->proofRefs.data[pa.first + i],
                       y = work_->proofRefs.data[pb.first + j];
                    const int n = compare(work_->catalog.data[x - 1], work_->catalog.data[y - 1]);
                    if (n <= 0) {
                        id = x;
                        ++i;
                        if (!n)
                            ++j;
                    } else {
                        id = y;
                        ++j;
                    }
                }
                if (count == 512) {
                    fail(Error::OperatingProfile, true);
                    return 0;
                }
                refs[count++] = id;
            }
            out = insert(out, refs, count);
        }
    return out;
}
std::uint32_t Planner::proof(Id index) {
    if (result_.error != Error::None)
        return 0;
    Evaluation &e = work_->evaluations[index - 1];
    if (e.proved)
        return e.family;
    if (!e.evaluated || !e.known) {
        fail(Error::InvalidProgram);
        return 0;
    }
    if (e.proving) {
        fail(Error::InvalidProgram);
        return 0;
    }
    e.proving = true;
    const auto &n = program_->nodes.data[index - 1];
    std::uint32_t family = 0;
    if (n.op == Op::Value)
        family = singleton(state_->values.data[n.ref - 1].evidence);
    else if (n.op == Op::Fact) {
        const FactRef *ref = nullptr;
        for (std::size_t i = 0; i < state_->facts.count; ++i)
            if (state_->facts.data[i].subject == n.ref)
                ref = &state_->facts.data[i].ref;
        if (!ref && input_->kind == InputKind::Fact && input_->fact.subject == n.ref)
            ref = &input_->fact.ref;
        if (!ref)
            fail(Error::InvalidSnapshot);
        else {
            Evidence ev;
            ev.refs.data = ref;
            ev.refs.count = 1;
            family = singleton(ev);
        }
    } else if (n.op == Op::Literal || n.op == Op::Input || n.op == Op::PortIs)
        family = singleton({});
    else if (n.op == Op::Not)
        family = proof(n.children[0]);
    else if (n.op == Op::All || n.op == Op::Any) {
        const bool alternatives = (n.op == Op::All && !e.value) || (n.op == Op::Any && e.value);
        if (!alternatives)
            family = singleton({});
        for (unsigned i = 0; i < n.count; ++i) {
            const auto &child = work_->evaluations[n.children[i] - 1];
            if (alternatives && (!child.known || child.value != e.value))
                continue;
            family = combine(family, proof(n.children[i]), alternatives);
        }
    } else
        family = combine(proof(n.children[0]), proof(n.children[1]), false);
    e.proving = false;
    e.proved = true;
    e.family = family;
    return family;
}
bool Planner::select(std::uint32_t family, Evidence &destination, bool newFact) {
    if (result_.error != Error::None)
        return false;
    if (!family) {
        fail(Error::InvalidProgram);
        return false;
    }
    std::uint32_t best = family;
    for (std::uint32_t p = work_->proofs.data[family - 1].next; p;
         p = work_->proofs.data[p - 1].next) {
        const auto &a = work_->proofs.data[p - 1];
        const auto &b = work_->proofs.data[best - 1];
        bool less = a.count < b.count;
        if (a.count == b.count)
            for (unsigned i = 0; i < a.count; ++i) {
                const int n = compare(work_->catalog.data[work_->proofRefs.data[a.first + i] - 1],
                                      work_->catalog.data[work_->proofRefs.data[b.first + i] - 1]);
                if (n) {
                    less = n < 0;
                    break;
                }
            }
        if (less)
            best = p;
    }
    const Proof &selected = work_->proofs.data[best - 1];
    if (newFact && selected.count > 8) {
        fail(Error::DependencyLimit);
        return false;
    }
    const auto start = plan_->evidence.count;
    if (!plan_->evidence.data && selected.count) {
        fail(Error::OperatingProfile, true);
        return false;
    }
    if (start + selected.count > plan_->evidence.capacity) {
        fail(Error::OperatingProfile, true);
        return false;
    }
    for (unsigned i = 0; i < selected.count; ++i)
        plan_->evidence.data[plan_->evidence.count++] =
            work_->catalog.data[work_->proofRefs.data[selected.first + i] - 1];
    destination.refs.data = selected.count ? plan_->evidence.data + start : nullptr;
    destination.refs.count = selected.count;
    return true;
}
bool Planner::action(const Rule &rule, Id index, std::uint32_t cause) {
    if (result_.error != Error::None)
        return false;
    if (++actions_ > 32 || ++visits_ > 4096) {
        fail(Error::Budget);
        return false;
    }
    const auto &a = program_->actions.data[index - 1];
    result_.action = index;
    if (a.op == Op::Set || a.op == Op::Add) {
        for (std::size_t i = 0; i < plan_->values.count; ++i)
            if (plan_->values.data[i].target == a.target) {
                fail(Error::Conflict);
                return false;
            }
        Evaluation value = evaluate(a.expr);
        if (!value.known) {
            fail(Error::Type);
            return false;
        }
        cause = combine(cause, proof(a.expr), false);
        if (a.op == Op::Add) {
            const auto &old = state_->values.data[a.target - 1];
            const std::int64_t result = static_cast<std::int64_t>(old.value) + value.value;
            if (result < -2147483647LL - 1 || result > 2147483647) {
                fail(Error::Overflow);
                return false;
            }
            value.value = static_cast<std::int32_t>(result);
            cause = combine(cause, singleton(old.evidence), false);
        }
        Id out = append(plan_->values);
        if (!out) {
            fail(Error::OperatingProfile, true);
            return false;
        }
        auto &write = plan_->values.data[out - 1];
        write.target = a.target;
        write.value.type = value.type;
        write.value.value = value.value;
        return select(cause, write.value.evidence);
    }
    if (a.op == Op::Arm || a.op == Op::Cancel) {
        for (std::size_t i = 0; i < plan_->timers.count; ++i)
            if (plan_->timers.data[i].target == a.target) {
                fail(Error::Conflict);
                return false;
            }
        const auto &old = state_->timers.data[a.target - 1];
        Timer next = old;
        next.evidence = {};
        if (a.op == Op::Arm) {
            if (old.active && plan_->consumedTimer != a.target) {
                fail(Error::Conflict);
                return false;
            }
            if (old.generation == std::numeric_limits<std::uint64_t>::max() ||
                state_->fence.activeTime >
                    std::numeric_limits<std::uint64_t>::max() -
                        static_cast<std::uint64_t>(program_->timers.data[a.target - 1].initial)) {
                fail(Error::Overflow);
                return false;
            }
            next.active = true;
            next.deadline = state_->fence.activeTime +
                            static_cast<std::uint64_t>(program_->timers.data[a.target - 1].initial);
            next.generation = old.generation + 1;
            if (!select(cause, next.evidence))
                return false;
        } else {
            next.active = false;
            next.deadline = 0;
        }
        Id out = append(plan_->timers);
        if (!out) {
            fail(Error::OperatingProfile, true);
            return false;
        }
        plan_->timers.data[out - 1].target = a.target;
        plan_->timers.data[out - 1].value = next;
        return true;
    }
    if (produces(a.op)) {
        if (a.op == Op::Override && input_->kind != InputKind::Override) {
            fail(Error::Context);
            return false;
        }
        if (plan_->emissions.count == 8) {
            fail(Error::Budget);
            return false;
        }
        for (std::size_t i = 0; i < plan_->emissions.count; ++i)
            if (plan_->emissions.data[i].subject == a.target) {
                fail(Error::Conflict);
                return false;
            }
        for (std::size_t i = 0; i < state_->solved.count; ++i)
            if (state_->solved.data[i] == a.target) {
                fail(Error::InvalidSnapshot);
                return false;
            }
        if (state_->fence.lastIssued >=
            std::numeric_limits<std::uint64_t>::max() - plan_->emissions.count) {
            fail(Error::Overflow);
            return false;
        }
        Id out = append(plan_->emissions);
        if (!out) {
            fail(Error::OperatingProfile, true);
            return false;
        }
        auto &emission = plan_->emissions.data[out - 1];
        emission.subject = a.target;
        emission.rule = rule.stable;
        emission.generation = rule.ir.generation;
        emission.type = a.op == Op::Override ? FactType::LocalOverride : FactType::TaskSolved;
        emission.candidate.event = program_->context.event;
        emission.candidate.origin = program_->context.box;
        emission.candidate.epoch = program_->context.sourceEpoch;
        emission.candidate.sequence = state_->fence.lastIssued + plan_->emissions.count;
        if (!select(cause, emission.dependencies, true))
            return false;
        for (std::size_t i = 0; i < emission.dependencies.refs.count; ++i)
            if (equal(emission.candidate, emission.dependencies.refs.data[i].id)) {
                fail(Error::Conflict);
                return false;
            }
        return true;
    }
    if (a.op == Op::Effect) {
        if (plan_->intents.count == 8) {
            fail(Error::Budget);
            return false;
        }
        const auto &exclusive = program_->ports.data[a.target - 1].exclusive;
        for (std::size_t i = 0; i < plan_->intents.count; ++i)
            if (plan_->intents.data[i].port == a.target ||
                equal(plan_->intents.data[i].exclusive, exclusive)) {
                fail(Error::Conflict);
                return false;
            }
        Id out = append(plan_->intents);
        if (!out) {
            fail(Error::OperatingProfile, true);
            return false;
        }
        auto &intent = plan_->intents.data[out - 1];
        intent.port = a.target;
        intent.resource = a.resource;
        intent.rule = rule.stable;
        intent.exclusive = exclusive;
        return select(cause, intent.evidence);
    }
    fail(Error::InvalidProgram);
    return false;
}
namespace {
bool sameOperation(const Input &a, const Input &b) {
    if (a.kind != b.kind)
        return false;
    if (a.kind == InputKind::Local)
        return a.bootGeneration == b.bootGeneration && a.sequence == b.sequence;
    if (a.kind == InputKind::Timer)
        return a.timer == b.timer && a.armGeneration == b.armGeneration;
    if (a.kind == InputKind::Override)
        return equal(a.overrideAdmission.record().operation,
                     b.overrideAdmission.record().operation);
    return equal(a.fact.ref.id, b.fact.ref.id);
}
bool sameInput(const Input &a, const Input &b) {
    if (!equal(a.context, b.context) || !sameOperation(a, b))
        return false;
    if (a.kind == InputKind::Local)
        return a.port == b.port && a.type == b.type && a.known == b.known &&
               (!a.known || a.value == b.value);
    if (a.kind == InputKind::Fact)
        return a.fact.subject == b.fact.subject && a.fact.kind == b.fact.kind &&
               equal(a.fact.ref, b.fact.ref);
    if (a.kind == InputKind::Override)
        return sameOverrideRecord(a.overrideAdmission.record(), b.overrideAdmission.record());
    return true;
}
bool sameNamespace(const Context &a, const Context &b) {
    return equal(a.event, b.event) && equal(a.run, b.run) && equal(a.station, b.station);
}
} // namespace
Result Planner::plan(const Program &program, const State &state, const Input &input,
                     const Capacities &capacity, Plan &destination, PlannerWorkspace &workspace) {
    return run(program, state, input, capacity, destination, workspace, false);
}
ReplayResult Planner::replayHistorical(const Program &program, const State &state,
                                       const Input &storedInput, const OverrideRecord *original,
                                       const Capacities &capacity, Plan &computed,
                                       PlannerWorkspace &workspace, StateStorage &inactive,
                                       State &next) {
    const Input copied = storedInput;
    OverrideRecord record;
    if (original)
        record = *original;
    computed.ready = false;
    ReplayResult out;
    Input input = copied;
    if (input.kind == InputKind::Override) {
        if (!original || !nonzero(record.operation) ||
            !sameContext(record.context, input.context)) {
            out.result.error = Error::Context;
            return out;
        }
        input.overrideAdmission = OverrideAdmission(record, OverrideAdmissionScope::Historical);
    } else if (original) {
        out.result.error = Error::Context;
        return out;
    }
    out.result = run(program, state, input, capacity, computed, workspace, true);
    if (out.result.decision == Decision::Ready) {
        out.result = commitRam(program, state, computed, inactive, next);
        if (out.result.decision == Decision::Ready)
            out.scope = ReplayScope::ReplayedInactive;
    }
    computed.ready = false;
    return out;
}
Result Planner::run(const Program &program, const State &state, const Input &input,
                    const Capacities &capacity, Plan &destination, PlannerWorkspace &workspace,
                    bool historical) {
    program_ = &program;
    state_ = &state;
    input_ = &input;
    plan_ = &destination;
    work_ = &workspace;
    result_ = {};
    visits_ = proofWork_ = actions_ = 0;
    destination.ready = false;
    destination.values.count = destination.timers.count = destination.latches.count =
        destination.emissions.count = destination.intents.count = destination.evidence.count = 0;
    destination.admitsFact = false;
    destination.consumedTimer = 0;
    workspace.catalog.count = workspace.proofs.count = workspace.proofRefs.count = 0;
    for (auto &e : workspace.evaluations)
        e = {};
    auto finish = [&]() {
        const Result out = result_;
        destination.ready = out.decision == Decision::Ready;
        program_ = nullptr;
        state_ = nullptr;
        input_ = nullptr;
        plan_ = nullptr;
        work_ = nullptr;
        return out;
    };
    if (!program.ready || program.nodes.count > 512 || program.rules.count > 64 ||
        program.values.count > 32 || program.timers.count > 16) {
        fail(Error::InvalidProgram);
        return finish();
    }
    if (!equal(input.context, program.context)) {
        fail(Error::Context);
        return finish();
    }
    if (!safePlan(program, state, destination, workspace)) {
        fail(Error::InvalidSnapshot);
        return finish();
    }
    if (!capacity.factsAvailable || !capacity.ledgerAvailable || !capacity.resourcesAvailable) {
        fail(Error::Unavailable, true);
        return finish();
    }
    if (!snapshot())
        return finish();
    if (input.kind != InputKind::Override &&
        input.overrideAdmission.scope() != OverrideAdmissionScope::Empty) {
        fail(Error::Context);
        return finish();
    }
    if (input.kind == InputKind::Override &&
        (input.overrideAdmission.scope() !=
             (historical ? OverrideAdmissionScope::Historical : OverrideAdmissionScope::Live) ||
         !nonzero(input.overrideAdmission.record().operation) ||
         !equal(input.overrideAdmission.record().context, program.context))) {
        fail(Error::Context);
        return finish();
    }
    for (std::size_t i = 0; i < state.ledger.count; ++i) {
        const auto &r = state.ledger.data[i];
        if ((input.kind == InputKind::Fact || input.kind == InputKind::Override ||
             sameNamespace(r.context, state.context)) &&
            sameOperation(input, r.input)) {
            if (!equal(r.context, state.context) || !sameInput(input, r.input))
                fail(Error::Conflict);
            else
                result_.decision = Decision::Duplicate;
            return finish();
        }
    }
    if (state.mode != Mode::Running) {
        fail(state.mode == Mode::RecoveryRequired ? Error::RecoveryRequired : Error::Stopped);
        return finish();
    }
    if (state.fence.progress == std::numeric_limits<std::uint64_t>::max() ||
        state.fence.ledger == std::numeric_limits<std::uint64_t>::max()) {
        fail(Error::Overflow);
        return finish();
    }
    if (input.kind == InputKind::Local) {
        if (!input.bootGeneration || !input.sequence || !input.port ||
            input.port > program.ports.count) {
            fail(Error::Reference);
            return finish();
        }
        const auto &p = program.ports.data[input.port - 1];
        if (!p.input || p.type != input.type ||
            (input.type != Type::Boolean && input.type != Type::Integer) ||
            (input.known && input.type == Type::Boolean && input.value != 0 && input.value != 1)) {
            fail(Error::Type);
            return finish();
        }
    } else if (input.kind == InputKind::Timer) {
        if (!input.timer || input.timer > state.timers.count || !input.armGeneration) {
            fail(Error::Reference);
            return finish();
        }
        const auto &timer = state.timers.data[input.timer - 1];
        if (!timer.active || timer.generation != input.armGeneration ||
            timer.deadline > state.fence.activeTime) {
            result_.decision = Decision::StaleTimer;
            return finish();
        }
        destination.consumedTimer = input.timer;
    } else if (input.kind == InputKind::Fact) {
        if (!validFact(input.fact)) {
            fail(Error::Context);
            return finish();
        }
        if (equal(program.subjects.data[input.fact.subject - 1].binding.station,
                  program.context.station)) {
            bool solved = false, producer = false;
            for (std::size_t i = 0; i < state.solved.count; ++i)
                solved |= state.solved.data[i] == input.fact.subject;
            for (std::size_t i = 0; i < program.rules.count; ++i) {
                const auto &r = program.rules.data[i];
                for (unsigned j = 0; j < r.ir.actionCount; ++j) {
                    const auto &a = program.actions.data[r.ir.firstAction + j];
                    if (!produces(a.op) || a.target != input.fact.subject)
                        continue;
                    for (std::size_t k = 0; k < state.latches.count; ++k)
                        producer |= equal(state.latches.data[k].rule, r.stable);
                }
            }
            if (!solved || !producer) {
                fail(Error::InvalidSnapshot);
                return finish();
            }
        }
        admit(input.fact.ref);
        bool known = false;
        for (std::size_t i = 0; i < state.facts.count; ++i)
            known |= state.facts.data[i].subject == input.fact.subject;
        destination.admitsFact = !known;
        destination.fact = input.fact;
    } else if (input.kind == InputKind::Override) {
        const auto &a = input.overrideAdmission.record();
        if (program.language != 2 || !program.manualOverride) {
            fail(Error::Unsupported);
            return finish();
        }
        if (!a.original || a.original > program.rules.count || !a.replacement ||
            a.replacement > program.rules.count ||
            !overridePlanningFence(a.expected, state.fence) || !nonzero(a.ownerBoot) ||
            !nonzero(a.token) || !nonzero(a.prepareId) || !nonzero(a.confirmId) ||
            !nonzero(a.authority.peer) || !a.authority.principal || !a.authority.roleEpoch ||
            !a.authority.connectionToken || !a.authority.registration || !a.authority.lifecycle ||
            !a.authority.registryRevision || !a.authority.eventRevision ||
            a.confirmedAt < a.preparedAt || a.confirmedAt > a.expiresAt) {
            fail(Error::Context);
            return finish();
        }
        const auto &original = program.rules.data[a.original - 1];
        const auto &replacement = program.rules.data[a.replacement - 1];
        if (original.ir.fallback != a.replacement || original.ir.group != a.original ||
            replacement.ir.group != a.original || !equal(original.stable, a.rule) ||
            !equal(replacement.stable, a.rule) || original.ir.generation != a.originalGeneration ||
            replacement.ir.generation != a.replacementGeneration ||
            !configuration::module_ir::manualReleasePair(program.actions, original.ir,
                                                         replacement.ir)) {
            fail(Error::Context);
            return finish();
        }
    } else {
        fail(Error::Type);
        return finish();
    }
    if (result_.error != Error::None)
        return finish();
    destination.context = program.context;
    destination.expected = state.fence;
    destination.receipt.context = program.context;
    destination.receipt.input = input;
    std::uint32_t base = 0;
    bool baseReady = false;
    Id order[64];
    for (std::size_t i = 0; i < program.rules.count; ++i) {
        std::size_t j = i;
        while (j) {
            const auto &a = program.rules.data[i];
            const auto &b = program.rules.data[order[j - 1]];
            if (a.ir.priority > b.ir.priority ||
                (a.ir.priority == b.ir.priority &&
                 std::memcmp(a.stable.bytes, b.stable.bytes, 16) >= 0))
                break;
            order[j] = order[j - 1];
            --j;
        }
        order[j] = static_cast<Id>(i);
    }
    for (std::size_t ri = 0; ri < program.rules.count && result_.error == Error::None; ++ri) {
        const auto &r = program.rules.data[order[ri]];
        result_.rule = static_cast<Id>(order[ri] + 1);
        bool latched = false;
        for (std::size_t i = 0; i < state.latches.count; ++i)
            latched |= equal(state.latches.data[i].rule, r.stable);
        if (latched)
            continue;
        bool matches =
            r.ir.trigger == 2 && input.kind == InputKind::Timer && r.ir.timer == input.timer;
        matches |= r.ir.trigger == 4 && input.kind == InputKind::Override &&
                   input.overrideAdmission.record().replacement == order[ri] + 1;
        if ((r.ir.trigger == 1 && input.kind == InputKind::Local) ||
            (r.ir.trigger == 3 && input.kind == InputKind::Fact))
            for (unsigned j = 0; j < r.ir.triggerCount; ++j)
                matches |= program.triggers.data[r.ir.firstTrigger + j] ==
                           (input.kind == InputKind::Local ? input.port : input.fact.subject);
        if (!matches)
            continue;
        const Evaluation guard = evaluate(r.ir.guard);
        if (!guard.known || !guard.value)
            continue;
        if (!baseReady) {
            base = input.kind == InputKind::Timer
                       ? singleton(state.timers.data[input.timer - 1].evidence)
                       : singleton({});
            baseReady = true;
        }
        const auto cause = combine(base, proof(r.ir.guard), false);
        if (r.ir.once) {
            for (std::size_t i = 0; i < destination.latches.count; ++i)
                if (equal(destination.latches.data[i].rule, r.stable)) {
                    fail(Error::Conflict);
                    return finish();
                }
            Id latch = append(destination.latches);
            if (!latch) {
                fail(Error::OperatingProfile, true);
                break;
            }
            auto &l = destination.latches.data[latch - 1];
            l.rule = r.stable;
            l.generation = r.ir.generation;
            select(cause, l.evidence);
        }
        for (unsigned ai = 0; ai < r.ir.actionCount && result_.error == Error::None; ++ai)
            action(r, static_cast<Id>(r.ir.firstAction + ai + 1), cause);
    }
    if (result_.error == Error::None &&
        (state.ledger.count >= capacity.receipts ||
         state.facts.count + static_cast<unsigned>(destination.admitsFact) > capacity.facts ||
         state.latches.count + destination.latches.count > capacity.latches ||
         destination.emissions.count > capacity.outbox ||
         destination.intents.count > capacity.intents))
        fail(Error::Unavailable, true);
    if (result_.error == Error::None && destination.admitsFact &&
        state.fence.facts == std::numeric_limits<std::uint64_t>::max())
        fail(Error::Overflow);
    if (result_.error == Error::None) {
        result_.decision = Decision::Ready;
        result_.rule = result_.node = result_.action = 0;
    }
    return finish();
}
namespace {
struct Range {
    std::uintptr_t begin = 0, end = 0;
};
template <typename T> bool range(const T *data, std::size_t count, Range &out) {
    out = {};
    if (!count)
        return true;
    if (!data || count > std::numeric_limits<std::uintptr_t>::max() / sizeof(T))
        return false;
    out.begin = reinterpret_cast<std::uintptr_t>(data);
    const auto bytes = count * sizeof(T);
    if (out.begin > std::numeric_limits<std::uintptr_t>::max() - bytes)
        return false;
    out.end = out.begin + bytes;
    return true;
}
bool overlap(Range a, Range b) {
    return a.begin < a.end && b.begin < b.end && a.begin < b.end && b.begin < a.end;
}
template <typename T>
bool avoids(const Range *outputs, std::size_t count, const T *data, std::size_t size) {
    Range source;
    if (!range(data, size, source))
        return false;
    for (std::size_t i = 0; i < count; ++i)
        if (overlap(outputs[i], source))
            return false;
    return true;
}
template <typename T> bool avoids(const Range *out, std::size_t count, const Pool<T> &p) {
    return avoids(out, count, p.data, p.count);
}
template <typename T> bool avoids(const Range *out, std::size_t count, Span<T> p) {
    return avoids(out, count, p.data, p.count);
}
bool disjoint(const Range *out, std::size_t count, const Program &p, const State &s) {
    if (!valid(s.values) || !valid(s.timers) || !valid(s.facts) || !valid(s.latches) ||
        !valid(s.ledger) || !valid(s.solved))
        return false;
    for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (overlap(out[i], out[j]))
                return false;
    if (!avoids(out, count, p.strings) || !avoids(out, count, p.offsets) ||
        !avoids(out, count, p.nodes) || !avoids(out, count, p.actions) ||
        !avoids(out, count, p.rules) || !avoids(out, count, p.values) ||
        !avoids(out, count, p.timers) || !avoids(out, count, p.ports) ||
        !avoids(out, count, p.subjects) || !avoids(out, count, p.resources) ||
        !avoids(out, count, p.triggers) || !avoids(out, count, p.epochs) ||
        !avoids(out, count, p.allowedResources))
        return false;
    if (!avoids(out, count, s.values) || !avoids(out, count, s.timers) ||
        !avoids(out, count, s.facts) || !avoids(out, count, s.latches) ||
        !avoids(out, count, s.ledger) || !avoids(out, count, s.solved))
        return false;
    for (std::size_t i = 0; i < s.values.count; ++i)
        if (!avoids(out, count, s.values.data[i].evidence.refs))
            return false;
    for (std::size_t i = 0; i < s.timers.count; ++i)
        if (!avoids(out, count, s.timers.data[i].evidence.refs))
            return false;
    for (std::size_t i = 0; i < s.latches.count; ++i)
        if (!avoids(out, count, s.latches.data[i].evidence.refs))
            return false;
    return true;
}
template <typename T> bool room(const Pool<T> &p, std::size_t count) {
    return count <= 65534 && count <= p.capacity && (!count || p.data);
}
bool safePlan(const Program &p, const State &s, const Plan &plan, const PlannerWorkspace &work) {
    Range out[10];
    if (!range(plan.values.data, plan.values.capacity, out[0]) ||
        !range(plan.timers.data, plan.timers.capacity, out[1]) ||
        !range(plan.latches.data, plan.latches.capacity, out[2]) ||
        !range(plan.emissions.data, plan.emissions.capacity, out[3]) ||
        !range(plan.intents.data, plan.intents.capacity, out[4]) ||
        !range(plan.evidence.data, plan.evidence.capacity, out[5]) ||
        !range(work.catalog.data, work.catalog.capacity, out[6]) ||
        !range(work.proofs.data, work.proofs.capacity, out[7]) ||
        !range(work.proofRefs.data, work.proofRefs.capacity, out[8]) ||
        !range(work.evaluations, 512, out[9]))
        return false;
    return disjoint(out, 10, p, s);
}
template <typename T> Span<T> span(const Pool<T> &p) {
    Span<T> out;
    out.data = p.count ? p.data : nullptr;
    out.count = p.count;
    return out;
}
Result failure(Error error, bool deferred = false) {
    Result r;
    r.error = error;
    r.decision = deferred ? Decision::Deferred : Decision::Rejected;
    return r;
}
Result ready() {
    Result r;
    r.decision = Decision::Ready;
    return r;
}
Evidence copyEvidence(Evidence source, Pool<FactRef> &destination) {
    Evidence out;
    const auto start = destination.count;
    for (std::size_t i = 0; i < source.refs.count; ++i)
        destination.data[destination.count++] = source.refs.data[i];
    out.refs.data = source.refs.count ? destination.data + start : nullptr;
    out.refs.count = source.refs.count;
    return out;
}
bool addEvidence(std::size_t &total, Evidence source) {
    if (!valid(source.refs) || source.refs.count > std::numeric_limits<std::size_t>::max() - total)
        return false;
    total += source.refs.count;
    return true;
}
const Value &afterValue(const State &s, const Plan &p, std::size_t index) {
    for (std::size_t i = 0; i < p.values.count; ++i)
        if (p.values.data[i].target == index + 1)
            return p.values.data[i].value;
    return s.values.data[index];
}
Timer afterTimer(const State &s, const Plan &p, std::size_t index) {
    for (std::size_t i = 0; i < p.timers.count; ++i)
        if (p.timers.data[i].target == index + 1)
            return p.timers.data[i].value;
    Timer out = s.timers.data[index];
    if (p.consumedTimer == index + 1) {
        out.active = false;
        out.deadline = 0;
        out.evidence = {};
    }
    return out;
}
} // namespace
Result Planner::initialize(const Program &program, const Fence &fence, StateStorage &storage,
                           State &destination) {
    if (!program.ready || !room(storage.values, program.values.count) ||
        !room(storage.timers, program.timers.count))
        return failure(Error::OperatingProfile, true);
    Range outputs[8];
    if (!range(storage.values.data, storage.values.capacity, outputs[0]) ||
        !range(storage.timers.data, storage.timers.capacity, outputs[1]) ||
        !range(storage.facts.data, storage.facts.capacity, outputs[2]) ||
        !range(storage.latches.data, storage.latches.capacity, outputs[3]) ||
        !range(storage.ledger.data, storage.ledger.capacity, outputs[4]) ||
        !range(storage.solved.data, storage.solved.capacity, outputs[5]) ||
        !range(storage.evidence.data, storage.evidence.capacity, outputs[6]))
        return failure(Error::InvalidSnapshot);
    State empty;
    if (!disjoint(outputs, 7, program, empty))
        return failure(Error::InvalidSnapshot);
    storage.values.count = storage.timers.count = storage.facts.count = storage.latches.count =
        storage.ledger.count = storage.solved.count = storage.evidence.count = 0;
    for (std::size_t i = 0; i < program.values.count; ++i) {
        Id id = append(storage.values);
        auto &v = storage.values.data[id - 1];
        v.type = program.values.data[i].type;
        v.value = program.values.data[i].initial;
    }
    for (std::size_t i = 0; i < program.timers.count; ++i)
        append(storage.timers);
    State next;
    next.context = program.context;
    next.fence = fence;
    next.mode = Mode::Stopped;
    next.values = span(storage.values);
    next.timers = span(storage.timers);
    next.facts = span(storage.facts);
    next.latches = span(storage.latches);
    next.ledger = span(storage.ledger);
    next.solved = span(storage.solved);
    destination = next;
    return ready();
}
Result Planner::commitRam(const Program &program, const State &current, const Plan &plan,
                          StateStorage &staging, State &destination) {
    if (!program.ready || !plan.ready || !equal(current.context, program.context) ||
        !equal(plan.context, current.context) || !equal(plan.expected, current.fence) ||
        current.mode != Mode::Running)
        return failure(Error::Stale);
    if (!valid(current.values) || !valid(current.timers) || !valid(current.facts) ||
        !valid(current.latches) || !valid(current.ledger) || !valid(current.solved) ||
        !valid(plan.values) || !valid(plan.timers) || !valid(plan.latches) ||
        !valid(plan.emissions) || !valid(plan.intents) || plan.emissions.count > 8 ||
        plan.intents.count > 8 || current.values.count != program.values.count ||
        current.timers.count != program.timers.count)
        return failure(Error::InvalidSnapshot);
    if (current.fence.progress == std::numeric_limits<std::uint64_t>::max() ||
        current.fence.ledger == std::numeric_limits<std::uint64_t>::max() ||
        (plan.admitsFact && current.fence.facts == std::numeric_limits<std::uint64_t>::max()) ||
        current.fence.lastIssued > std::numeric_limits<std::uint64_t>::max() - plan.emissions.count)
        return failure(Error::Overflow);
    for (std::size_t i = 0; i < plan.values.count; ++i) {
        const auto &w = plan.values.data[i];
        if (!w.target || w.target > program.values.count ||
            w.value.type != program.values.data[w.target - 1].type)
            return failure(Error::InvalidSnapshot);
        for (std::size_t j = 0; j < i; ++j)
            if (w.target == plan.values.data[j].target)
                return failure(Error::Conflict);
    }
    for (std::size_t i = 0; i < plan.timers.count; ++i) {
        const auto &w = plan.timers.data[i];
        if (!w.target || w.target > program.timers.count)
            return failure(Error::InvalidSnapshot);
        for (std::size_t j = 0; j < i; ++j)
            if (w.target == plan.timers.data[j].target)
                return failure(Error::Conflict);
    }
    if (plan.consumedTimer > program.timers.count)
        return failure(Error::InvalidSnapshot);
    std::size_t evidence = 0;
    for (std::size_t i = 0; i < current.values.count; ++i)
        if (!addEvidence(evidence, afterValue(current, plan, i).evidence))
            return failure(Error::InvalidSnapshot);
    for (std::size_t i = 0; i < current.timers.count; ++i)
        if (!addEvidence(evidence, afterTimer(current, plan, i).evidence))
            return failure(Error::InvalidSnapshot);
    for (std::size_t i = 0; i < current.latches.count; ++i)
        if (!addEvidence(evidence, current.latches.data[i].evidence))
            return failure(Error::InvalidSnapshot);
    for (std::size_t i = 0; i < plan.latches.count; ++i)
        if (!addEvidence(evidence, plan.latches.data[i].evidence))
            return failure(Error::InvalidSnapshot);
    if (!room(staging.values, current.values.count) ||
        !room(staging.timers, current.timers.count) ||
        !room(staging.facts, current.facts.count + static_cast<unsigned>(plan.admitsFact)) ||
        !room(staging.latches, current.latches.count + plan.latches.count) ||
        !room(staging.ledger, current.ledger.count + 1) ||
        !room(staging.solved, current.solved.count + plan.emissions.count) ||
        !room(staging.evidence, evidence))
        return failure(Error::OperatingProfile, true);
    Range outputs[7];
    if (!range(staging.values.data, staging.values.capacity, outputs[0]) ||
        !range(staging.timers.data, staging.timers.capacity, outputs[1]) ||
        !range(staging.facts.data, staging.facts.capacity, outputs[2]) ||
        !range(staging.latches.data, staging.latches.capacity, outputs[3]) ||
        !range(staging.ledger.data, staging.ledger.capacity, outputs[4]) ||
        !range(staging.solved.data, staging.solved.capacity, outputs[5]) ||
        !range(staging.evidence.data, staging.evidence.capacity, outputs[6]) ||
        !disjoint(outputs, 7, program, current) || !avoids(outputs, 7, plan.values) ||
        !avoids(outputs, 7, plan.timers) || !avoids(outputs, 7, plan.latches) ||
        !avoids(outputs, 7, plan.emissions) || !avoids(outputs, 7, plan.intents) ||
        !avoids(outputs, 7, plan.evidence))
        return failure(Error::InvalidSnapshot);
    staging.values.count = staging.timers.count = staging.facts.count = staging.latches.count =
        staging.ledger.count = staging.solved.count = staging.evidence.count = 0;
    for (std::size_t i = 0; i < current.values.count; ++i) {
        const Id id = append(staging.values);
        auto &v = staging.values.data[id - 1];
        v = afterValue(current, plan, i);
        v.evidence = copyEvidence(v.evidence, staging.evidence);
    }
    for (std::size_t i = 0; i < current.timers.count; ++i) {
        const Id id = append(staging.timers);
        auto &t = staging.timers.data[id - 1];
        t = afterTimer(current, plan, i);
        t.evidence = copyEvidence(t.evidence, staging.evidence);
    }
    for (std::size_t i = 0; i < current.facts.count; ++i) {
        const Id id = append(staging.facts);
        staging.facts.data[id - 1] = current.facts.data[i];
    }
    if (plan.admitsFact) {
        const Id id = append(staging.facts);
        staging.facts.data[id - 1] = plan.fact;
    }
    for (std::size_t i = 0; i < current.latches.count + plan.latches.count; ++i) {
        const Id id = append(staging.latches);
        auto &l = staging.latches.data[id - 1];
        l = i < current.latches.count ? current.latches.data[i]
                                      : plan.latches.data[i - current.latches.count];
        l.evidence = copyEvidence(l.evidence, staging.evidence);
    }
    for (std::size_t i = 0; i < current.ledger.count; ++i) {
        const Id id = append(staging.ledger);
        staging.ledger.data[id - 1] = current.ledger.data[i];
    }
    const Id receipt = append(staging.ledger);
    staging.ledger.data[receipt - 1] = plan.receipt;
    for (std::size_t i = 0; i < current.solved.count; ++i) {
        const Id id = append(staging.solved);
        staging.solved.data[id - 1] = current.solved.data[i];
    }
    for (std::size_t i = 0; i < plan.emissions.count; ++i) {
        const Id id = append(staging.solved);
        staging.solved.data[id - 1] = plan.emissions.data[i].subject;
    }
    State next;
    next.context = current.context;
    next.fence = current.fence;
    ++next.fence.progress;
    ++next.fence.ledger;
    if (plan.admitsFact)
        ++next.fence.facts;
    next.fence.lastIssued += plan.emissions.count;
    next.mode = current.mode;
    next.values = span(staging.values);
    next.timers = span(staging.timers);
    next.facts = span(staging.facts);
    next.latches = span(staging.latches);
    next.ledger = span(staging.ledger);
    next.solved = span(staging.solved);
    destination = next;
    return ready();
}
Result Planner::due(const Program &program, const State &state, Pool<Due> &destination) {
    if (state.mode != Mode::Running)
        return failure(state.mode == Mode::RecoveryRequired ? Error::RecoveryRequired
                                                            : Error::Stopped);
    if (!program.ready || !valid(state.timers) || state.timers.count != program.timers.count ||
        state.timers.count > 16)
        return failure(Error::InvalidSnapshot);
    std::size_t count = 0;
    for (std::size_t i = 0; i < state.timers.count; ++i)
        if (state.timers.data[i].active && state.timers.data[i].deadline <= state.fence.activeTime)
            ++count;
    if (!room(destination, count))
        return failure(Error::OperatingProfile, true);
    Range output;
    if (!range(destination.data, destination.capacity, output) ||
        !disjoint(&output, 1, program, state))
        return failure(Error::InvalidSnapshot);
    destination.count = 0;
    for (std::size_t i = 0; i < state.timers.count; ++i) {
        const auto &t = state.timers.data[i];
        if (!t.active || t.deadline > state.fence.activeTime)
            continue;
        std::size_t at = destination.count;
        while (at) {
            const auto &previous = destination.data[at - 1];
            if (previous.deadline < t.deadline ||
                (previous.deadline == t.deadline &&
                 std::strcmp(name(program, program.timers.data[previous.timer - 1].name),
                             name(program, program.timers.data[i].name)) < 0))
                break;
            destination.data[at] = previous;
            --at;
        }
        Due d;
        d.timer = static_cast<Id>(i + 1);
        d.deadline = t.deadline;
        d.generation = t.generation;
        destination.data[at] = d;
        ++destination.count;
    }
    return ready();
}
Result Planner::advanceClock(State &state, std::uint64_t activeTime) {
    if (state.mode != Mode::Running)
        return failure(state.mode == Mode::RecoveryRequired ? Error::RecoveryRequired
                                                            : Error::Stopped);
    if (activeTime < state.fence.activeTime)
        return failure(Error::Overflow);
    state.fence.activeTime = activeTime;
    return ready();
}
Result Planner::control(State &state, Mode mode) {
    if (state.fence.progress == std::numeric_limits<std::uint64_t>::max())
        return failure(Error::Overflow);
    if (state.mode == Mode::RecoveryRequired && mode != Mode::RecoveryRequired)
        return failure(Error::RecoveryRequired);
    if (mode == Mode::Running && state.mode != Mode::Paused && state.mode != Mode::Stopped)
        return failure(Error::Stopped);
    state.mode = mode;
    ++state.fence.progress;
    return ready();
}
} // namespace rules
} // namespace execution
} // namespace larp
