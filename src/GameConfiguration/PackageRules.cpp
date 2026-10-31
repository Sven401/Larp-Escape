#include "GameConfiguration/PackageRules.h"
#include <cstdio>
#include <cstring>

namespace larp {
namespace configuration {
namespace {
const char *const expressionKeys[] = {"arg", "args",  "field",   "id",   "left",
                                      "op",  "right", "subject", "type", "value"};
const char *const actionKeys[] = {"expr", "id", "op", "port", "resource", "subject", "timer"};
const char *const ruleKeys[] = {"actions", "generation", "guard",    "id",
                                "mode",    "offline",    "priority", "trigger"};
const char *const triggerKeys[] = {"kind", "ports", "subjects", "timer"};
const char *const offlineKeys[] = {"policy", "rule"};
std::uint32_t bit(const char *key, const char *const *keys, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        if (std::strcmp(key, keys[i]) == 0)
            return 1u << i;
    return 0;
}
} // namespace
void PackageRuleReader::reset() {
    result_ = {};
    depth_ = length_ = offset_ = idCount_ = nodeCount_ = actionCount_ = ruleCount_ = 0;
    valueCount_ = timerCount_ = portCount_ = stationCount_ = resourceCount_ = 0;
    subjectCount_ = refCount_ = 0;
    unsupported_ = false;
    context_[0] = '\0';
    subjectDefinitions_ = 0;
}
void PackageRuleReader::path() {
    std::size_t used = 0;
    result_.path[0] = '\0';
    for (std::size_t i = 0; i < depth_; ++i) {
        const Frame &f = frames_[i];
        if (f.key[0]) {
            if (used + 1 < sizeof(result_.path))
                result_.path[used++] = '/';
            for (const char *p = f.key; *p; ++p) {
                const bool escape = *p == '/' || *p == '~';
                if (used + (escape ? 2 : 1) >= sizeof(result_.path))
                    break;
                if (escape) {
                    result_.path[used++] = '~';
                    result_.path[used++] = *p == '/' ? '1' : '0';
                } else
                    result_.path[used++] = *p;
            }
        } else if (f.count) {
            const int written = std::snprintf(result_.path + used, sizeof(result_.path) - used,
                                              "/%u", static_cast<unsigned>(f.count - 1));
            if (written < 0 || static_cast<std::size_t>(written) >= sizeof(result_.path) - used)
                break;
            used += static_cast<std::size_t>(written);
        }
    }
    result_.path[used] = '\0';
}
void PackageRuleReader::fail(RuleError error, std::size_t offset) {
    if (result_.error != RuleError::None)
        return;
    result_.error = error;
    result_.offset = offset;
    if (context_[0])
        std::strcpy(result_.path, context_);
    else
        path();
}
void PackageRuleReader::context(std::size_t stationIndex, std::size_t ruleIndex, const char *field,
                                std::size_t actionIndex) {
    if (actionIndex == 255)
        std::snprintf(context_, sizeof(context_), "/stations/%lu/rules/%lu/%s",
                      static_cast<unsigned long>(stationIndex),
                      static_cast<unsigned long>(ruleIndex), field);
    else
        std::snprintf(context_, sizeof(context_), "/stations/%lu/rules/%lu/actions/%lu",
                      static_cast<unsigned long>(stationIndex),
                      static_cast<unsigned long>(ruleIndex),
                      static_cast<unsigned long>(actionIndex));
}
bool PackageRuleReader::string() const {
    return scalar_ == CanonicalJsonToken::String;
}
bool PackageRuleReader::equals(const char *s) const {
    return length_ < sizeof(text_) && std::strlen(s) == length_ &&
           std::memcmp(s, text_, length_) == 0;
}
bool PackageRuleReader::identifier() const {
    if (!string() || length_ == 0 || length_ > 48)
        return false;
    for (std::size_t i = 0; i < length_; ++i)
        if (static_cast<unsigned char>(text_[i]) < 32 || static_cast<unsigned char>(text_[i]) > 127)
            return false;
    return true;
}
bool PackageRuleReader::integer(std::int64_t low, std::int64_t high) const {
    if (scalar_ != CanonicalJsonToken::Integer || length_ == 0 || length_ > 11)
        return false;
    const bool negative = text_[0] == '-';
    std::int64_t value = 0;
    for (std::size_t i = negative ? 1 : 0; i < length_; ++i)
        value = value * 10 + text_[i] - '0';
    if (negative)
        value = -value;
    return value >= low && value <= high;
}
PackageRuleReader::Type PackageRuleReader::type() const {
    if (!string())
        return Type::Invalid;
    if (equals("bool") || equals("boolean_sample"))
        return Type::Boolean;
    if (equals("int32") || equals("int32_sample"))
        return Type::Integer;
    if (equals("light") || equals("light_preset"))
        return Type::Light;
    if (equals("audio"))
        return Type::Audio;
    return Type::Invalid;
}
PackageRuleReader::Op PackageRuleReader::op() const {
    const char *const names[] = {"",    "literal", "value", "input", "port_is", "fact", "not",
                                 "all", "any",     "eq",    "lt",    "le",      "gt",   "ge",
                                 "add", "sub",     "set",   "arm",   "cancel",  "emit", "effect"};
    if (string())
        for (std::size_t i = 1; i < sizeof(names) / sizeof(*names); ++i)
            if (equals(names[i]))
                return static_cast<Op>(i);
    return Op::Invalid;
}
std::uint16_t PackageRuleReader::intern() {
    if (!identifier()) {
        fail(string() ? RuleError::Value : RuleError::Type, offset_);
        return 0;
    }
    for (std::size_t i = 0; i < idCount_; ++i)
        if (std::strcmp(ids_[i], text_) == 0)
            return static_cast<std::uint16_t>(i + 1);
    if (idCount_ == LocalIdentifiers) {
        fail(RuleError::OperatingProfileLimit, offset_);
        return 0;
    }
    std::strcpy(ids_[idCount_++], text_);
    return static_cast<std::uint16_t>(idCount_);
}
std::uint16_t PackageRuleReader::subject() {
    if (!identifier()) {
        fail(string() ? RuleError::Value : RuleError::Type, offset_);
        return 0;
    }
    for (std::size_t i = 0; i < subjectCount_; ++i)
        if (std::strcmp(subjects_[i].id, text_) == 0)
            return static_cast<std::uint16_t>(i + 1);
    if (subjectCount_ == Subjects) {
        fail(RuleError::OperatingProfileLimit, offset_);
        return 0;
    }
    subjects_[subjectCount_] = {};
    std::strcpy(subjects_[subjectCount_++].id, text_);
    return static_cast<std::uint16_t>(subjectCount_);
}
void PackageRuleReader::fields(std::uint32_t expected) {
    if (frames_[depth_ - 1].seen != expected)
        fail(RuleError::FieldSet, offset_);
}
PackageRuleReader::Kind PackageRuleReader::child(bool array) {
    if (!depth_)
        return Kind::Root;
    Frame &f = frames_[depth_ - 1];
    const char *key = f.key;
    if (f.kind == Kind::Skip)
        return Kind::Skip;
    if (f.kind == Kind::Modules || f.kind == Kind::Instances) {
        unsupported_ = true;
        return Kind::Skip;
    }
    if (f.kind == Kind::Root && array) {
        if (!std::strcmp(key, "stations"))
            return Kind::Stations;
        if (!std::strcmp(key, "modules"))
            return Kind::Modules;
        if (!std::strcmp(key, "subjects"))
            return Kind::Subjects;
        if (!std::strcmp(key, "resources"))
            return Kind::Resources;
        return Kind::Skip;
    }
    if (f.kind == Kind::Station && array) {
        if (!std::strcmp(key, "values"))
            return Kind::Values;
        if (!std::strcmp(key, "timers"))
            return Kind::Timers;
        if (!std::strcmp(key, "ports"))
            return Kind::Ports;
        if (!std::strcmp(key, "rules"))
            return Kind::Rules;
        if (!std::strcmp(key, "instances"))
            return Kind::Instances;
    }
    if (!array) {
        switch (f.kind) {
        case Kind::Stations:
            return Kind::Station;
        case Kind::Values:
            return Kind::Value;
        case Kind::Timers:
            return Kind::Timer;
        case Kind::Ports:
            return Kind::Port;
        case Kind::Rules:
            return Kind::Rule;
        case Kind::Actions:
            return Kind::Action;
        case Kind::Resources:
            return Kind::Resource;
        case Kind::Subjects:
            return Kind::Subject;
        case Kind::Arguments:
            return Kind::Expression;
        default:
            break;
        }
        if (f.kind == Kind::Rule) {
            if (!std::strcmp(key, "guard"))
                return Kind::Expression;
            if (!std::strcmp(key, "trigger"))
                return Kind::Trigger;
            if (!std::strcmp(key, "offline"))
                return Kind::Offline;
        }
        if (f.kind == Kind::Action && !std::strcmp(key, "expr"))
            return Kind::Expression;
        if (f.kind == Kind::Expression &&
            (!std::strcmp(key, "arg") || !std::strcmp(key, "left") || !std::strcmp(key, "right")))
            return Kind::Expression;
    } else {
        if (f.kind == Kind::Rule && !std::strcmp(key, "actions"))
            return Kind::Actions;
        if (f.kind == Kind::Expression && !std::strcmp(key, "args"))
            return Kind::Arguments;
        if (f.kind == Kind::Trigger &&
            (!std::strcmp(key, "ports") || !std::strcmp(key, "subjects")))
            return Kind::TriggerList;
    }
    if (f.kind == Kind::Root)
        return Kind::Skip;
    fail(RuleError::Type, offset_);
    return Kind::Skip;
}
void PackageRuleReader::token(CanonicalJsonToken tokenValue, std::size_t offset) {
    if (result_.error != RuleError::None)
        return;
    offset_ = offset;
    if (tokenValue == CanonicalJsonToken::ObjectBegin ||
        tokenValue == CanonicalJsonToken::ArrayBegin) {
        Kind kind = child(tokenValue == CanonicalJsonToken::ArrayBegin);
        if (result_.error != RuleError::None)
            return;
        if (depth_ == CanonicalPackageJsonReader::ContainerDepth) {
            fail(RuleError::Budget, offset);
            return;
        }
        Frame f;
        f.kind = kind;
        if (kind == Kind::Station) {
            if (stationCount_ == 8) {
                fail(RuleError::Budget, offset);
                return;
            }
            stationIds_[stationCount_++][0] = '\0';
            idCount_ = nodeCount_ = actionCount_ = ruleCount_ = 0;
            valueCount_ = timerCount_ = portCount_ = 0;
            previousRule_[0] = '\0';
        } else if (kind == Kind::Rule) {
            if (ruleCount_ == 64) {
                fail(RuleError::Budget, offset);
                return;
            }
            f.index = static_cast<std::uint16_t>(ruleCount_++);
            rules_[f.index] = {};
            rules_[f.index].firstAction = static_cast<std::uint16_t>(actionCount_);
            rules_[f.index].offset = offset;
        } else if (kind == Kind::Action) {
            if (rules_[ruleCount_ - 1].actionCount == 8 || actionCount_ == 512) {
                fail(RuleError::Budget, offset);
                return;
            }
            f.index = static_cast<std::uint16_t>(actionCount_++);
            actions_[f.index] = {};
            actions_[f.index].offset = offset;
            ++rules_[ruleCount_ - 1].actionCount;
        } else if (kind == Kind::Expression) {
            if (nodeCount_ == 512) {
                fail(RuleError::Budget, offset);
                return;
            }
            f.index = static_cast<std::uint16_t>(++nodeCount_);
            nodes_[f.index - 1] = {};
            nodes_[f.index - 1].offset = offset;
            Frame &parent = frames_[depth_ - 1];
            if (parent.kind == Kind::Rule)
                rules_[parent.index].guard = f.index;
            else if (parent.kind == Kind::Action)
                actions_[parent.index].expr = f.index;
            else {
                Frame &expr = parent.kind == Kind::Arguments ? frames_[depth_ - 2] : parent;
                Node &node = nodes_[expr.index - 1];
                if (node.count == 8) {
                    fail(RuleError::Budget, offset);
                    return;
                }
                node.children[node.count++] = f.index;
                nodes_[f.index - 1].depth = static_cast<std::uint8_t>(node.depth + 1);
                if (nodes_[f.index - 1].depth > 8) {
                    fail(RuleError::Budget, offset);
                    return;
                }
            }
        } else if (kind == Kind::Value || kind == Kind::Timer || kind == Kind::Port) {
            std::size_t &count = kind == Kind::Value   ? valueCount_
                                 : kind == Kind::Timer ? timerCount_
                                                       : portCount_;
            const std::size_t limit = kind == Kind::Value ? 32 : kind == Kind::Timer ? 16 : Ports;
            if (count == limit) {
                fail(kind == Kind::Port ? RuleError::OperatingProfileLimit : RuleError::Budget,
                     offset);
                return;
            }
            f.index = static_cast<std::uint16_t>(count++);
            Symbol *symbols = kind == Kind::Value   ? values_
                              : kind == Kind::Timer ? timers_
                                                    : ports_;
            symbols[f.index] = {};
        } else if (kind == Kind::Resource) {
            if (resourceCount_ == 32) {
                fail(RuleError::Budget, offset);
                return;
            }
            f.index = static_cast<std::uint16_t>(resourceCount_++);
            resourceIds_[f.index][0] = '\0';
            resourceTypes_[f.index] = Type::Invalid;
        }
        if (depth_)
            ++frames_[depth_ - 1].count;
        frames_[depth_++] = f;
        return;
    }
    if (tokenValue == CanonicalJsonToken::ObjectEnd || tokenValue == CanonicalJsonToken::ArrayEnd) {
        if (!depth_) {
            fail(RuleError::Type, offset);
            return;
        }
        close();
        --depth_;
        return;
    }
    scalar_ = tokenValue;
    length_ = 0;
    text_[0] = '\0';
}
void PackageRuleReader::scalarByte(std::uint8_t byte) {
    if (result_.error != RuleError::None)
        return;
    if (length_ < sizeof(text_) - 1)
        text_[length_] = static_cast<char>(byte);
    ++length_;
}
void PackageRuleReader::scalarEnd(std::size_t) {
    if (result_.error != RuleError::None || !depth_)
        return;
    text_[length_ < sizeof(text_) ? length_ : sizeof(text_) - 1] = '\0';
    Frame &f = frames_[depth_ - 1];
    if (f.kind == Kind::Skip)
        return;
    if (scalar_ == CanonicalJsonToken::Key) {
        if (length_ > 48) {
            fail(RuleError::FieldSet, offset_);
            return;
        }
        std::strcpy(f.key, text_);
        std::uint32_t mask = 1;
        switch (f.kind) {
        case Kind::Expression:
            mask = bit(f.key, expressionKeys, 10);
            break;
        case Kind::Action:
            mask = bit(f.key, actionKeys, 7);
            break;
        case Kind::Rule:
            mask = bit(f.key, ruleKeys, 8);
            break;
        case Kind::Trigger:
            mask = bit(f.key, triggerKeys, 4);
            break;
        case Kind::Offline:
            mask = bit(f.key, offlineKeys, 2);
            break;
        default:
            break;
        }
        if (!mask)
            fail(RuleError::FieldSet, offset_);
        f.seen |= mask;
    } else
        scalar();
}
void PackageRuleReader::scalar() {
    Frame &f = frames_[depth_ - 1];
    const char *key = f.key;
    if (f.kind == Kind::Modules || f.kind == Kind::Instances) {
        unsupported_ = true;
        return;
    }
    if (f.kind == Kind::Root || f.kind == Kind::Resource || f.kind == Kind::Station ||
        f.kind == Kind::Value || f.kind == Kind::Timer || f.kind == Kind::Port ||
        f.kind == Kind::Subject) {
        if (f.kind == Kind::Station && !std::strcmp(key, "id") && identifier())
            std::strcpy(stationIds_[stationCount_ - 1], text_);
        if (f.kind == Kind::Resource) {
            if (!std::strcmp(key, "id") && identifier())
                std::strcpy(resourceIds_[f.index], text_);
            if (!std::strcmp(key, "kind"))
                resourceTypes_[f.index] = type();
        }
        if (f.kind == Kind::Value || f.kind == Kind::Timer || f.kind == Kind::Port) {
            Symbol &s = (f.kind == Kind::Value   ? values_
                         : f.kind == Kind::Timer ? timers_
                                                 : ports_)[f.index];
            if (!std::strcmp(key, "id"))
                s.id = intern();
            if (!std::strcmp(key, "type"))
                s.type = type();
            if (!std::strcmp(key, "direction"))
                s.input = equals("input");
        }
        if (f.kind == Kind::Subject) {
            if (!std::strcmp(key, "id")) {
                f.index = subject();
                if (f.index) {
                    subjects_[f.index - 1].defined = true;
                    subjects_[f.index - 1].definition = subjectDefinitions_++;
                }
            }
            if (!std::strcmp(key, "originStation") && f.index && identifier())
                std::strcpy(subjects_[f.index - 1].origin, text_);
        }
        return;
    }
    if (f.kind == Kind::TriggerList) {
        Rule &r = rules_[ruleCount_ - 1];
        if (r.triggerCount == TriggerEntries) {
            fail(RuleError::OperatingProfileLimit, offset_);
            return;
        }
        const bool fact = !std::strcmp(frames_[depth_ - 2].key, "subjects");
        r.triggers[r.triggerCount++] = fact ? subject() : intern();
        return;
    }
    if (f.kind == Kind::Rule) {
        if (!std::strcmp(key, "generation")) {
            if (!integer(1, 4294967295LL))
                fail(RuleError::Range, offset_);
        } else if (!std::strcmp(key, "priority")) {
            if (!integer(0, 65535))
                fail(RuleError::Range, offset_);
        } else if (!std::strcmp(key, "id")) {
            if (!identifier())
                fail(RuleError::Value, offset_);
            else if (previousRule_[0] && std::strcmp(previousRule_, text_) >= 0)
                fail(RuleError::ListOrder, offset_);
            else
                std::strcpy(previousRule_, text_);
        } else if (!std::strcmp(key, "mode")) {
            if (!string() || (!equals("once") && !equals("each")))
                fail(RuleError::Value, offset_);
            rules_[f.index].once = equals("once");
        } else
            fail(RuleError::Type, offset_);
        return;
    }
    if (f.kind == Kind::Trigger) {
        Rule &r = rules_[ruleCount_ - 1];
        if (!std::strcmp(key, "kind")) {
            r.trigger = string() ? equals("local")   ? 1
                                   : equals("timer") ? 2
                                   : equals("fact")  ? 3
                                                     : 0
                                 : 0;
            if (!r.trigger)
                fail(RuleError::Value, offset_);
        } else if (!std::strcmp(key, "timer"))
            r.timer = intern();
        else
            fail(RuleError::Type, offset_);
        return;
    }
    if (f.kind == Kind::Offline) {
        if (!std::strcmp(key, "policy")) {
            Rule &r = rules_[ruleCount_ - 1];
            r.policy = string() ? equals("wait")             ? 1
                                  : equals("local_continue") ? 2
                                  : equals("local_fallback") ? 3
                                                             : 0
                                : 0;
            if (!r.policy)
                fail(RuleError::Value, offset_);
        } else if (!std::strcmp(key, "rule"))
            intern();
        else
            fail(RuleError::FieldSet, offset_);
        return;
    }
    if (f.kind == Kind::Action) {
        Action &a = actions_[f.index];
        if (!std::strcmp(key, "op"))
            a.op = op();
        else if (!std::strcmp(key, "id") || !std::strcmp(key, "port") || !std::strcmp(key, "timer"))
            a.target = intern();
        else if (!std::strcmp(key, "subject"))
            a.target = subject();
        else if (!std::strcmp(key, "resource"))
            a.resource = intern();
        else
            fail(RuleError::Type, offset_);
        return;
    }
    if (f.kind == Kind::Expression) {
        Node &n = nodes_[f.index - 1];
        if (!std::strcmp(key, "op"))
            n.op = op();
        else if (!std::strcmp(key, "type")) {
            if (!string() || (!equals("bool") && !equals("int32")))
                fail(RuleError::Type, offset_);
            n.literal = type();
        } else if (!std::strcmp(key, "value")) {
            n.scalar = scalar_ == CanonicalJsonToken::Boolean   ? Type::Boolean
                       : integer(-2147483647LL - 1, 2147483647) ? Type::Integer
                                                                : Type::Invalid;
            if (n.scalar == Type::Invalid)
                fail(RuleError::Range, offset_);
        } else if (!std::strcmp(key, "field")) {
            if (!string() || !equals("value"))
                fail(RuleError::Value, offset_);
        } else if (!std::strcmp(key, "id"))
            n.ref = intern();
        else if (!std::strcmp(key, "subject"))
            n.ref = subject();
        else
            fail(RuleError::Type, offset_);
        return;
    }
    fail(RuleError::Type, offset_);
}
void PackageRuleReader::close() {
    const Frame &f = frames_[depth_ - 1];
    if (f.kind == Kind::Rule)
        fields(255);
    else if (f.kind == Kind::Trigger) {
        const Rule &r = rules_[ruleCount_ - 1];
        fields(r.trigger == 1 ? 3 : r.trigger == 2 ? 9 : 5);
    } else if (f.kind == Kind::Offline) {
        const Rule &r = rules_[ruleCount_ - 1];
        fields(r.policy == 3 ? 3 : 1);
        if (r.policy == 3)
            unsupported_ = true;
    } else if (f.kind == Kind::Expression) {
        const Node &n = nodes_[f.index - 1];
        std::uint32_t expected = 0;
        switch (n.op) {
        case Op::Literal:
            expected = 800;
            break; // op,type,value
        case Op::Value:
        case Op::PortIs:
            expected = 40;
            break;
        case Op::Input:
            expected = 36;
            break;
        case Op::Fact:
            expected = 160;
            break;
        case Op::Not:
            expected = 33;
            break;
        case Op::All:
        case Op::Any:
            expected = 34;
            break;
        case Op::Eq:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge:
        case Op::Add:
        case Op::Sub:
            expected = 112;
            break;
        default:
            fail(RuleError::Value, offset_);
            break;
        }
        fields(expected);
        if (n.op == Op::Literal && n.literal != n.scalar)
            fail(RuleError::Type, offset_);
    } else if (f.kind == Kind::Arguments) {
        const Node &n = nodes_[frames_[depth_ - 2].index - 1];
        if (n.count < 2 || n.count > 8)
            fail(RuleError::Range, offset_);
    } else if (f.kind == Kind::Action) {
        const Action &a = actions_[f.index];
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
            fail(RuleError::Value, offset_);
            break;
        }
    } else if (f.kind == Kind::Station && result_.error == RuleError::None)
        station();
}
PackageRuleReader::Symbol *PackageRuleReader::find(Symbol *symbols, std::size_t count,
                                                   std::uint16_t id) {
    for (std::size_t i = 0; i < count; ++i)
        if (symbols[i].id == id && id)
            return &symbols[i];
    return nullptr;
}
void PackageRuleReader::reference(std::uint16_t id, bool emit, std::size_t offset) {
    if (!id)
        return;
    if (refCount_ == SubjectReferences) {
        fail(RuleError::OperatingProfileLimit, offset);
        return;
    }
    SubjectRef &ref = refs_[refCount_++];
    ref.id = id;
    ref.station = static_cast<std::uint8_t>(stationCount_ - 1);
    ref.emit = emit;
    ref.offset = offset;
    ref.rule = validationRule_;
    ref.action = validationAction_;
    ref.trigger = validationTrigger_;
}
PackageRuleReader::Type PackageRuleReader::expression(std::uint16_t index, const Rule &rule,
                                                      bool positive) {
    if (!index || index > nodeCount_) {
        fail(RuleError::Type, rule.offset);
        return Type::Invalid;
    }
    const Node &n = nodes_[index - 1];
    if (positive && n.op != Op::Fact && n.op != Op::All && n.op != Op::Any)
        fail(RuleError::Type, n.offset);
    switch (n.op) {
    case Op::Literal:
        return n.literal;
    case Op::Value: {
        Symbol *s = find(values_, valueCount_, n.ref);
        if (!s)
            fail(RuleError::Reference, n.offset);
        return s ? s->type : Type::Invalid;
    }
    case Op::Input: {
        if (rule.trigger != 1 || !rule.triggerCount) {
            fail(RuleError::Type, n.offset);
            return Type::Invalid;
        }
        Symbol *s = find(ports_, portCount_, rule.triggers[0]);
        return s ? s->type : Type::Invalid;
    }
    case Op::PortIs: {
        Symbol *p = find(ports_, portCount_, n.ref);
        if (rule.trigger != 1 || !p || !p->input ||
            (p->type != Type::Boolean && p->type != Type::Integer))
            fail(RuleError::Reference, n.offset);
        return Type::Boolean;
    }
    case Op::Fact:
        reference(n.ref, false, n.offset);
        return Type::Boolean;
    default:
        break;
    }
    Type first = Type::Invalid;
    for (std::size_t i = 0; i < n.count; ++i) {
        const Type childType = expression(n.children[i], rule, positive);
        if (i == 0)
            first = childType;
        if (n.op == Op::All || n.op == Op::Any || n.op == Op::Not) {
            if (childType != Type::Boolean)
                fail(RuleError::Type, n.offset);
        } else if (n.op == Op::Eq) {
            if (childType != first || (childType != Type::Boolean && childType != Type::Integer))
                fail(RuleError::Type, n.offset);
        } else if (childType != Type::Integer)
            fail(RuleError::Type, n.offset);
    }
    if ((n.op == Op::Not && n.count != 1) ||
        (n.op != Op::Not && n.op != Op::All && n.op != Op::Any && n.count != 2))
        fail(RuleError::FieldSet, n.offset);
    return n.op == Op::Add || n.op == Op::Sub ? Type::Integer : Type::Boolean;
}
void PackageRuleReader::station() {
    std::uint16_t portOutputs[Ports] = {};
    bool portFact[Ports] = {};
    for (std::size_t ri = 0; ri < ruleCount_ && result_.error == RuleError::None; ++ri) {
        const Rule &r = rules_[ri];
        validationRule_ = static_cast<std::uint8_t>(ri);
        validationAction_ = 255;
        context(stationCount_ - 1, ri, "offline");
        if (r.policy != 3 && ((r.trigger != 3 && r.policy != 2) ||
                              (r.trigger == 3 && r.policy != 1 && r.policy != 2)))
            fail(RuleError::Value, r.offset);
        if (r.trigger == 3 && !r.once)
            fail(RuleError::Type, r.offset);
        Type inputType = Type::Invalid;
        context(stationCount_ - 1, ri, "trigger");
        validationTrigger_ = true;
        if (r.trigger == 2 && !find(timers_, timerCount_, r.timer))
            fail(RuleError::Reference, r.offset);
        for (std::size_t ti = 0; ti < r.triggerCount; ++ti) {
            if (r.trigger == 3)
                reference(r.triggers[ti], false, r.offset);
            else {
                Symbol *p = find(ports_, portCount_, r.triggers[ti]);
                if (!p) {
                    fail(RuleError::Reference, r.offset);
                    continue;
                }
                if (!p->input || (p->type != Type::Boolean && p->type != Type::Integer))
                    fail(RuleError::Type, r.offset);
                if (inputType != Type::Invalid && inputType != p->type)
                    fail(RuleError::Type, r.offset);
                inputType = p->type;
            }
        }
        context(stationCount_ - 1, ri, "guard");
        validationTrigger_ = false;
        if (expression(r.guard, r, r.trigger == 3) != Type::Boolean)
            fail(RuleError::Type, r.offset);
        for (std::size_t ai = 0; ai < r.actionCount; ++ai) {
            validationAction_ = static_cast<std::uint8_t>(ai);
            context(stationCount_ - 1, ri, "", ai);
            const Action &a = actions_[r.firstAction + ai];
            if (r.trigger == 3 && a.op != Op::Emit && a.op != Op::Effect)
                fail(RuleError::Type, a.offset);
            if (a.op == Op::Set || a.op == Op::Add) {
                Symbol *s = find(values_, valueCount_, a.target);
                if (!s) {
                    fail(RuleError::Reference, a.offset);
                    continue;
                }
                const Type t = expression(a.expr, r);
                if (t != s->type || (a.op == Op::Add && t != Type::Integer))
                    fail(RuleError::Type, a.offset);
            } else if (a.op == Op::Arm || a.op == Op::Cancel) {
                if (!find(timers_, timerCount_, a.target))
                    fail(RuleError::Reference, a.offset);
            } else if (a.op == Op::Emit) {
                reference(a.target, true, a.offset);
                if (a.target) {
                    SubjectSymbol &s = subjects_[a.target - 1];
                    ++s.outputs;
                    s.factOutput |= r.trigger == 3;
                    if (s.outputs > 1 && s.factOutput)
                        fail(RuleError::Conflict, a.offset);
                    else if (!r.once || s.outputs > 1)
                        fail(RuleError::OperatingProfileLimit, a.offset);
                }
            } else if (a.op == Op::Effect) {
                Symbol *p = find(ports_, portCount_, a.target);
                if (!p) {
                    fail(RuleError::Reference, a.offset);
                    continue;
                }
                Type resource = Type::Invalid;
                for (std::size_t i = 0; i < resourceCount_; ++i)
                    if (a.resource && !std::strcmp(resourceIds_[i], ids_[a.resource - 1]))
                        resource = resourceTypes_[i];
                if (resource == Type::Invalid)
                    fail(RuleError::Reference, a.offset);
                if (p->input || (p->type != Type::Light && p->type != Type::Audio) ||
                    resource != p->type)
                    fail(RuleError::Type, a.offset);
                const std::size_t pi = static_cast<std::size_t>(p - ports_);
                ++portOutputs[pi];
                portFact[pi] |= r.trigger == 3;
                if (portOutputs[pi] > 1 && portFact[pi])
                    fail(RuleError::Conflict, a.offset);
            }
            // Writes/intents within one rule always coexist; across local rules
            // only matching trigger contexts can participate in the same plan.
            for (std::size_t bj = 0; bj < ai; ++bj) {
                const Action &b = actions_[r.firstAction + bj];
                const bool values =
                    (a.op == Op::Set || a.op == Op::Add) && (b.op == Op::Set || b.op == Op::Add);
                const bool timers = (a.op == Op::Arm || a.op == Op::Cancel) &&
                                    (b.op == Op::Arm || b.op == Op::Cancel);
                if (a.target == b.target &&
                    (values || timers ||
                     (a.op == b.op && (a.op == Op::Emit || a.op == Op::Effect))))
                    fail(RuleError::Conflict, a.offset);
            }
            if (r.trigger != 3)
                for (std::size_t rj = 0; rj < ri; ++rj) {
                    const Rule &other = rules_[rj];
                    if (other.trigger != r.trigger)
                        continue;
                    bool overlap = r.trigger == 2 && r.timer == other.timer;
                    if (r.trigger == 1)
                        for (std::size_t x = 0; x < r.triggerCount; ++x)
                            for (std::size_t y = 0; y < other.triggerCount; ++y)
                                overlap |= r.triggers[x] == other.triggers[y];
                    if (!overlap)
                        continue;
                    for (std::size_t j = 0; j < other.actionCount; ++j) {
                        const Action &b = actions_[other.firstAction + j];
                        const bool values = (a.op == Op::Set || a.op == Op::Add) &&
                                            (b.op == Op::Set || b.op == Op::Add);
                        const bool timers = (a.op == Op::Arm || a.op == Op::Cancel) &&
                                            (b.op == Op::Arm || b.op == Op::Cancel);
                        if (a.target == b.target &&
                            (values || timers ||
                             (a.op == b.op && (a.op == Op::Emit || a.op == Op::Effect))))
                            fail(RuleError::OperatingProfileLimit, a.offset);
                    }
                }
        }
    }
    context_[0] = '\0';
}
RuleResult PackageRuleReader::finish(DeclarationResult declarations) {
    result_.declarations = declarations;
    if (declarations.scope != DeclarationScope::DeclarationsOnly) {
        result_.scope = RuleScope::Rejected;
        result_.error = RuleError::Declarations;
        result_.offset = declarations.offset;
        std::strcpy(result_.path, declarations.path);
        return result_;
    }
    for (std::size_t i = 0; i < refCount_ && result_.error == RuleError::None; ++i) {
        const SubjectRef &r = refs_[i];
        const SubjectSymbol &s = subjects_[r.id - 1];
        context(r.station, r.rule, r.trigger ? "trigger" : "guard", r.action);
        if (!s.defined || (r.emit && std::strcmp(s.origin, stationIds_[r.station])))
            fail(RuleError::Reference, r.offset);
    }
    for (std::size_t i = 0; i < subjectCount_ && result_.error == RuleError::None; ++i) {
        const SubjectSymbol &s = subjects_[i];
        std::snprintf(context_, sizeof(context_), "/subjects/%u/originStation",
                      static_cast<unsigned>(s.definition));
        bool origin = false;
        for (std::size_t j = 0; j < stationCount_; ++j)
            origin |= !std::strcmp(s.origin, stationIds_[j]);
        if (!s.defined || !origin)
            fail(RuleError::Reference, declarations.offset);
    }
    context_[0] = '\0';
    if (result_.error == RuleError::None) {
        result_.offset = declarations.offset;
        if (unsupported_)
            result_.error = RuleError::UnsupportedScope;
        else
            result_.scope = RuleScope::DirectRulesOnly;
    }
    return result_;
}
RuleResult PackageRuleReader::validate(IPackageByteSource &source) {
    reset();
    return finish(declarations_.validate(source, this));
}
RuleResult PackageRuleReader::validate(const char *bytes, std::size_t count) {
    reset();
    return finish(declarations_.validate(bytes, count, this));
}
} // namespace configuration
} // namespace larp
