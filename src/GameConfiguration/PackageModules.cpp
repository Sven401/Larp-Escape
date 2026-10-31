#include "GameConfiguration/PackageModules.h"
#include <cstdio>
#include <cstring>
#include <limits>

namespace larp {
namespace configuration {
using namespace module_ir;
namespace {
bool positive(const Pool<Node> &nodes, Id index);
const char *const contentFields[] = {"format",    "label",    "modules",  "packageId", "requires",
                                     "resources", "revision", "stations", "subjects"};
const char *const moduleFields[] = {"constraints", "format",  "inputs",     "instances", "label",
                                    "moduleId",    "outputs", "parameters", "resources", "rules",
                                    "timers",      "values",  "version"};
const char *const stationFields[] = {"id",    "instances", "label", "ports",
                                     "rules", "timers",    "values"};
const char *const pinFields[] = {"digest", "moduleId", "version"};
const char *const parameterFields[] = {"default", "description", "id",   "label",
                                       "max",     "min",         "type", "unit"};
const char *const valueFields[] = {"id", "initial", "type"};
const char *const timerFields[] = {"durationMs", "id"};
const char *const portFields[] = {"capability", "direction", "id", "type"};
const char *const interfaceFields[] = {"description", "id", "label", "type"};
const char *const resourceFields[] = {"bytes", "digest", "id", "kind"};
const char *const slotFields[] = {"id", "kind"};
const char *const subjectFields[] = {"id", "originStation"};
const char *const ruleFields[] = {"actions", "generation", "guard",    "id",
                                  "mode",    "offline",    "priority", "trigger"};
const char *const triggerFields[] = {"kind", "ports", "subjects", "timer"};
const char *const offlineFields[] = {"policy", "rule"};
const char *const actionFields[] = {"expr", "id", "op", "port", "resource", "subject", "timer"};
const char *const expressionFields[] = {"arg", "args",  "field",   "id",   "left",
                                        "op",  "right", "subject", "type", "value"};
const char *const instanceFields[] = {"digest",  "id",         "inputs",    "moduleId",
                                      "outputs", "parameters", "resources", "version"};
const char *const literalFields[] = {"id", "op", "type", "value"};
const char *const initialFields[] = {"parameter"};
std::uint32_t field(const char *key, const char *const *keys, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        if (!std::strcmp(key, keys[i]))
            return 1u << i;
    return 0;
}
const char *const opNames[] = {"",     "literal", "parameter", "value", "input", "port_is",
                               "fact", "not",     "all",       "any",   "eq",    "lt",
                               "le",   "gt",      "ge",        "add",   "sub",   "set",
                               "arm",  "cancel",  "emit",      "effect"};
const char *typeName(Type t) {
    switch (t) {
    case Type::Boolean:
        return "bool";
    case Type::Integer:
        return "int32";
    case Type::Light:
        return "light";
    case Type::Audio:
        return "audio";
    case Type::Solved:
        return "solved";
    default:
        return "";
    }
}
class HashSource : public IPackageByteSource {
  public:
    HashSource(IPackageByteSource &source, IPackageDigest &digest)
        : source_(source), digest_(digest) {
        failed_ = !digest_.begin();
    }
    PackageReadStatus readByte(std::uint8_t &byte) override {
        const PackageReadStatus s = source_.readByte(byte);
        if (s == PackageReadStatus::Byte && !failed_ && !digest_.update(&byte, 1))
            failed_ = true;
        return s;
    }
    bool finish(std::uint8_t output[32]) {
        std::memset(output, 0, 32);
        std::size_t count = 0;
        const bool ok = !failed_ && digest_.finish(output, count) && count == 32;
        const bool clean = digest_.abort();
        if (!ok || !clean)
            std::memset(output, 0, 32);
        return ok && clean;
    }
    bool abort() {
        return digest_.abort();
    }

