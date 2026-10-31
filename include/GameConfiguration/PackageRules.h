#pragma once
#include "GameConfiguration/PackageDeclarations.h"

namespace larp {
namespace configuration {
enum class RuleError : std::uint8_t {
    None,
    Declarations,
    FieldSet,
    Type,
    Value,
    Range,
    Reference,
    ListOrder,
    Budget,
    OperatingProfileLimit,
    UnsupportedScope,
    Conflict
};
enum class RuleScope : std::uint8_t { Rejected, DirectRulesOnly };
struct RuleResult {
    RuleScope scope = RuleScope::Rejected;
    RuleError error = RuleError::None;
    DeclarationResult declarations;
    std::size_t offset = 0;
    char path[256] = {};
};
// Exclusive synchronous validation. No executable package/model is published.
// Fixed semantic tables, no heap, byte replay, DOM or retained source pointers.
class PackageRuleReader : private ICanonicalJsonObserver {
  public:
    static const std::size_t Ports = 64;
    static const std::size_t Subjects = 128;
    static const std::size_t TriggerEntries = 64;
    static const std::size_t SubjectReferences = 4096;
    static const std::size_t LocalIdentifiers = 1024;
    RuleResult validate(IPackageByteSource &source);
    RuleResult validate(const char *bytes, std::size_t count);

  private:
    enum class Kind : std::uint8_t {
        Skip,
        Root,
        Stations,
        Station,
        Values,
        Value,
        Timers,
        Timer,
        Ports,
        Port,
        Rules,
        Rule,
        Trigger,
        TriggerList,
        Offline,
        Actions,
        Action,
        Expression,
        Arguments,
        Modules,
        Instances,
        Resources,
        Resource,
        Subjects,
        Subject
    };
    enum class Op : std::uint8_t {
        Invalid,
        Literal,
        Value,
        Input,
        PortIs,
        Fact,
        Not,
        All,
        Any,
        Eq,
        Lt,
        Le,
        Gt,
        Ge,
        Add,
        Sub,
        Set,
        Arm,
        Cancel,
        Emit,
        Effect
    };
    enum class Type : std::uint8_t { Invalid, Boolean, Integer, Light, Audio };
    struct Frame {
        Kind kind = Kind::Skip;
        std::uint32_t seen = 0;
        std::uint16_t index = 0;
        std::uint16_t count = 0;
        char key[49] = {};
    };
    struct Node {
        Op op = Op::Invalid;
        Type literal = Type::Invalid;
        Type scalar = Type::Invalid;
        std::uint16_t ref = 0;
        std::uint16_t children[8] = {};
        std::uint8_t count = 0;
        std::uint8_t depth = 1;
        std::size_t offset = 0;
    };
    struct Action {
        Op op = Op::Invalid;
        std::uint16_t target = 0;
        std::uint16_t resource = 0;
        std::uint16_t expr = 0;
        std::size_t offset = 0;
    };
    struct Rule {
        std::uint16_t firstAction = 0;
        std::uint16_t actionCount = 0;
        std::uint16_t guard = 0;
        std::uint16_t triggers[TriggerEntries] = {};
        std::uint16_t triggerCount = 0;
        std::uint16_t timer = 0;
        std::uint8_t trigger = 0;
        std::uint8_t policy = 0;
        bool once = false;
        std::size_t offset = 0;
    };
    struct Symbol {
        std::uint16_t id = 0;
        Type type = Type::Invalid;
        bool input = false;
    };
    struct SubjectSymbol {
        char id[49] = {};
        char origin[49] = {};
        bool defined = false;
        bool factOutput = false;
        std::uint16_t outputs = 0;
        std::uint16_t definition = 0;
    };
    struct SubjectRef {
        std::uint16_t id = 0;
        std::uint8_t station = 0;
        bool emit = false;
        bool trigger = false;
        std::uint8_t rule = 0;
        std::uint8_t action = 255;
        std::size_t offset = 0;
    };
    PackageDeclarationReader declarations_;
    RuleResult result_;
    Frame frames_[CanonicalPackageJsonReader::ContainerDepth];
    std::size_t depth_ = 0;
    CanonicalJsonToken scalar_ = CanonicalJsonToken::Key;
    char text_[97] = {};
    std::size_t length_ = 0;
    std::size_t offset_ = 0;
    char ids_[LocalIdentifiers][49];
    std::size_t idCount_ = 0;
    Node nodes_[512];
    std::size_t nodeCount_ = 0;
    Action actions_[512];
    std::size_t actionCount_ = 0;
    Rule rules_[64];
    std::size_t ruleCount_ = 0;
    Symbol values_[32], timers_[16], ports_[Ports];
    std::size_t valueCount_ = 0, timerCount_ = 0, portCount_ = 0;
    char stationIds_[8][49];
    std::size_t stationCount_ = 0;
    char previousRule_[49] = {};
    char resourceIds_[32][49];
    Type resourceTypes_[32];
    std::size_t resourceCount_ = 0;
    SubjectSymbol subjects_[Subjects];
    std::size_t subjectCount_ = 0;
    SubjectRef refs_[SubjectReferences];
    std::size_t refCount_ = 0;
    bool unsupported_ = false;
    char context_[128] = {};
    std::uint8_t validationRule_ = 0, validationAction_ = 255;
    bool validationTrigger_ = false;
    std::uint16_t subjectDefinitions_ = 0;
    void context(std::size_t station, std::size_t rule, const char *field,
                 std::size_t action = 255);
    void reset();
    RuleResult finish(DeclarationResult declarations);
    void token(CanonicalJsonToken token, std::size_t offset) override;
    void scalarByte(std::uint8_t byte) override;
    void scalarEnd(std::size_t offset) override;
    void fail(RuleError error, std::size_t offset);
    void path();
    Kind child(bool array);
    void close();
    void scalar();
    std::uint16_t intern();
    std::uint16_t subject();
    bool string() const;
    bool equals(const char *text) const;
    bool identifier() const;
    bool integer(std::int64_t low, std::int64_t high) const;
    Type type() const;
    Op op() const;
    void fields(std::uint32_t expected);
    Symbol *find(Symbol *symbols, std::size_t count, std::uint16_t id);
    Type expression(std::uint16_t index, const Rule &rule, bool positive = false);
    void station();
    void reference(std::uint16_t id, bool emit, std::size_t offset);
};
} // namespace configuration
} // namespace larp