  private:
    IPackageByteSource &source_;
    IPackageDigest &digest_;
    bool failed_ = false;
};
bool samePin(const ModulePin &a, const ModulePin &b) {
    return !std::strcmp(a.id, b.id) && !std::strcmp(a.version, b.version) &&
           !std::strcmp(a.digest, b.digest);
}
bool semver(const char *text) {
    const char *p = text;
    for (unsigned i = 0; i < 3; ++i) {
        const char *start = p;
        unsigned n = 0;
        while (*p >= '0' && *p <= '9') {
            if (p - start == 5)
                return false;
            n = n * 10 + static_cast<unsigned>(*p++ - '0');
        }
        if (p == start || n > 65535 || (p - start > 1 && *start == '0'))
            return false;
        if (i != 2) {
            if (*p++ != '.')
                return false;
        } else if (*p)
            return false;
    }
    return true;
}
bool hex(const char *s) {
    if (std::strlen(s) != 64)
        return false;
    for (; *s; ++s)
        if (!(*s >= '0' && *s <= '9') && !(*s >= 'a' && *s <= 'f'))
            return false;
    return true;
}
} // namespace
void PackageModuleCompiler::fail(ModuleError error) {
    if (result_.error != ModuleError::None)
        return;
    result_.error = error;
    result_.offset = offset_;
    if (deferred_) {
        result_.offset = location_.offset;
        result_.source = location_.source;
        const char *const roles[] = {
            "",       "rules", "rules",  "rules",   "constraints", "parameters", "values",
            "timers", "ports", "inputs", "outputs", "resources",   "subjects",   "instances"};
        const char *role =
            location_.role < sizeof(roles) / sizeof(*roles) ? roles[location_.role] : "";
        char prefix[48] = {};
        if (location_.station != 65535)
            std::snprintf(prefix, sizeof(prefix), "/stations/%u",
                          static_cast<unsigned>(location_.station));
        if (location_.role == 2)
            std::snprintf(result_.path, sizeof(result_.path), "%s/rules/%u/guard", prefix,
                          static_cast<unsigned>(location_.index));
        else if (location_.role == 3)
            std::snprintf(result_.path, sizeof(result_.path), "%s/rules/%u/actions/%u", prefix,
                          static_cast<unsigned>(location_.index),
                          static_cast<unsigned>(location_.subindex));
        else
            std::snprintf(result_.path, sizeof(result_.path), "%s/%s/%u", prefix, role,
                          static_cast<unsigned>(location_.index));
        return;
    }
    std::size_t used = 0;
    for (std::size_t i = 0; i < depth_; ++i) {
        const Frame &f = frames_[i];
        if (f.key[0]) {
            if (used + 1 < sizeof(result_.path))
                result_.path[used++] = '/';
            for (const char *p = f.key; *p; ++p) {
                const bool escaped = *p == '/' || *p == '~';
                if (used + (escaped ? 2 : 1) >= sizeof(result_.path))
                    break;
                if (escaped) {
                    result_.path[used++] = '~';
                    result_.path[used++] = *p == '/' ? '1' : '0';
                } else
                    result_.path[used++] = *p;
            }
        } else if (f.count) {
            const int n = std::snprintf(result_.path + used, sizeof(result_.path) - used, "/%u",
                                        static_cast<unsigned>(f.count - 1));
            if (n < 0 || static_cast<std::size_t>(n) >= sizeof(result_.path) - used)
                break;
            used += static_cast<std::size_t>(n);
        }
    }
    result_.path[used] = '\0';
}
Location PackageModuleCompiler::location(Kind kind) const {
    Location loc;
    loc.offset = static_cast<std::uint32_t>(offset_);
    loc.source = result_.source;
    if (!moduleDocument_ && currentDefinition_ && currentDefinition_ <= work_->definitions.count)
        loc.station = work_->definitions.data[currentDefinition_ - 1].location.station;
    if (kind == Kind::Station) {
        loc.station = frames_[depth_ - 2].count - 1;
        return loc;
    }
    if (kind == Kind::Rule) {
        loc.role = 1;
        loc.index = frames_[depth_ - 2].count - 1;
        return loc;
    }
    if (kind == Kind::Action) {
        loc = work_->rules.data[currentRule_ - 1].location;
        loc.role = 3;
        loc.subindex = work_->rules.data[currentRule_ - 1].actionCount - 1;
        loc.offset = static_cast<std::uint32_t>(offset_);
        return loc;
    }
    if (kind == Kind::Expression) {
        for (std::size_t i = depth_; i > 0; --i) {
            const Frame &f = frames_[i - 1];
            if (f.kind == Kind::Action) {
                loc = work_->actions.data[f.index - 1].location;
                loc.offset = static_cast<std::uint32_t>(offset_);
                return loc;
            }
            if (f.kind == Kind::Rule) {
                loc = work_->rules.data[f.index - 1].location;
                loc.role = 2;
                loc.offset = static_cast<std::uint32_t>(offset_);
                return loc;
            }
            if (f.kind == Kind::Constraints) {
                loc.role = 4;
                loc.index = f.count - 1;
                return loc;
            }
        }
    }
    switch (kind) {
    case Kind::Parameter:
        loc.role = 5;
        break;
    case Kind::Value:
        loc.role = 6;
        break;
    case Kind::Timer:
        loc.role = 7;
        break;
    case Kind::Port:
        loc.role = 8;
        break;
    case Kind::Input:
        loc.role = 9;
        break;
    case Kind::Output:
        loc.role = 10;
        break;
    case Kind::Resource:
        loc.role = 11;
        if (!moduleDocument_)
            loc.station = 65535;
        break;
    case Kind::Subject:
        loc.role = 12;
        loc.station = 65535;
        break;
    case Kind::Instance:
        loc.role = 13;
        break;
    case Kind::BindingLiteral:
        loc = work_->instances.data[currentInstance_ - 1].location;
        loc.offset = static_cast<std::uint32_t>(offset_);
        return loc;
    default:
        break;
    }
    if (depth_ > 1)
        loc.index = frames_[depth_ - 2].count - 1;
    return loc;
}
void PackageModuleCompiler::fields(std::uint32_t expected) {
    if (frames_[depth_ - 1].seen != expected)
        fail(ModuleError::FieldSet);
}
const char *PackageModuleCompiler::string(Id id) const {
    return id && id <= work_->stringOffsets.count
               ? work_->strings.data + work_->stringOffsets.data[id - 1]
               : "";
}
bool PackageModuleCompiler::equals(const char *s) const {
    return length_ < sizeof(text_) && std::strlen(s) == length_ && !std::memcmp(s, text_, length_);
}
bool PackageModuleCompiler::identifier() const {
    if (scalar_ != CanonicalJsonToken::String || length_ == 0 || length_ > 48)
        return false;
    for (std::size_t i = 0; i < length_; ++i)
        if (static_cast<unsigned char>(text_[i]) < 32 || static_cast<unsigned char>(text_[i]) > 127)
            return false;
    return true;
}
Id PackageModuleCompiler::intern() {
    if (scalar_ != CanonicalJsonToken::String || length_ >= sizeof(text_)) {
        fail(ModuleError::OperatingProfileLimit);
        return 0;
    }
    for (std::size_t i = 0; i < work_->stringOffsets.count; ++i)
        if (!std::strcmp(string(static_cast<Id>(i + 1)), text_))
            return static_cast<Id>(i + 1);
    if (!work_->strings.data || work_->strings.count + length_ + 1 > work_->strings.capacity) {
        fail(ModuleError::OperatingProfileLimit);
        return 0;
    }
    const Id id = append(work_->stringOffsets);
    if (!id)
        return 0;
    work_->stringOffsets.data[id - 1] = static_cast<std::uint32_t>(work_->strings.count);
    std::memcpy(work_->strings.data + work_->strings.count, text_, length_ + 1);
    work_->strings.count += length_ + 1;
    return id;
}
bool PackageModuleCompiler::integer(std::int64_t lo, std::int64_t hi, std::int64_t &value) const {
    if (scalar_ != CanonicalJsonToken::Integer || !length_ || length_ > 11)
        return false;
    value = 0;
    const bool negative = text_[0] == '-';
    for (std::size_t i = negative ? 1 : 0; i < length_; ++i)
        value = value * 10 + text_[i] - '0';
    if (negative)
        value = -value;
    return value >= lo && value <= hi;
}
Type PackageModuleCompiler::type() const {
    if (scalar_ != CanonicalJsonToken::String)
        return Type::Invalid;
    if (equals("bool") || equals("boolean_sample"))
        return Type::Boolean;
    if (equals("int32") || equals("int32_sample"))
        return Type::Integer;
    if (equals("light") || equals("light_preset"))
        return Type::Light;
    if (equals("audio"))
        return Type::Audio;
    if (equals("solved"))
        return Type::Solved;
    return Type::Invalid;
}
Op PackageModuleCompiler::op() const {
    if (scalar_ == CanonicalJsonToken::String)
        for (std::size_t i = 1; i < sizeof(opNames) / sizeof(*opNames); ++i)
            if (equals(opNames[i]))
                return static_cast<Op>(i);
    return Op::Invalid;
}
Literal PackageModuleCompiler::primitive() {
    Literal l;
    std::int64_t n = 0;
    if (scalar_ == CanonicalJsonToken::Boolean) {
        l.type = Type::Boolean;
        l.value = equals("true") ? 1 : 0;
    } else if (integer(-2147483647LL - 1, 2147483647, n)) {
        l.type = Type::Integer;
        l.value = static_cast<std::int32_t>(n);
    } else
        fail(ModuleError::Type);
    return l;
}
PackageModuleCompiler::Kind PackageModuleCompiler::child(bool array) {
    if (!depth_)
        return moduleDocument_ ? Kind::Module : Kind::Content;
    Frame &f = frames_[depth_ - 1];
    const char *k = f.key;
    if (f.kind == Kind::Skip)
        return Kind::Skip;
    if (array) {
        if (f.kind == Kind::Content) {
            if (!std::strcmp(k, "stations"))
                return Kind::Stations;
            if (!std::strcmp(k, "modules"))
                return Kind::Pins;
            if (!std::strcmp(k, "subjects"))
                return Kind::Subjects;
            if (!std::strcmp(k, "resources"))
                return Kind::Resources;
            if (!std::strcmp(k, "requires"))
                return Kind::Skip;
        }
        if (f.kind == Kind::Module || f.kind == Kind::Station) {
            if (!std::strcmp(k, "constraints"))
                return Kind::Constraints;
            if (!std::strcmp(k, "parameters"))
                return Kind::Parameters;
            if (!std::strcmp(k, "inputs"))
                return Kind::Inputs;
            if (!std::strcmp(k, "outputs"))
                return Kind::Outputs;
            if (!std::strcmp(k, "values"))
                return Kind::Values;
            if (!std::strcmp(k, "timers"))
                return Kind::Timers;
            if (!std::strcmp(k, "ports"))
                return Kind::Ports;
            if (!std::strcmp(k, "rules"))
                return Kind::Rules;
            if (!std::strcmp(k, "instances"))
                return Kind::Instances;
            if (!std::strcmp(k, "resources"))
                return Kind::Resources;
        }
        if (f.kind == Kind::Rule && !std::strcmp(k, "actions"))
            return Kind::Actions;
        if (f.kind == Kind::Trigger && (!std::strcmp(k, "ports") || !std::strcmp(k, "subjects")))
            return Kind::TriggerList;
        if (f.kind == Kind::Expression && !std::strcmp(k, "args"))
            return Kind::Arguments;
    } else {
        switch (f.kind) {
        case Kind::Stations:
            return Kind::Station;
        case Kind::Pins:
            return Kind::Pin;
        case Kind::Parameters:
            return Kind::Parameter;
        case Kind::Values:
            return Kind::Value;
        case Kind::Timers:
            return Kind::Timer;
        case Kind::Ports:
            return Kind::Port;
        case Kind::Inputs:
            return Kind::Input;
        case Kind::Outputs:
            return Kind::Output;
        case Kind::Resources:
            return Kind::Resource;
        case Kind::Subjects:
            return Kind::Subject;
        case Kind::Rules:
            return Kind::Rule;
        case Kind::Actions:
            return Kind::Action;
        case Kind::Instances:
            return Kind::Instance;
        case Kind::Arguments:
        case Kind::Constraints:
            return Kind::Expression;
        case Kind::ParameterMap:
            return Kind::BindingLiteral;
        default:
            break;
        }
        if (f.kind == Kind::Rule) {
            if (!std::strcmp(k, "guard"))
                return Kind::Expression;
            if (!std::strcmp(k, "trigger"))
                return Kind::Trigger;
            if (!std::strcmp(k, "offline"))
                return Kind::Offline;
        }
        if (f.kind == Kind::Action && !std::strcmp(k, "expr"))
            return Kind::Expression;
        if (f.kind == Kind::Expression &&
            (!std::strcmp(k, "arg") || !std::strcmp(k, "left") || !std::strcmp(k, "right")))
            return Kind::Expression;
        if ((f.kind == Kind::Value && !std::strcmp(k, "initial")) ||
            (f.kind == Kind::Timer && !std::strcmp(k, "durationMs")))
            return Kind::InitialParameter;
        if (f.kind == Kind::Instance) {
            if (!std::strcmp(k, "parameters"))
                return Kind::ParameterMap;
            if (!std::strcmp(k, "inputs"))
                return Kind::InputMap;
            if (!std::strcmp(k, "outputs"))
                return Kind::OutputMap;
            if (!std::strcmp(k, "resources"))
                return Kind::ResourceMap;
        }
    }
    fail(ModuleError::Type);
    return Kind::Skip;
}
void PackageModuleCompiler::token(CanonicalJsonToken t, std::size_t offset) {
    if (result_.error != ModuleError::None)
        return;
    offset_ = offset;
    if (t == CanonicalJsonToken::ObjectBegin || t == CanonicalJsonToken::ArrayBegin) {
        Frame f;
        f.kind = child(t == CanonicalJsonToken::ArrayBegin);
        if (result_.error != ModuleError::None)
            return;
        if (depth_ == 32) {
            fail(ModuleError::Budget);
            return;
        }
        if (f.kind == Kind::Module)
            f.index = currentDefinition_;
        if (f.kind == Kind::Station) {
            f.index = append(work_->definitions);
            if (!f.index)
                return;
            currentDefinition_ = f.index;
        }
        if (f.kind == Kind::Pin)
            pin_ = {};
        if (f.kind == Kind::Parameter || f.kind == Kind::Value || f.kind == Kind::Timer ||
            f.kind == Kind::Port || f.kind == Kind::Input || f.kind == Kind::Output ||
            f.kind == Kind::Resource || f.kind == Kind::Subject) {
            f.index = append(work_->symbols);
            if (!f.index)
                return;
            Symbol &s = work_->symbols.data[f.index - 1];
            s.owner = currentDefinition_;
            switch (f.kind) {
            case Kind::Parameter:
                s.kind = SymbolKind::Parameter;
                break;
            case Kind::Value:
                s.kind = SymbolKind::Value;
                break;
            case Kind::Timer:
                s.kind = SymbolKind::Timer;
                s.type = Type::Integer;
                break;
            case Kind::Port:
                s.kind = SymbolKind::Port;
                break;
            case Kind::Input:
                s.kind = SymbolKind::Input;
                s.input = true;
                break;
            case Kind::Output:
                s.kind = SymbolKind::Output;
                break;
            case Kind::Resource:
                s.kind = SymbolKind::Resource;
                s.owner = moduleDocument_ ? currentDefinition_ : 0;
                break;
            case Kind::Subject:
                s.kind = SymbolKind::Subject;
                s.type = Type::Solved;
                s.owner = 0;
                break;
            default:
                break;
            }
        }
        if (f.kind == Kind::Rule) {
            f.index = append(work_->rules);
            if (!f.index)
                return;
            currentRule_ = f.index;
            Rule &r = work_->rules.data[f.index - 1];
            r.owner = currentDefinition_;
            r.firstAction = static_cast<Id>(work_->actions.count);
            r.firstTrigger = static_cast<Id>(work_->triggers.count);
        }
        if (f.kind == Kind::Action) {
            f.index = append(work_->actions);
            if (!f.index)
                return;
            ++work_->rules.data[currentRule_ - 1].actionCount;
        }
        if (f.kind == Kind::Instance) {
            f.index = append(work_->instances);
            if (!f.index)
                return;
            currentInstance_ = f.index;
            work_->instances.data[f.index - 1].owner = currentDefinition_;
        }
        if (f.kind == Kind::Expression) {
            f.index = append(work_->nodes);
            if (!f.index)
                return;
            Frame &p = frames_[depth_ - 1];
            if (p.kind == Kind::Rule)
                work_->rules.data[p.index - 1].guard = f.index;
            else if (p.kind == Kind::Action)
                work_->actions.data[p.index - 1].expr = f.index;
            else if (p.kind == Kind::Constraints) {
                Id c = append(work_->constraints);
                if (!c)
                    return;
                work_->constraints.data[c - 1].owner = currentDefinition_;
                work_->constraints.data[c - 1].expression = f.index;
            } else {
                Frame &n = p.kind == Kind::Arguments ? frames_[depth_ - 2] : p;
                Node &node = work_->nodes.data[n.index - 1];
                if (node.count == 8) {
                    fail(ModuleError::Budget);
                    return;
                }
                node.children[node.count++] = f.index;
                f.expressionDepth = n.expressionDepth;
            }
            if (++f.expressionDepth > 8) {
                fail(ModuleError::Budget);
                return;
            }
        }
        if (f.kind == Kind::BindingLiteral) {
            f.index = append(work_->bindings);
            if (!f.index)
                return;
            Binding &b = work_->bindings.data[f.index - 1];
            b.instance = currentInstance_;
            b.kind = 0;
            const auto saved = scalar_;
            scalar_ = CanonicalJsonToken::String;
            length_ = std::strlen(frames_[depth_ - 1].key);
            std::strcpy(text_, frames_[depth_ - 1].key);
            b.from = intern();
            scalar_ = saved;
        }
        if (f.kind == Kind::InitialParameter)
            f.index = frames_[depth_ - 1].index;
        if (depth_)
            ++frames_[depth_ - 1].count;
        frames_[depth_++] = f;
        const Location loc = location(f.kind);
        if (f.kind == Kind::Station || f.kind == Kind::Module)
            work_->definitions.data[f.index - 1].location = loc;
        if (f.kind == Kind::Rule)
            work_->rules.data[f.index - 1].location = loc;
        if (f.kind == Kind::Action)
            work_->actions.data[f.index - 1].location = loc;
        if (f.kind == Kind::Expression)
            work_->nodes.data[f.index - 1].location = loc;
        if (f.kind == Kind::Parameter || f.kind == Kind::Value || f.kind == Kind::Timer ||
            f.kind == Kind::Port || f.kind == Kind::Input || f.kind == Kind::Output ||
            f.kind == Kind::Resource || f.kind == Kind::Subject)
            work_->symbols.data[f.index - 1].location = loc;
        if (f.kind == Kind::Instance)
            work_->instances.data[f.index - 1].location = loc;
        if (f.kind == Kind::BindingLiteral)
            work_->bindings.data[f.index - 1].location = loc;
        if (f.kind == Kind::Expression && depth_ > 1 &&
            frames_[depth_ - 2].kind == Kind::Constraints)
            work_->constraints.data[work_->constraints.count - 1].location = loc;
        return;
    }
    if (t == CanonicalJsonToken::ObjectEnd || t == CanonicalJsonToken::ArrayEnd) {
        if (!depth_) {
            fail(ModuleError::Type);
            return;
        }
        close();
        --depth_;
        return;
    }
    scalar_ = t;
    length_ = 0;
    text_[0] = '\0';
}
void PackageModuleCompiler::scalarByte(std::uint8_t b) {
    if (result_.error != ModuleError::None)
        return;
    if (length_ < sizeof(text_) - 1)
        text_[length_] = static_cast<char>(b);
    ++length_;
}
void PackageModuleCompiler::scalarEnd(std::size_t) {
    if (result_.error != ModuleError::None || !depth_)
        return;
    text_[length_ < sizeof(text_) ? length_ : sizeof(text_) - 1] = '\0';
    Frame &f = frames_[depth_ - 1];
    if (f.kind == Kind::Skip)
        return;
    if (scalar_ != CanonicalJsonToken::Key) {
        scalar();
        return;
    }
    if (length_ > 48) {
        fail(ModuleError::FieldSet);
        return;
    }
    std::strcpy(f.key, text_);
    const char *const *keys = nullptr;
    std::size_t count = 0;
    switch (f.kind) {
    case Kind::Content:
        keys = contentFields;
        count = 9;
        break;
    case Kind::Module:
        keys = moduleFields;
        count = 13;
        break;
    case Kind::Station:
        keys = stationFields;
        count = 7;
        break;
    case Kind::Pin:
        keys = pinFields;
        count = 3;
        break;
    case Kind::Parameter:
        keys = parameterFields;
        count = 8;
        break;
    case Kind::Value:
        keys = valueFields;
        count = 3;
        break;
    case Kind::Timer:
        keys = timerFields;
        count = 2;
        break;
    case Kind::Port:
        keys = portFields;
        count = 4;
        break;
    case Kind::Input:
    case Kind::Output:
        keys = interfaceFields;
        count = 4;
        break;
    case Kind::Resource:
        keys = moduleDocument_ ? slotFields : resourceFields;
        count = moduleDocument_ ? 2 : 4;
        break;
    case Kind::Subject:
        keys = subjectFields;
        count = 2;
        break;
    case Kind::Rule:
        keys = ruleFields;
        count = 8;
        break;
    case Kind::Trigger:
        keys = triggerFields;
        count = 4;
        break;
    case Kind::Offline:
        keys = offlineFields;
        count = 2;
        break;
    case Kind::Action:
        keys = actionFields;
        count = 7;
        break;
    case Kind::Expression:
        keys = expressionFields;
        count = 10;
        break;
    case Kind::Instance:
        keys = instanceFields;
        count = 8;
        break;
    case Kind::BindingLiteral:
        keys = literalFields;
        count = 4;
        break;
    case Kind::InitialParameter:
        keys = initialFields;
        count = 1;
        break;
    case Kind::ParameterMap:
    case Kind::InputMap:
    case Kind::OutputMap:
    case Kind::ResourceMap:
        return;
    default:
        fail(ModuleError::Type);
        return;
    }
    const std::uint32_t b = field(f.key, keys, count);
    if (!b)
        fail(ModuleError::FieldSet);
    f.seen |= b;
}
void PackageModuleCompiler::scalar() {
    Frame &f = frames_[depth_ - 1];
    const char *k = f.key;
    std::int64_t n = 0;
    if (f.kind == Kind::Content)
        return; // declaration reader owns the outer metadata
    if (f.kind == Kind::Module || f.kind == Kind::Station) {
        Definition &d = work_->definitions.data[f.index - 1];
        if (!std::strcmp(k, "id") || !std::strcmp(k, "moduleId")) {
            if (!identifier())
                fail(ModuleError::Value);
            d.id = intern();
        } else if (!std::strcmp(k, "version")) {
            if (scalar_ != CanonicalJsonToken::String || length_ > 17 || !semver(text_))
                fail(ModuleError::Value);
            d.version = intern();
        } else if (!std::strcmp(k, "format")) {
            if (scalar_ != CanonicalJsonToken::String || !equals("larp.module.v1"))
                fail(ModuleError::Value);
        } else if (!std::strcmp(k, "label")) {
            if (scalar_ != CanonicalJsonToken::String || !length_ || length_ > 96)
                fail(ModuleError::Range);
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::Pin) {
        if (scalar_ != CanonicalJsonToken::String) {
            fail(ModuleError::Type);
            return;
        }
        if (!std::strcmp(k, "moduleId")) {
            if (!identifier())
                fail(ModuleError::Value);
            else
                std::strcpy(pin_.id, text_);
        } else if (!std::strcmp(k, "version")) {
            if (length_ > 17 || !semver(text_))
                fail(ModuleError::Value);
            else
                std::strcpy(pin_.version, text_);
        } else {
            if (length_ > 64 || !hex(text_))
                fail(ModuleError::Value);
            else
                std::strcpy(pin_.digest, text_);
        }
        return;
    }
    if (f.kind == Kind::Parameter || f.kind == Kind::Value || f.kind == Kind::Timer ||
        f.kind == Kind::Port || f.kind == Kind::Input || f.kind == Kind::Output ||
        f.kind == Kind::Resource || f.kind == Kind::Subject) {
        Symbol &s = work_->symbols.data[f.index - 1];
        if (!std::strcmp(k, "id")) {
            if (!identifier()) {
                fail(ModuleError::Value);
                return;
            }
            Frame &list = frames_[depth_ - 2];
            if (list.previous[0] && std::strcmp(list.previous, text_) >= 0)
                fail(ModuleError::ListOrder);
            std::strcpy(list.previous, text_);
            s.id = intern();
        } else if (!std::strcmp(k, "type")) {
            s.type = type();
            if (s.type == Type::Invalid)
                fail(ModuleError::Type);
            if ((f.kind == Kind::Parameter || f.kind == Kind::Value) &&
                (!equals("bool") && !equals("int32")))
                fail(ModuleError::Type);
            if ((f.kind == Kind::Input || f.kind == Kind::Output) &&
                (!equals("boolean_sample") && !equals("int32_sample") && !equals("light") &&
                 !equals("audio") && !equals("solved")))
                fail(ModuleError::Type);
        } else if (!std::strcmp(k, "initial") || !std::strcmp(k, "default"))
            s.initial = primitive();
        else if (!std::strcmp(k, "durationMs")) {
            if (!integer(1, 86400000, n))
                fail(ModuleError::Range);
            s.initial.type = Type::Integer;
            s.initial.value = static_cast<std::int32_t>(n);
        } else if (!std::strcmp(k, "min") || !std::strcmp(k, "max")) {
            if (!integer(-2147483647LL - 1, 2147483647, n))
                fail(ModuleError::Range);
            if (!std::strcmp(k, "min"))
                s.minimum = static_cast<std::int32_t>(n);
            else
                s.maximum = static_cast<std::int32_t>(n);
        } else if (!std::strcmp(k, "unit")) {
            if (scalar_ != CanonicalJsonToken::String ||
                (!equals("none") && !equals("count") && !equals("ms")))
                fail(ModuleError::Value);
        } else if (!std::strcmp(k, "direction")) {
            if (!equals("input") && !equals("output"))
                fail(ModuleError::Value);
            s.input = equals("input");
        } else if (!std::strcmp(k, "capability")) {
            s.auxiliary = intern();
        } else if (!std::strcmp(k, "originStation")) {
            if (!identifier())
                fail(ModuleError::Value);
            s.auxiliary = intern();
        } else if (!std::strcmp(k, "kind")) {
            s.type = type();
            if (s.type != Type::Light && s.type != Type::Audio)
                fail(ModuleError::Type);
        } else if (!std::strcmp(k, "label")) {
            if (scalar_ != CanonicalJsonToken::String || !length_ || length_ > 96)
                fail(ModuleError::Range);
        } else if (!std::strcmp(k, "description")) {
            if (scalar_ != CanonicalJsonToken::String)
                fail(ModuleError::Type);
        } else if (!std::strcmp(k, "bytes") ||
                   !std::strcmp(k, "digest")) { /* checked by declarations */
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::Rule) {
        Rule &r = work_->rules.data[f.index - 1];
        if (!std::strcmp(k, "id")) {
            if (!identifier())
                fail(ModuleError::Value);
            else {
                Frame &list = frames_[depth_ - 2];
                if (list.previous[0] && std::strcmp(list.previous, text_) >= 0)
                    fail(ModuleError::ListOrder);
                std::strcpy(list.previous, text_);
            }
            r.id = intern();
        } else if (!std::strcmp(k, "generation")) {
            if (!integer(1, 4294967295LL, n))
                fail(ModuleError::Range);
            r.generation = static_cast<std::uint32_t>(n);
        } else if (!std::strcmp(k, "priority")) {
            if (!integer(0, 65535, n))
                fail(ModuleError::Range);
            r.priority = static_cast<std::uint16_t>(n);
        } else if (!std::strcmp(k, "mode")) {
            if (scalar_ != CanonicalJsonToken::String || (!equals("once") && !equals("each")))
                fail(ModuleError::Value);
            r.once = equals("once");
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::Trigger) {
        Rule &r = work_->rules.data[currentRule_ - 1];
        if (!std::strcmp(k, "kind")) {
            r.trigger = equals("local") ? 1 : equals("timer") ? 2 : equals("fact") ? 3 : 0;
            if (!r.trigger)
                fail(ModuleError::Value);
        } else if (!std::strcmp(k, "timer")) {
            if (!identifier())
                fail(ModuleError::Value);
            r.timer = intern();
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::TriggerList) {
        if (!identifier())
            fail(ModuleError::Value);
        Id t = append(work_->triggers);
        if (t)
            work_->triggers.data[t - 1] = intern();
        ++work_->rules.data[currentRule_ - 1].triggerCount;
        return;
    }
    if (f.kind == Kind::Offline) {
        Rule &r = work_->rules.data[currentRule_ - 1];
        if (!std::strcmp(k, "policy")) {
            r.policy = equals("wait")             ? 1
                       : equals("local_continue") ? 2
                       : equals("local_fallback") ? 3
                                                  : 0;
            if (!r.policy)
                fail(ModuleError::Value);
        } else if (!std::strcmp(k, "rule")) {
            if (!identifier())
                fail(ModuleError::Value);
            r.fallback = intern();
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::Action) {
        Action &a = work_->actions.data[f.index - 1];
        if (!std::strcmp(k, "op"))
            a.op = op();
        else if (!std::strcmp(k, "resource")) {
            if (!identifier())
                fail(ModuleError::Value);
            a.resource = intern();
        } else if (!std::strcmp(k, "id") || !std::strcmp(k, "port") || !std::strcmp(k, "subject") ||
                   !std::strcmp(k, "timer")) {
            if (!identifier())
                fail(ModuleError::Value);
            a.target = intern();
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::Expression) {
        Node &e = work_->nodes.data[f.index - 1];
        if (!std::strcmp(k, "op"))
            e.op = op();
        else if (!std::strcmp(k, "type")) {
            if (!equals("bool") && !equals("int32"))
                fail(ModuleError::Type);
            e.type = type();
        } else if (!std::strcmp(k, "value")) {
            Literal l = primitive();
            e.value = l.value;
            if (e.type != l.type)
                fail(ModuleError::Type);
        } else if (!std::strcmp(k, "field")) {
            if (!equals("value"))
                fail(ModuleError::Value);
        } else if (!std::strcmp(k, "id") || !std::strcmp(k, "subject")) {
            if (!identifier())
                fail(ModuleError::Value);
            e.ref = intern();
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::Instance) {
        Instance &i = work_->instances.data[f.index - 1];
        if (!std::strcmp(k, "id")) {
            if (!identifier())
                fail(ModuleError::Value);
            else {
                Frame &list = frames_[depth_ - 2];
                if (list.previous[0] && std::strcmp(list.previous, text_) >= 0)
                    fail(ModuleError::ListOrder);
                std::strcpy(list.previous, text_);
            }
            i.id = intern();
        } else if (!std::strcmp(k, "moduleId")) {
            if (!identifier())
                fail(ModuleError::Value);
            i.module = intern();
        } else if (!std::strcmp(k, "version")) {
            if (length_ > 17 || !semver(text_))
                fail(ModuleError::Value);
            i.version = intern();
        } else if (!std::strcmp(k, "digest")) {
            if (length_ > 64 || !hex(text_))
                fail(ModuleError::Value);
            i.digest = intern();
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::InputMap || f.kind == Kind::OutputMap || f.kind == Kind::ResourceMap) {
        if (!identifier()) {
            fail(ModuleError::Value);
            return;
        }
        const Id to = intern();
        const Id b = append(work_->bindings);
        if (!b)
            return;
        Binding &binding = work_->bindings.data[b - 1];
        binding.location = work_->instances.data[currentInstance_ - 1].location;
        binding.location.offset = static_cast<std::uint32_t>(offset_);
        binding.instance = currentInstance_;
        binding.kind = f.kind == Kind::InputMap ? 1 : f.kind == Kind::OutputMap ? 2 : 3;
        binding.to = to;
        const std::size_t savedLength = length_;
        char saved[49];
        std::strcpy(saved, text_);
        length_ = std::strlen(k);
        std::strcpy(text_, k);
        binding.from = intern();
        length_ = savedLength;
        std::strcpy(text_, saved);
        return;
    }
    if (f.kind == Kind::BindingLiteral) {
        Binding &b = work_->bindings.data[f.index - 1];
        if (!std::strcmp(k, "op")) {
            if (equals("literal"))
                f.expressionDepth = 1;
            else if (equals("parameter") && moduleDocument_)
                f.expressionDepth = 2;
            else
                fail(ModuleError::Value);
        } else if (!std::strcmp(k, "type")) {
            if (!equals("bool") && !equals("int32"))
                fail(ModuleError::Type);
            b.value.type = type();
        } else if (!std::strcmp(k, "value")) {
            Literal l = primitive();
            if (b.value.type != l.type)
                fail(ModuleError::Type);
            b.value.value = l.value;
        } else if (!std::strcmp(k, "id")) {
            if (!identifier())
                fail(ModuleError::Value);
            b.value.parameter = intern();
        } else
            fail(ModuleError::Type);
        return;
    }
    if (f.kind == Kind::InitialParameter) {
        if (!moduleDocument_) {
            fail(ModuleError::Value);
            return;
        }
        if (!identifier())
            fail(ModuleError::Value);
        work_->symbols.data[f.index - 1].initial.parameter = intern();
        return;
    }
    fail(ModuleError::Type);
}
void PackageModuleCompiler::close() {
    const Frame &f = frames_[depth_ - 1];
    switch (f.kind) {
    case Kind::Content:
        fields(511);
        break;
    case Kind::Module:
        fields(8191);
        break;
    case Kind::Station:
        fields(127);
        break;
    case Kind::Pin: {
        fields(7);
        if (result_.error != ModuleError::None)
            break;
        bool known = false;
        for (std::size_t i = 0; i < pinCount_; ++i)
            if (!std::strcmp(pins_[i].id, pin_.id) &&
                !std::strcmp(pins_[i].version, pin_.version)) {
                known = true;
                if (!samePin(pins_[i], pin_))
                    fail(ModuleError::ModuleConflict);
            }
        if (!known) {
            if (pinCount_ == 16)
                fail(ModuleError::Budget);
            else
                pins_[pinCount_++] = pin_;
        }
        break;
    }
    case Kind::Parameter: {
        const Symbol &s = work_->symbols.data[f.index - 1];
        fields(s.type == Type::Boolean ? 79 : 255);
        if (s.initial.type != s.type)
            fail(ModuleError::Type);
        if (s.type == Type::Integer &&
            (s.minimum > s.maximum || s.initial.value < s.minimum || s.initial.value > s.maximum))
            fail(ModuleError::Range);
        break;
    }
    case Kind::Value:
        fields(7);
        break;
    case Kind::Timer:
        fields(3);
        break;
    case Kind::Port:
        fields(15);
        break;
    case Kind::Input:
    case Kind::Output:
        fields(15);
        break;
    case Kind::Resource:
        fields(moduleDocument_ ? 3 : 15);
        break;
    case Kind::Subject:
        fields(3);
        break;
    case Kind::Rule:
        fields(255);
        if (work_->rules.data[f.index - 1].actionCount > 8)
            fail(ModuleError::Budget);
        break;
    case Kind::Trigger: {
        const Rule &r = work_->rules.data[currentRule_ - 1];
        fields(r.trigger == 1 ? 3 : r.trigger == 2 ? 9 : 5);
        break;
    }
    case Kind::Offline:
        fields(work_->rules.data[currentRule_ - 1].policy == 3 ? 3 : 1);
        break;
    case Kind::Action: {
        const Action &a = work_->actions.data[f.index - 1];
        switch (a.op) {
        case Op::Set:
        case Op::Add:
            fields(7);
            break;
        case Op::Arm:
        case Op::Cancel:
            fields(68);
            break;
        case Op::Emit:
            fields(36);
            break;
        case Op::Effect:
            fields(28);
            break;
        default:
            fail(ModuleError::Value);
            break;
        }
        break;
    }
    case Kind::Expression: {
        const Node &e = work_->nodes.data[f.index - 1];
        switch (e.op) {
        case Op::Literal:
            fields(800);
            break;
        case Op::Parameter:
            if (!moduleDocument_)
                fail(ModuleError::Value);
            fields(40);
            break;
        case Op::Value:
        case Op::PortIs:
            fields(40);
            break;
        case Op::Input:
            fields(36);
            break;
        case Op::Fact:
            fields(160);
            break;
        case Op::Not:
            fields(33);
            break;
        case Op::All:
        case Op::Any:
            fields(34);
            break;
        case Op::Eq:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge:
        case Op::Add:
        case Op::Sub:
            fields(112);
            break;
        default:
            fail(ModuleError::Value);
            break;
        }
        break;
    }
    case Kind::Arguments:
        if (work_->nodes.data[frames_[depth_ - 2].index - 1].count < 2)
            fail(ModuleError::Range);
        break;
    case Kind::Instance:
        fields(255);
        break;
    case Kind::BindingLiteral:
        fields(f.expressionDepth == 1 ? 14 : 3);
        break;
    case Kind::InitialParameter:
        fields(1);
        break;
    default:
        break;
    }
}
Id PackageModuleCompiler::findSymbol(Id owner, Id id, SymbolKind kind) const {
    for (std::size_t i = 0; i < work_->symbols.count; ++i) {
        const Symbol &s = work_->symbols.data[i];
        if (s.owner == owner && s.id == id && s.kind == kind)
            return static_cast<Id>(i + 1);
    }
    return 0;
}
Id PackageModuleCompiler::findPin(Id id, Id version, Id digest) {
    for (std::size_t i = 0; i < pinCount_; ++i)
        if (!std::strcmp(pins_[i].id, string(id)) &&
            !std::strcmp(pins_[i].version, string(version)) &&
            !std::strcmp(pins_[i].digest, string(digest)))
            return static_cast<Id>(i + 1);
    fail(ModuleError::MissingModule);
    return 0;
}
Type PackageModuleCompiler::expressionType(Id index, Id owner, bool constraint, const Rule *rule) {
    if (!index || index > work_->nodes.count) {
        fail(ModuleError::Reference);
        return Type::Invalid;
    }
    const Node &n = work_->nodes.data[index - 1];
    location_ = n.location;
    if (n.op == Op::Literal)
        return n.type;
    if (n.op == Op::Parameter) {
        Id s = findSymbol(owner, n.ref, SymbolKind::Parameter);
        if (!s)
            fail(ModuleError::Reference);
        return s ? work_->symbols.data[s - 1].type : Type::Invalid;
    }
    if (constraint &&
        (n.op == Op::Value || n.op == Op::Input || n.op == Op::PortIs || n.op == Op::Fact)) {
        fail(ModuleError::Type);
        return Type::Invalid;
    }
    if (n.op == Op::Value) {
        Id s = findSymbol(owner, n.ref, SymbolKind::Value);
        if (!s)
            fail(ModuleError::Reference);
        return s ? work_->symbols.data[s - 1].type : Type::Invalid;
    }
    if (n.op == Op::Input) {
        if (!rule || rule->trigger != 1 || !rule->triggerCount) {
            fail(ModuleError::Type);
            return Type::Invalid;
        }
        Id port = work_->triggers.data[rule->firstTrigger];
        Id s = findSymbol(owner, port, SymbolKind::Port);
        if (!s)
            s = findSymbol(owner, port, SymbolKind::Input);
        if (!s) {
            fail(ModuleError::Reference);
            return Type::Invalid;
        }
        return work_->symbols.data[s - 1].type;
    }
    if (n.op == Op::PortIs) {
        Id s = findSymbol(owner, n.ref, SymbolKind::Port);
        if (!s)
            s = findSymbol(owner, n.ref, SymbolKind::Input);
        if (!rule || rule->trigger != 1 || !s || !work_->symbols.data[s - 1].input ||
            (work_->symbols.data[s - 1].type != Type::Boolean &&
             work_->symbols.data[s - 1].type != Type::Integer))
            fail(ModuleError::Type);
        return Type::Boolean;
    }
    if (n.op == Op::Fact) {
        Id s = findSymbol(owner, n.ref, SymbolKind::Input);
        if (!s)
            s = findSymbol(owner, n.ref, SymbolKind::Output);
        if (!work_->definitions.data[owner - 1].module)
            s = findSymbol(0, n.ref, SymbolKind::Subject);
        if (!s || work_->symbols.data[s - 1].type != Type::Solved)
            fail(ModuleError::Reference);
        return Type::Boolean;
    }
    Type first = Type::Invalid;
    for (std::size_t i = 0; i < n.count; ++i) {
        Type t = expressionType(n.children[i], owner, constraint, rule);
        if (!i)
            first = t;
        if (n.op == Op::All || n.op == Op::Any || n.op == Op::Not) {
            if (t != Type::Boolean)
                fail(ModuleError::Type);
        } else if (n.op == Op::Eq) {
            if (t != first || (t != Type::Boolean && t != Type::Integer))
                fail(ModuleError::Type);
        } else if (t != Type::Integer)
            fail(ModuleError::Type);
    }
    if ((n.op == Op::Not && n.count != 1) ||
        (n.op != Op::Not && n.op != Op::All && n.op != Op::Any && n.count != 2))
        fail(ModuleError::FieldSet);
    return n.op == Op::Add || n.op == Op::Sub ? Type::Integer : Type::Boolean;
}
bool PackageModuleCompiler::validateDefinitions() {
    for (std::size_t i = 0; i < work_->symbols.count; ++i) {
        const Symbol &s = work_->symbols.data[i];
        location_ = s.location;
        for (std::size_t j = 0; j < i; ++j) {
            const Symbol &t = work_->symbols.data[j];
            if (s.owner == t.owner && s.id == t.id &&
                (s.kind == t.kind ||
                 ((s.kind == SymbolKind::Input || s.kind == SymbolKind::Output) &&
                  (t.kind == SymbolKind::Input || t.kind == SymbolKind::Output))))
                fail(ModuleError::Conflict);
        }
        if (s.initial.parameter) {
            Id p = findSymbol(s.owner, s.initial.parameter, SymbolKind::Parameter);
            if (!p)
                fail(ModuleError::Reference);
            else if (work_->symbols.data[p - 1].type != s.type)
                fail(ModuleError::Type);
        } else if ((s.kind == SymbolKind::Value || s.kind == SymbolKind::Parameter) &&
                   s.initial.type != s.type)
            fail(ModuleError::Type);
        if (s.kind == SymbolKind::Subject) {
            bool origin = false;
            for (std::size_t j = 0; j < work_->definitions.count; ++j)
                origin |= !work_->definitions.data[j].module &&
                          work_->definitions.data[j].id == s.auxiliary;
            if (!origin)
                fail(ModuleError::Reference);
        }
    }
    for (std::size_t i = 0; i < work_->constraints.count; ++i) {
        const Constraint &c = work_->constraints.data[i];
        location_ = c.location;
        if (expressionType(c.expression, c.owner, true) != Type::Boolean)
            fail(ModuleError::Type);
    }
    for (std::size_t i = 0; i < work_->rules.count; ++i) {
        const Rule &r = work_->rules.data[i];
        location_ = r.location;
        if (r.policy == 3) {
            fail(ModuleError::UnsupportedScope);
            continue;
        }
        if ((r.trigger != 3 && r.policy != 2) || (r.trigger == 3 && r.policy != 1 && r.policy != 2))
            fail(ModuleError::Value);
        if (r.trigger == 3 && !r.once)
            fail(ModuleError::Type);
        if (r.trigger == 3 && !positive(work_->nodes, r.guard))
            fail(ModuleError::Type);
        Type triggerType = Type::Invalid;
        if (r.trigger == 2 && !findSymbol(r.owner, r.timer, SymbolKind::Timer))
            fail(ModuleError::Reference);
        for (std::size_t j = 0; j < r.triggerCount; ++j) {
            Id id = work_->triggers.data[r.firstTrigger + j];
            Id s = 0;
            if (r.trigger == 3) {
                s = findSymbol(r.owner, id, SymbolKind::Input);
                if (!s)
                    s = findSymbol(r.owner, id, SymbolKind::Output);
                if (!work_->definitions.data[r.owner - 1].module)
                    s = findSymbol(0, id, SymbolKind::Subject);
                if (!s || work_->symbols.data[s - 1].type != Type::Solved)
                    fail(ModuleError::Reference);
            } else {
                s = findSymbol(r.owner, id, SymbolKind::Port);
                if (!s)
                    s = findSymbol(r.owner, id, SymbolKind::Input);
                if (!s) {
                    fail(ModuleError::Reference);
                    continue;
                }
                const Symbol &p = work_->symbols.data[s - 1];
                if (!p.input || (p.type != Type::Boolean && p.type != Type::Integer))
                    fail(ModuleError::Type);
                if (triggerType != Type::Invalid && triggerType != p.type)
                    fail(ModuleError::Type);
                triggerType = p.type;
            }
        }
        if (expressionType(r.guard, r.owner, false, &r) != Type::Boolean)
            fail(ModuleError::Type);
        for (std::size_t j = 0; j < r.actionCount; ++j) {
            const Action &a = work_->actions.data[r.firstAction + j];
            location_ = a.location;
            Id s = 0;
            if (a.op == Op::Set || a.op == Op::Add) {
                s = findSymbol(r.owner, a.target, SymbolKind::Value);
                if (!s)
                    fail(ModuleError::Reference);
                else {
                    Type t = expressionType(a.expr, r.owner, false, &r);
                    if (t != work_->symbols.data[s - 1].type ||
                        (a.op == Op::Add && t != Type::Integer))
                        fail(ModuleError::Type);
                }
            } else if (a.op == Op::Arm || a.op == Op::Cancel) {
                if (!findSymbol(r.owner, a.target, SymbolKind::Timer))
                    fail(ModuleError::Reference);
            } else if (a.op == Op::Emit) {
                s = findSymbol(r.owner, a.target, SymbolKind::Output);
                if (!work_->definitions.data[r.owner - 1].module)
                    s = findSymbol(0, a.target, SymbolKind::Subject);
                if (!s || work_->symbols.data[s - 1].type != Type::Solved)
                    fail(ModuleError::Reference);
            } else if (a.op == Op::Effect) {
                s = findSymbol(r.owner, a.target, SymbolKind::Output);
                if (!work_->definitions.data[r.owner - 1].module)
                    s = findSymbol(r.owner, a.target, SymbolKind::Port);
                Id resource = findSymbol(work_->definitions.data[r.owner - 1].module ? r.owner : 0,
                                         a.resource, SymbolKind::Resource);
                if (!s || !resource)
                    fail(ModuleError::Reference);
                else if (work_->symbols.data[s - 1].input ||
                         work_->symbols.data[s - 1].type !=
                             work_->symbols.data[resource - 1].type ||
                         (work_->symbols.data[s - 1].type != Type::Light &&
                          work_->symbols.data[s - 1].type != Type::Audio))
                    fail(ModuleError::Type);
            }
            if (r.trigger == 3 && a.op != Op::Emit && a.op != Op::Effect)
                fail(ModuleError::Type);
            for (std::size_t k = 0; k < j; ++k) {
                const Action &b = work_->actions.data[r.firstAction + k];
                const bool values =
                    (a.op == Op::Set || a.op == Op::Add) && (b.op == Op::Set || b.op == Op::Add);
                const bool timers = (a.op == Op::Arm || a.op == Op::Cancel) &&
                                    (b.op == Op::Arm || b.op == Op::Cancel);
                if (a.target == b.target &&
                    (values || timers ||
                     (a.op == b.op && (a.op == Op::Emit || a.op == Op::Effect))))
                    fail(ModuleError::Conflict);
            }
            for (std::size_t k = 0; k < i; ++k) {
                const Rule &other = work_->rules.data[k];
                if (other.owner != r.owner || (r.trigger != 3 && other.trigger != 3))
                    continue;
                for (std::size_t l = 0; l < other.actionCount; ++l) {
                    const Action &b = work_->actions.data[other.firstAction + l];
                    if (a.op == b.op && a.target == b.target &&
                        (a.op == Op::Emit || a.op == Op::Effect))
                        fail(ModuleError::Conflict);
                }
            }
        }
    }
    bool reachable[16][16] = {};
    for (std::size_t i = 0; i < work_->instances.count; ++i) {
        const Instance &instance = work_->instances.data[i];
        location_ = instance.location;
        const Id target = findPin(instance.module, instance.version, instance.digest);
        if (!target)
            continue;
        if (work_->definitions.data[instance.owner - 1].module) {
            const std::size_t from = instance.owner - moduleDefinition_;
            if (from < 16)
                reachable[from][target - 1] = true;
        }
    }
    for (std::size_t k = 0; k < pinCount_; ++k)
        for (std::size_t i = 0; i < pinCount_; ++i)
            for (std::size_t j = 0; j < pinCount_; ++j)
                reachable[i][j] |= reachable[i][k] && reachable[k][j];
    for (std::size_t i = 0; i < pinCount_; ++i)
        if (reachable[i][i])
            fail(ModuleError::Cycle);
    for (std::size_t i = 0; i < work_->instances.count; ++i)
        validateInstance(static_cast<Id>(i + 1));
    return result_.error == ModuleError::None;
}
bool PackageModuleCompiler::validateInstance(Id index) {
    const Instance &i = work_->instances.data[index - 1];
    location_ = i.location;
    const Id pin = findPin(i.module, i.version, i.digest);
    if (!pin)
        return false;
    const Id child = static_cast<Id>(moduleDefinition_ + pin - 1);
    for (std::size_t si = 0; si < work_->symbols.count; ++si) {
        const Symbol &s = work_->symbols.data[si];
        if (s.owner != child)
            continue;
        const unsigned kind = s.kind == SymbolKind::Parameter  ? 0
                              : s.kind == SymbolKind::Input    ? 1
                              : s.kind == SymbolKind::Output   ? 2
                              : s.kind == SymbolKind::Resource ? 3
                                                               : 4;
        if (kind == 4)
            continue;
        unsigned count = 0;
        for (std::size_t bi = 0; bi < work_->bindings.count; ++bi) {
            const Binding &b = work_->bindings.data[bi];
            if (b.instance != index || b.kind != kind || b.from != s.id)
                continue;
            ++count;
            if (kind == 0) {
                Type t = b.value.type;
                if (b.value.parameter) {
                    Id p = findSymbol(i.owner, b.value.parameter, SymbolKind::Parameter);
                    if (!p)
                        fail(ModuleError::Reference);
                    else
                        t = work_->symbols.data[p - 1].type;
                }
                if (t != s.type)
                    fail(ModuleError::Type);
                if (!b.value.parameter && s.type == Type::Integer &&
                    (b.value.value < s.minimum || b.value.value > s.maximum))
                    fail(ModuleError::Range);
            } else {
                Id target = 0;
                const bool parentModule = work_->definitions.data[i.owner - 1].module;
                if (kind == 3)
                    target = findSymbol(parentModule ? i.owner : 0, b.to, SymbolKind::Resource);
                else if (!parentModule)
                    target = s.type == Type::Solved ? findSymbol(0, b.to, SymbolKind::Subject)
                                                    : findSymbol(i.owner, b.to, SymbolKind::Port);
                else {
                    target = findSymbol(i.owner, b.to,
                                        kind == 2 ? SymbolKind::Output : SymbolKind::Input);
                    if (!target && kind == 1 && s.type == Type::Solved)
                        target = findSymbol(i.owner, b.to, SymbolKind::Output);
                }
                if (!target)
                    fail(ModuleError::Reference);
                else {
                    const Symbol &t = work_->symbols.data[target - 1];
                    if (t.type != s.type)
                        fail(ModuleError::Type);
                    if (t.kind == SymbolKind::Port && t.input != (kind == 1))
                        fail(ModuleError::Type);
                    if (kind == 2 && s.type == Type::Solved && !parentModule &&
                        t.auxiliary != work_->definitions.data[i.owner - 1].id)
                        fail(ModuleError::Reference);
                }
            }
        }
        if (count != 1)
            fail(ModuleError::Reference);
    }
    for (std::size_t bi = 0; bi < work_->bindings.count; ++bi) {
        const Binding &b = work_->bindings.data[bi];
        if (b.instance != index)
            continue;
        const SymbolKind kind = b.kind == 0   ? SymbolKind::Parameter
                                : b.kind == 1 ? SymbolKind::Input
                                : b.kind == 2 ? SymbolKind::Output
                                              : SymbolKind::Resource;
        if (!findSymbol(child, b.from, kind))
            fail(ModuleError::Reference);
    }
    return result_.error == ModuleError::None;
}
Literal PackageModuleCompiler::substitute(Literal value, const Context &context) {
    if (!value.parameter)
        return value;
    for (std::size_t i = 0; i < context.parameterCount; ++i)
        if (work_->symbols.data[context.parameterSymbols[i] - 1].id == value.parameter)
            return context.parameters[i];
    fail(ModuleError::Reference);
    return {};
}
Literal PackageModuleCompiler::evaluate(Id index, const Context &context) {
    const Node &n = work_->nodes.data[index - 1];
    location_ = n.location;
    Literal result;
    if (n.op == Op::Literal) {
        result.type = n.type;
        result.value = n.value;
        return result;
    }
    if (n.op == Op::Parameter) {
        result.parameter = n.ref;
        return substitute(result, context);
    }
    Literal args[8];
    for (std::size_t i = 0; i < n.count; ++i)
        args[i] = evaluate(n.children[i], context); // deliberately eager
    result.type = n.op == Op::Add || n.op == Op::Sub ? Type::Integer : Type::Boolean;
    const std::int64_t left = n.count ? args[0].value : 0, right = n.count > 1 ? args[1].value : 0;
    switch (n.op) {
    case Op::Not:
        result.value = !left;
        break;
    case Op::All:
        result.value = 1;
        for (std::size_t i = 0; i < n.count; ++i)
            result.value = result.value && args[i].value;
        break;
    case Op::Any:
        for (std::size_t i = 0; i < n.count; ++i)
            result.value = result.value || args[i].value;
        break;
    case Op::Eq:
        result.value = left == right;
        break;
    case Op::Lt:
        result.value = left < right;
        break;
    case Op::Le:
        result.value = left <= right;
        break;
    case Op::Gt:
        result.value = left > right;
        break;
    case Op::Ge:
        result.value = left >= right;
        break;
    case Op::Add:
    case Op::Sub: {
        const std::int64_t value = n.op == Op::Add ? left + right : left - right;
        if (value < -2147483647LL - 1 || value > 2147483647)
            fail(ModuleError::Range);
        else
            result.value = static_cast<std::int32_t>(value);
        break;
    }
    default:
        fail(ModuleError::Type);
        break;
    }
    return result;
}
Id PackageModuleCompiler::resolveInterface(Id id, const Context &context, Type required,
                                           bool output) {
    const bool module = work_->definitions.data[context.definition - 1].module;
    if (!module) {
        Id s = required == Type::Solved    ? findSymbol(0, id, SymbolKind::Subject)
               : required == Type::Invalid ? findSymbol(0, id, SymbolKind::Resource)
                                           : findSymbol(context.definition, id, SymbolKind::Port);
        if (!s) {
            fail(ModuleError::Reference);
            return 0;
        }
        const Symbol &symbol = work_->symbols.data[s - 1];
        if (required != Type::Invalid && symbol.type != required)
            fail(ModuleError::Type);
        if (symbol.kind == SymbolKind::Port && symbol.input == output)
            fail(ModuleError::Type);
        if (required == Type::Solved && output &&
            symbol.auxiliary != work_->definitions.data[context.definition - 1].id)
            fail(ModuleError::Reference);
        return id;
    }
    for (std::size_t i = 0; i < context.interfaceCount; ++i) {
        const Symbol &s = work_->symbols.data[context.interfaceSymbols[i] - 1];
        if (s.id != id)
            continue;
        if (required == Type::Invalid) {
            if (s.kind != SymbolKind::Resource)
                continue;
        } else {
            if (s.type != required)
                continue;
            if (output && s.kind != SymbolKind::Output)
                continue;
            if (!output && required != Type::Solved && s.kind != SymbolKind::Input)
                continue;
        }
        return context.interfaces[i];
    }
    fail(ModuleError::Reference);
    return 0;
}
namespace {
bool sameName(const Name &a, const Name &b) {
    if (a.count != b.count)
        return false;
    for (unsigned i = 0; i < a.count; ++i)
        if (a.segments[i] != b.segments[i])
            return false;
    return true;
}
Name qualified(Name path, Id local) {
    if (path.count < 5)
        path.segments[path.count++] = local;
    return path;
}
} // namespace
Id PackageModuleCompiler::copyExpression(Id index, Context &context) {
    if (expanded_->nodes.count == 512) {
        fail(ModuleError::Budget);
        return 0;
    }
    const Id out = append(expanded_->nodes);
    if (!out)
        return 0;
    const Node &source = work_->nodes.data[index - 1];
    Node &node = expanded_->nodes.data[out - 1];
    node = source;
    if (source.op == Op::Parameter) {
        Literal value;
        value.parameter = source.ref;
        value = substitute(value, context);
        node.op = Op::Literal;
        node.type = value.type;
        node.value = value.value;
        node.ref = 0;
    }
    if (source.op == Op::Value) {
        const Name target = qualified(context.path, source.ref);
        node.ref = 0;
        for (std::size_t i = 0; i < expanded_->values.count; ++i)
            if (sameName(target, expanded_->values.data[i].name))
                node.ref = static_cast<Id>(i + 1);
        if (!node.ref)
            fail(ModuleError::Reference);
    }
    if (source.op == Op::PortIs) {
        Id s = findSymbol(context.definition, source.ref, SymbolKind::Input);
        if (!s)
            s = findSymbol(context.definition, source.ref, SymbolKind::Port);
        if (s)
            node.ref =
                resolveInterface(source.ref, context, work_->symbols.data[s - 1].type, false);
        else
            fail(ModuleError::Reference);
    }
    if (source.op == Op::Fact)
        node.ref = resolveInterface(source.ref, context, Type::Solved, false);
    for (std::size_t i = 0; i < source.count; ++i)
        node.children[i] = copyExpression(source.children[i], context);
    return out;
}
Id PackageModuleCompiler::copyRule(const Rule &source, Context &context) {
    if (expanded_->rules.count == 64) {
        fail(ModuleError::Budget);
        return 0;
    }
    const Id out = append(expanded_->rules);
    if (!out)
        return 0;
    ExpandedRule &target = expanded_->rules.data[out - 1];
    target.name = qualified(context.path, source.id);
    target.rule = source;
    Rule &r = target.rule;
    r.guard = copyExpression(source.guard, context);
    r.firstTrigger = static_cast<Id>(expanded_->triggers.count);
    for (std::size_t i = 0; i < source.triggerCount; ++i) {
        Id id = work_->triggers.data[source.firstTrigger + i];
        Id t = append(expanded_->triggers);
        if (!t)
            return 0;
        if (source.trigger == 3)
            expanded_->triggers.data[t - 1] = resolveInterface(id, context, Type::Solved, false);
        else {
            Id s = findSymbol(context.definition, id, SymbolKind::Input);
            if (!s)
                s = findSymbol(context.definition, id, SymbolKind::Port);
            if (!s)
                fail(ModuleError::Reference);
            else
                expanded_->triggers.data[t - 1] =
                    resolveInterface(id, context, work_->symbols.data[s - 1].type, false);
        }
    }
    if (source.trigger == 2) {
        const Name wanted = qualified(context.path, source.timer);
        r.timer = 0;
        for (std::size_t i = 0; i < expanded_->timers.count; ++i)
            if (sameName(wanted, expanded_->timers.data[i].name))
                r.timer = static_cast<Id>(i + 1);
        if (!r.timer)
            fail(ModuleError::Reference);
    }
    r.firstAction = static_cast<Id>(expanded_->actions.count);
    for (std::size_t i = 0; i < source.actionCount; ++i) {
        Id a = append(expanded_->actions);
        if (!a)
            return 0;
        Action &action = expanded_->actions.data[a - 1];
        action = work_->actions.data[source.firstAction + i];
        if (action.expr)
            action.expr = copyExpression(action.expr, context);
        if (action.op == Op::Set || action.op == Op::Add || action.op == Op::Arm ||
            action.op == Op::Cancel) {
            const Name wanted = qualified(context.path, action.target);
            const auto &symbols = action.op == Op::Set || action.op == Op::Add ? expanded_->values
                                                                               : expanded_->timers;
            action.target = 0;
            for (std::size_t j = 0; j < symbols.count; ++j)
                if (sameName(wanted, symbols.data[j].name))
                    action.target = static_cast<Id>(j + 1);
            if (!action.target)
                fail(ModuleError::Reference);
        } else if (action.op == Op::Emit)
            action.target = resolveInterface(action.target, context, Type::Solved, true);
        else if (action.op == Op::Effect) {
            Id s = findSymbol(context.definition, action.target, SymbolKind::Output);
            if (!s)
                s = findSymbol(context.definition, action.target, SymbolKind::Port);
            if (!s)
                fail(ModuleError::Reference);
            else
                action.target =
                    resolveInterface(action.target, context, work_->symbols.data[s - 1].type, true);
            action.resource = resolveInterface(action.resource, context, Type::Invalid, false);
        }
    }
    return out;
}
bool PackageModuleCompiler::expandContext(Context &context) {
    for (std::size_t si = 0; si < work_->symbols.count; ++si) {
        const Symbol &s = work_->symbols.data[si];
        if (s.owner != context.definition ||
            (s.kind != SymbolKind::Value && s.kind != SymbolKind::Timer))
            continue;
        location_ = s.location;
        auto &pool = s.kind == SymbolKind::Value ? expanded_->values : expanded_->timers;
        const std::size_t limit = s.kind == SymbolKind::Value ? 32 : 16;
        if (pool.count == limit) {
            fail(ModuleError::Budget);
            return false;
        }
        Id target = append(pool);
        if (!target)
            return false;
        ExpandedSymbol &e = pool.data[target - 1];
        e.name = qualified(context.path, s.id);
        e.type = s.type;
        e.initial = substitute(s.initial, context);
        if (e.initial.type != s.type)
            fail(ModuleError::Type);
        if (s.kind == SymbolKind::Timer && (e.initial.value < 1 || e.initial.value > 86400000))
            fail(ModuleError::Range);
    }
    for (std::size_t ci = 0; ci < work_->constraints.count; ++ci) {
        const Constraint &c = work_->constraints.data[ci];
        if (c.owner != context.definition)
            continue;
        Literal value = evaluate(c.expression, context);
        if (value.type != Type::Boolean || !value.value)
            fail(ModuleError::Constraint);
    }
    for (std::size_t ri = 0; ri < work_->rules.count; ++ri)
        if (work_->rules.data[ri].owner == context.definition)
            copyRule(work_->rules.data[ri], context);
    for (std::size_t ii = 0; ii < work_->instances.count && result_.error == ModuleError::None;
         ++ii) {
        const Instance &i = work_->instances.data[ii];
        if (i.owner != context.definition)
            continue;
        location_ = i.location;
        if (contextDepth_ == 4 || occurrences_ == 32) {
            fail(ModuleError::Budget);
            return false;
        }
        ++occurrences_;
        const Id pin = findPin(i.module, i.version, i.digest);
        if (!pin)
            return false;
        Context &childContext = contexts_[++contextDepth_];
        childContext = {};
        childContext.definition = static_cast<Id>(moduleDefinition_ + pin - 1);
        childContext.path = qualified(context.path, i.id);
        for (std::size_t si = 0; si < work_->symbols.count; ++si) {
            const Symbol &s = work_->symbols.data[si];
            if (s.owner != childContext.definition)
                continue;
            const unsigned kind = s.kind == SymbolKind::Parameter  ? 0
                                  : s.kind == SymbolKind::Input    ? 1
                                  : s.kind == SymbolKind::Output   ? 2
                                  : s.kind == SymbolKind::Resource ? 3
                                                                   : 4;
            if (kind == 4)
                continue;
            for (std::size_t bi = 0; bi < work_->bindings.count; ++bi) {
                const Binding &b = work_->bindings.data[bi];
                if (b.instance != ii + 1 || b.kind != kind || b.from != s.id)
                    continue;
                if (kind == 0) {
                    if (childContext.parameterCount == 64) {
                        fail(ModuleError::OperatingProfileLimit);
                        break;
                    }
                    Literal value = substitute(b.value, context);
                    if (value.type != s.type)
                        fail(ModuleError::Type);
                    if (s.type == Type::Integer &&
                        (value.value < s.minimum || value.value > s.maximum))
                        fail(ModuleError::Range);
                    childContext.parameterSymbols[childContext.parameterCount] =
                        static_cast<Id>(si + 1);
                    childContext.parameters[childContext.parameterCount++] = value;
                } else {
                    if (childContext.interfaceCount == 128) {
                        fail(ModuleError::OperatingProfileLimit);
                        break;
                    }
                    childContext.interfaceSymbols[childContext.interfaceCount] =
                        static_cast<Id>(si + 1);
                    childContext.interfaces[childContext.interfaceCount++] = resolveInterface(
                        b.to, context, kind == 3 ? Type::Invalid : s.type, kind == 2);
                }
            }
        }
        if (result_.error == ModuleError::None)
            expandContext(childContext);
        --contextDepth_;
    }
    return result_.error == ModuleError::None;
}
bool PackageModuleCompiler::expand(Id stationId) {
    expanded_->nodes.count = expanded_->actions.count = expanded_->rules.count =
        expanded_->values.count = expanded_->timers.count = expanded_->triggers.count = 0;
    occurrences_ = 0;
    contextDepth_ = 0;
    contexts_[0] = {};
    contexts_[0].definition = stationId;
    if (!expandContext(contexts_[0]))
        return false;
    result_.instances = static_cast<std::uint16_t>(result_.instances + occurrences_);
    return validateExpanded(stationId);
}
namespace {
bool positive(const Pool<Node> &nodes, Id index) {
    const Node &n = nodes.data[index - 1];
    if (n.op != Op::Fact && n.op != Op::All && n.op != Op::Any)
        return false;
    for (std::size_t i = 0; i < n.count; ++i)
        if (!positive(nodes, n.children[i]))
            return false;
    return true;
}
} // namespace
bool PackageModuleCompiler::validateExpanded(Id stationId) {
    for (std::size_t i = 0; i < expanded_->rules.count; ++i) {
        const Rule &r = expanded_->rules.data[i].rule;
        if (r.trigger == 3 && !positive(expanded_->nodes, r.guard))
            fail(ModuleError::Type);
        for (std::size_t j = 0; j < r.actionCount; ++j) {
            const Action &a = expanded_->actions.data[r.firstAction + j];
            if (a.op == Op::Emit) {
                const Id subject = findSymbol(0, a.target, SymbolKind::Subject);
                if (!subject || work_->symbols.data[subject - 1].auxiliary !=
                                    work_->definitions.data[stationId - 1].id)
                    fail(ModuleError::Reference);
            }
            for (std::size_t k = 0; k < j; ++k) {
                const Action &b = expanded_->actions.data[r.firstAction + k];
                const bool values =
                    (a.op == Op::Set || a.op == Op::Add) && (b.op == Op::Set || b.op == Op::Add);
                const bool timers = (a.op == Op::Arm || a.op == Op::Cancel) &&
                                    (b.op == Op::Arm || b.op == Op::Cancel);
                if (a.target == b.target &&
                    (values || timers ||
                     (a.op == b.op && (a.op == Op::Emit || a.op == Op::Effect))))
                    fail(ModuleError::Conflict);
            }
            for (std::size_t k = 0; k < i; ++k) {
                const Rule &other = expanded_->rules.data[k].rule;
                if (r.trigger != 3 && other.trigger != 3)
                    continue;
                for (std::size_t l = 0; l < other.actionCount; ++l) {
                    const Action &b = expanded_->actions.data[other.firstAction + l];
                    if (a.op == b.op && a.target == b.target &&
                        (a.op == Op::Emit || a.op == Op::Effect))
                        fail(ModuleError::Conflict);
                }
            }
        }
    }
    return result_.error == ModuleError::None;
}
bool PackageModuleCompiler::write(const char *s) {
    if (result_.error != ModuleError::None)
        return false;
    if (!sink_->write(s, std::strlen(s))) {
        fail(ModuleError::SinkFailure);
        return false;
    }
    return true;
}
bool PackageModuleCompiler::quoted(const char *s) {
    if (!write("\""))
        return false;
    for (; *s; ++s) {
        char b[3] = {*s, 0, 0};
        if (*s == '"' || *s == '\\') {
            b[0] = '\\';
            b[1] = *s;
        }
        if (!write(b))
            return false;
    }
    return write("\"");
}
bool PackageModuleCompiler::number(std::int64_t n) {
    char text[24];
    std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(n));
    return write(text);
}
bool PackageModuleCompiler::name(const Name &n) {
    if (!write("\""))
        return false;
    for (std::size_t i = 0; i < n.count; ++i) {
        if (!write("/"))
            return false;
        for (const char *p = string(n.segments[i]); *p; ++p) {
            if (*p == '/')
                write("~1");
            else if (*p == '~')
                write("~0");
            else {
                char b[3] = {*p, 0, 0};
                if (*p == '"' || *p == '\\') {
                    b[0] = '\\';
                    b[1] = *p;
                }
                write(b);
            }
        }
    }
    return write("\"");
}
bool PackageModuleCompiler::emitLiteral(const Literal &l) {
    return l.type == Type::Boolean ? write(l.value ? "true" : "false") : number(l.value);
}
bool PackageModuleCompiler::emitExpression(Id index) {
    const Node &n = expanded_->nodes.data[index - 1];
    write("{");
    switch (n.op) {
    case Op::Literal:
        write("\"op\":\"literal\",\"type\":");
        quoted(typeName(n.type));
        write(",\"value\":");
        {
            Literal l;
            l.type = n.type;
            l.value = n.value;
            emitLiteral(l);
        }
        break;
    case Op::Value:
        write("\"id\":");
        name(expanded_->values.data[n.ref - 1].name);
        write(",\"op\":\"value\"");
        break;
    case Op::PortIs:
        write("\"id\":");
        quoted(string(n.ref));
        write(",\"op\":\"port_is\"");
        break;
    case Op::Input:
        write("\"field\":\"value\",\"op\":\"input\"");
        break;
    case Op::Fact:
        write("\"op\":\"fact\",\"subject\":");
        quoted(string(n.ref));
        break;
    case Op::Not:
        write("\"arg\":");
        emitExpression(n.children[0]);
        write(",\"op\":\"not\"");
        break;
    case Op::All:
    case Op::Any:
        write("\"args\":[");
        for (std::size_t i = 0; i < n.count; ++i) {
            if (i)
                write(",");
            emitExpression(n.children[i]);
        }
        write("],\"op\":");
        quoted(opNames[static_cast<unsigned>(n.op)]);
        break;
    default:
        write("\"left\":");
        emitExpression(n.children[0]);
        write(",\"op\":");
        quoted(opNames[static_cast<unsigned>(n.op)]);
        write(",\"right\":");
        emitExpression(n.children[1]);
        break;
    }
    return write("}");
}
bool PackageModuleCompiler::emitRule(const ExpandedRule &e) {
    const Rule &r = e.rule;
    write("{\"actions\":[");
    for (std::size_t i = 0; i < r.actionCount; ++i) {
        const Action &a = expanded_->actions.data[r.firstAction + i];
        if (i)
            write(",");
        write("{");
        if (a.op == Op::Set || a.op == Op::Add) {
            write("\"expr\":");
            emitExpression(a.expr);
            write(",\"id\":");
            name(expanded_->values.data[a.target - 1].name);
            write(",\"op\":");
            quoted(opNames[static_cast<unsigned>(a.op)]);
        } else if (a.op == Op::Arm || a.op == Op::Cancel) {
            write("\"op\":");
            quoted(opNames[static_cast<unsigned>(a.op)]);
            write(",\"timer\":");
            name(expanded_->timers.data[a.target - 1].name);
        } else if (a.op == Op::Emit) {
            write("\"op\":\"emit\",\"subject\":");
            quoted(string(a.target));
        } else {
            write("\"op\":\"effect\",\"port\":");
            quoted(string(a.target));
            write(",\"resource\":");
            quoted(string(a.resource));
        }
        write("}");
    }
    write("],\"generation\":");
    number(r.generation);
    write(",\"guard\":");
    emitExpression(r.guard);
    write(",\"id\":");
    name(e.name);
    write(",\"mode\":");
    quoted(r.once ? "once" : "each");
    write(",\"offline\":{\"policy\":");
    quoted(r.policy == 1 ? "wait" : "local_continue");
    write("},\"priority\":");
    number(r.priority);
    write(",\"trigger\":{\"kind\":");
    quoted(r.trigger == 1 ? "local" : r.trigger == 2 ? "timer" : "fact");
    if (r.trigger == 2) {
        write(",\"timer\":");
        name(expanded_->timers.data[r.timer - 1].name);
    } else {
        write(r.trigger == 1 ? ",\"ports\":[" : ",\"subjects\":[");
        for (std::size_t i = 0; i < r.triggerCount; ++i) {
            if (i)
                write(",");
            quoted(string(expanded_->triggers.data[r.firstTrigger + i]));
        }
        write("]");
    }
    return write("}}");
}
namespace {
void encodedName(const Name &name, const ModuleCompileWorkspace &w, char output[486]) {
    std::size_t used = 0;
    for (std::size_t i = 0; i < name.count; ++i) {
        output[used++] = '/';
        const char *s = w.strings.data + w.stringOffsets.data[name.segments[i] - 1];
        for (; *s; ++s) {
            if (*s == '/' || *s == '~') {
                output[used++] = '~';
                output[used++] = *s == '/' ? '1' : '0';
            } else
                output[used++] = *s;
        }
    }
    output[used] = '\0';
}
bool before(const Name &a, const Name &b, const ModuleCompileWorkspace &w) {
    char left[486], right[486];
    encodedName(a, w, left);
    encodedName(b, w, right);
    return std::strcmp(left, right) < 0;
}
template <typename T>
void order(const Pool<T> &pool, Id indices[64], const ModuleCompileWorkspace &w) {
    for (std::size_t i = 0; i < pool.count; ++i) {
        std::size_t j = i;
        while (j && before(pool.data[i].name, pool.data[indices[j - 1]].name, w)) {
            indices[j] = indices[j - 1];
            --j;
        }
        indices[j] = static_cast<Id>(i);
    }
}
} // namespace
bool PackageModuleCompiler::emitStation(Id stationId) {
    if (!sink_->beginStation(string(work_->definitions.data[stationId - 1].id))) {
        fail(ModuleError::SinkFailure);
        return false;
    }
    write("{\"format\":\"larp.rules.v1\",\"ports\":[");
    bool comma = false;
    for (std::size_t i = 0; i < work_->symbols.count; ++i) {
        const Symbol &s = work_->symbols.data[i];
        if (s.owner != stationId || s.kind != SymbolKind::Port)
            continue;
        if (comma)
            write(",");
        comma = true;
        write("{\"capability\":");
        quoted(string(s.auxiliary));
        write(",\"direction\":");
        quoted(s.input ? "input" : "output");
        write(",\"id\":");
        quoted(string(s.id));
        write(",\"type\":");
        quoted(s.type == Type::Boolean   ? "boolean_sample"
               : s.type == Type::Integer ? "int32_sample"
                                         : typeName(s.type));
        write("}");
    }
    write("],\"rules\":[");
    Id indices[64];
    order(expanded_->rules, indices, *work_);
    for (std::size_t i = 0; i < expanded_->rules.count; ++i) {
        if (i)
            write(",");
        emitRule(expanded_->rules.data[indices[i]]);
    }
    write("],\"station\":");
    quoted(string(work_->definitions.data[stationId - 1].id));
    write(",\"timers\":[");
    order(expanded_->timers, indices, *work_);
    for (std::size_t i = 0; i < expanded_->timers.count; ++i) {
        if (i)
            write(",");
        const ExpandedSymbol &s = expanded_->timers.data[indices[i]];
        write("{\"durationMs\":");
        number(s.initial.value);
        write(",\"id\":");
        name(s.name);
        write("}");
    }
    write("],\"values\":[");
    order(expanded_->values, indices, *work_);
    for (std::size_t i = 0; i < expanded_->values.count; ++i) {
        if (i)
            write(",");
        const ExpandedSymbol &s = expanded_->values.data[indices[i]];
        write("{\"id\":");
        name(s.name);
        write(",\"initial\":");
        emitLiteral(s.initial);
        write(",\"type\":");
        quoted(typeName(s.type));
        write("}");
    }
    write("]}");
    if (result_.error == ModuleError::None && !sink_->endStation())
        fail(ModuleError::SinkFailure);
    return result_.error == ModuleError::None;
}
ModuleCompileResult PackageModuleCompiler::compile(
    IPackageByteSource &content, IModuleSourceProvider &provider, IPackageDigest &digest,
    IExpandedPackageSink &sink, ModuleCompileWorkspace &workspace,
    ExpandedStationWorkspace &expanded, IExpandedStationConsumer *consumer) {
    work_ = &workspace;
    expanded_ = &expanded;
    sink_ = &sink;
    result_ = {};
    pinCount_ = depth_ = length_ = offset_ = 0;
    currentDefinition_ = moduleDefinition_ = currentRule_ = currentInstance_ = 0;
    moduleDocument_ = false;
    writing_ = false;
    deferred_ = false;
    work_->strings.count = work_->stringOffsets.count = work_->nodes.count = work_->symbols.count =
        work_->actions.count = work_->rules.count = work_->definitions.count =
            work_->instances.count = work_->bindings.count = work_->constraints.count =
                work_->triggers.count = 0;
    HashSource contentSource(content, digest);
    const DeclarationResult declarations = declarations_.validate(contentSource, this);
    result_.canonical = declarations.canonical;
    if (declarations.scope != DeclarationScope::DeclarationsOnly) {
        result_.error =
            declarations.canonical ? ModuleError::Declarations : ModuleError::CanonicalInput;
        result_.offset = declarations.offset;
        std::strcpy(result_.path, declarations.path);
        contentSource.abort();
    } else if (result_.error == ModuleError::None) {
        if (!contentSource.finish(result_.contentDigest))
            fail(ModuleError::CryptoFailure);
    } else
        contentSource.abort();
    moduleDefinition_ = static_cast<Id>(work_->definitions.count + 1);
    for (std::size_t i = 0; i < pinCount_ && result_.error == ModuleError::None; ++i) {
        result_.source = static_cast<std::uint16_t>(i + 1);
        depth_ = length_ = offset_ = 0;
        moduleDocument_ = true;
        currentRule_ = currentInstance_ = 0;
        currentDefinition_ = append(work_->definitions);
        if (!currentDefinition_)
            break;
        work_->definitions.data[currentDefinition_ - 1].module = true;
        IPackageByteSource *source = nullptr;
        if (!provider.open(pins_[i], source) || !source) {
            if (source)
                provider.close(*source);
            fail(ModuleError::MissingModule);
            break;
        }
        HashSource moduleSource(*source, digest);
        const CanonicalJsonResult canonical = canonical_.validate(moduleSource, this);
        result_.canonical = canonical;
        if (!canonical) {
            result_.error = ModuleError::CanonicalInput;
            result_.offset = canonical.offset;
            result_.path[0] = '\0';
            moduleSource.abort();
        } else if (result_.error == ModuleError::None) {
            const Definition &definition = work_->definitions.data[currentDefinition_ - 1];
            if (std::strcmp(string(definition.id), pins_[i].id) ||
                std::strcmp(string(definition.version), pins_[i].version))
                fail(ModuleError::ModuleConflict);
            std::uint8_t actual[32] = {};
            if (result_.error == ModuleError::None) {
                if (!moduleSource.finish(actual))
                    fail(ModuleError::CryptoFailure);
                else {
                    unsigned mismatch = 0;
                    for (std::size_t j = 0; j < 32; ++j) {
                        const char hi = pins_[i].digest[j * 2], lo = pins_[i].digest[j * 2 + 1];
                        const unsigned expected =
                            static_cast<unsigned>((hi <= '9' ? hi - '0' : hi - 'a' + 10) * 16 +
                                                  (lo <= '9' ? lo - '0' : lo - 'a' + 10));
                        mismatch |= actual[j] ^ expected;
                    }
                    if (mismatch)
                        fail(ModuleError::DigestMismatch);
                }
            } else
                moduleSource.abort();
        } else
            moduleSource.abort();
        provider.close(*source);
    }
    deferred_ = true;
    if (result_.error == ModuleError::None)
        validateDefinitions();
    if (result_.error == ModuleError::None) {
        result_.instances = 0;
        for (Id i = 1; i < moduleDefinition_ && result_.error == ModuleError::None; ++i)
            expand(i);
    }
    bool consumerStarted = false;
    if (result_.error == ModuleError::None && consumer) {
        consumerStarted = true;
        if (!consumer->begin(result_.contentDigest))
            fail(ModuleError::SinkFailure);
    }
    if (result_.error == ModuleError::None) {
        writing_ = true;
        if (!sink.begin())
            fail(ModuleError::SinkFailure);
        else {
            const std::uint16_t instances = result_.instances;
            result_.instances = 0;
            for (Id i = 1; i < moduleDefinition_ && result_.error == ModuleError::None; ++i)
                if (expand(i)) {
                    if (consumer && !consumer->station(workspace, expanded, i))
                        fail(ModuleError::SinkFailure);
                    if (result_.error == ModuleError::None)
                        emitStation(i);
                }
            result_.instances = instances;
            if (result_.error == ModuleError::None && consumer && !consumer->prepare())
                fail(ModuleError::SinkFailure);
            if (result_.error == ModuleError::None && !sink.commit())
                fail(ModuleError::SinkFailure);
            if (result_.error == ModuleError::None && consumer)
                consumer->commit();
        }
    }
    if (result_.error == ModuleError::None) {
        result_.scope = ModuleScope::ExpandedUnauthenticated;
        result_.offset = declarations.offset;
        result_.source = 0;
        result_.path[0] = '\0';
    } else {
        if (writing_)
            sink.abort();
        if (consumerStarted)
            consumer->abort();
        std::memset(result_.contentDigest, 0, sizeof(result_.contentDigest));
    }
    work_ = nullptr;
    expanded_ = nullptr;
    sink_ = nullptr;
    return result_;
}
} // namespace configuration
} // namespace larp
