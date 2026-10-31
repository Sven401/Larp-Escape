#pragma once
#include "GameConfiguration/PackageDeclarations.h"

namespace larp {
namespace configuration {
enum class ModuleError : std::uint8_t {
    None,
    CanonicalInput,
    Declarations,
    FieldSet,
    Type,
    Value,
    Reference,
    Range,
    ListOrder,
    ModuleConflict,
    MissingModule,
    CryptoFailure,
    DigestMismatch,
    SourceFailure,
    Cycle,
    Budget,
    OperatingProfileLimit,
    Constraint,
    Conflict,
    UnsupportedScope,
    SinkFailure
};
enum class ModuleScope : std::uint8_t { Rejected, ExpandedUnauthenticated };
struct ModuleCompileResult {
    ModuleScope scope = ModuleScope::Rejected;
    ModuleError error = ModuleError::None;
    CanonicalJsonResult canonical;
    std::size_t offset = 0;
    std::uint16_t source = 0; // 0 content; 1..16 manifest pin
    char path[256] = {};
    std::uint16_t instances = 0;
    std::uint8_t contentDigest[32] = {};
};
struct ModulePin {
    char id[49] = {};
    char version[18] = {};
    char digest[65] = {};
};
class IModuleSourceProvider {
  public:
    virtual ~IModuleSourceProvider() = default;
    // Any non-null returned source is closed once, including on false.
    virtual bool open(const ModulePin &pin, IPackageByteSource *&source) = 0;
    virtual void close(IPackageByteSource &source) = 0;
};
class IPackageDigest {
  public:
    virtual ~IPackageDigest() = default;
    // Failed begin/update/finish remain abortable. begin clears stale backend
    // state. Buffers are synchronous borrows and must not be retained.
    virtual bool begin() = 0;
    virtual bool update(const std::uint8_t *bytes, std::size_t count) = 0;
    virtual bool finish(std::uint8_t digest[32], std::size_t &written) = 0;
    virtual bool abort() = 0;
};
// All writes are provisional. A failed operation must leave the transaction
// abortable; only successful commit publishes the complete compilation.
class IExpandedPackageSink {
  public:
    virtual ~IExpandedPackageSink() = default;
    virtual bool begin() = 0;
    virtual bool beginStation(const char *id) = 0;
    virtual bool write(const char *bytes, std::size_t count) = 0;
    virtual bool endStation() = 0;
    virtual bool commit() = 0;
    virtual void abort() = 0;
};
namespace module_ir {
using Id = std::uint16_t;
struct Location {
    std::uint32_t offset = 0;
    std::uint16_t source = 0, station = 65535, index = 0, subindex = 65535;
    std::uint8_t role = 0;
};
enum class Type : std::uint8_t { Invalid, Boolean, Integer, Light, Audio, Solved };
enum class Op : std::uint8_t {
    Invalid,
    Literal,
    Parameter,
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
enum class SymbolKind : std::uint8_t {
    Parameter,
    Value,
    Timer,
    Port,
    Input,
    Output,
    Resource,
    Subject
};
struct Literal {
    Type type = Type::Invalid;
    std::int32_t value = 0;
    Id parameter = 0;
};
struct Node {
    Location location;
    Op op = Op::Invalid;
    Type type = Type::Invalid;
    std::int32_t value = 0;
    Id ref = 0;
    Id children[8] = {};
    std::uint8_t count = 0;
};
struct Symbol {
    Location location;
    Id owner = 0, id = 0, auxiliary = 0;
    SymbolKind kind = SymbolKind::Value;
    Type type = Type::Invalid;
    Literal initial;
    std::int32_t minimum = 0, maximum = 0;
    bool input = false;
};
struct Action {
    Location location;
    Op op = Op::Invalid;
    Id target = 0, resource = 0, expr = 0;
};
struct Rule {
    Location location;
    Id owner = 0, id = 0, guard = 0, firstAction = 0, firstTrigger = 0, timer = 0, fallback = 0;
    std::uint32_t generation = 0;
    std::uint16_t priority = 0, actionCount = 0, triggerCount = 0;
    std::uint8_t trigger = 0, policy = 0;
    bool once = false;
};
struct Definition {
    Location location;
    Id id = 0, version = 0;
    bool module = false;
};
struct Instance {
    Location location;
    Id owner = 0, id = 0, module = 0, version = 0, digest = 0;
};
struct Binding {
    Location location;
    Id instance = 0, from = 0, to = 0;
    std::uint8_t kind = 0; // 0 parameter, 1 input, 2 output, 3 resource
    Literal value;
};
struct Constraint {
    Location location;
    Id owner = 0, expression = 0;
};
template <typename T> struct Pool {
    T *data = nullptr;
    std::size_t capacity = 0, count = 0;
};
struct Name {
    Id segments[5] = {};
    std::uint8_t count = 0;
};
struct ExpandedSymbol {
    Name name;
    Type type = Type::Invalid;
    Literal initial;
};
struct ExpandedRule {
    Name name;
    Rule rule;
};
} // namespace module_ir
// Storage belongs to the caller. Capacities are explicit operating limits;
// exhaustion never truncates a semantically valid document into another one.
struct ModuleCompileWorkspace {
    module_ir::Pool<char> strings;
    module_ir::Pool<std::uint32_t> stringOffsets;
    module_ir::Pool<module_ir::Node> nodes;
    module_ir::Pool<module_ir::Symbol> symbols;
    module_ir::Pool<module_ir::Action> actions;
    module_ir::Pool<module_ir::Rule> rules;
    module_ir::Pool<module_ir::Definition> definitions;
    module_ir::Pool<module_ir::Instance> instances;
    module_ir::Pool<module_ir::Binding> bindings;
    module_ir::Pool<module_ir::Constraint> constraints;
    module_ir::Pool<module_ir::Id> triggers;
};
struct ExpandedStationWorkspace {
    module_ir::Pool<module_ir::Node> nodes;
    module_ir::Pool<module_ir::Action> actions;
    module_ir::Pool<module_ir::ExpandedRule> rules;
    module_ir::Pool<module_ir::ExpandedSymbol> values;
    module_ir::Pool<module_ir::ExpandedSymbol> timers;
    module_ir::Pool<module_ir::Id> triggers;
};
// Exclusive, non-reentrant call: callbacks must not mutate sources, compiler or
// caller workspaces, nor retain write/update buffers. All must outlive the call.
// Pure compiler: one pass per immutable byte source, no heap/DOM/replay. Digest
// verification is required, and does not grant author trust or activation rights.
class PackageModuleCompiler : private ICanonicalJsonObserver {
  public:
    ModuleCompileResult compile(IPackageByteSource &content, IModuleSourceProvider &modules,
                                IPackageDigest &digest, IExpandedPackageSink &sink,
                                ModuleCompileWorkspace &workspace,
                                ExpandedStationWorkspace &expanded);

  private:
    enum class Kind : std::uint8_t {
        Skip,
        Content,
        Module,
        Stations,
        Station,
        Pins,
        Pin,
        Parameters,
        Parameter,
        Values,
        Value,
        Timers,
        Timer,
        Ports,
        Port,
        Inputs,
        Input,
        Outputs,
        Output,
        Resources,
        Resource,
        Subjects,
        Subject,
        Rules,
        Rule,
        Trigger,
        TriggerList,
        Offline,
        Actions,
        Action,
        Expression,
        Arguments,
        Instances,
        Instance,
        ParameterMap,
        InputMap,
        OutputMap,
        ResourceMap,
        BindingLiteral,
        InitialParameter,
        Constraints
    };
    struct Frame {
        Kind kind = Kind::Skip;
        std::uint32_t seen = 0;
        module_ir::Id index = 0;
        std::uint16_t count = 0;
        char key[49] = {}, previous[49] = {};
        std::uint8_t expressionDepth = 0;
    };
    struct Context {
        module_ir::Id definition = 0;
        module_ir::Name path;
        module_ir::Id parameterSymbols[64] = {}, interfaceSymbols[128] = {};
        module_ir::Literal parameters[64];
        module_ir::Id interfaces[128] = {};
        std::uint16_t parameterCount = 0, interfaceCount = 0;
    };
    PackageDeclarationReader declarations_;
    CanonicalPackageJsonReader canonical_;
    ModuleCompileWorkspace *work_ = nullptr;
    ExpandedStationWorkspace *expanded_ = nullptr;
    IExpandedPackageSink *sink_ = nullptr;
    ModuleCompileResult result_;
    ModulePin pins_[16], pin_;
    std::size_t pinCount_ = 0, depth_ = 0, length_ = 0, offset_ = 0;
    Frame frames_[32];
    Context contexts_[5];
    std::size_t contextDepth_ = 0;
    module_ir::Id currentDefinition_ = 0, moduleDefinition_ = 0;
    module_ir::Id currentRule_ = 0, currentInstance_ = 0;
    std::uint16_t occurrences_ = 0;
    bool moduleDocument_ = false, writing_ = false;
    bool deferred_ = false;
    module_ir::Location location_;
    module_ir::Location location(Kind kind) const;
    CanonicalJsonToken scalar_ = CanonicalJsonToken::Key;
    char text_[1025] = {};
    void token(CanonicalJsonToken token, std::size_t offset) override;
    void scalarByte(std::uint8_t byte) override;
    void scalarEnd(std::size_t offset) override;
    void fail(ModuleError error);
    void fields(std::uint32_t expected);
    const char *string(module_ir::Id id) const;
    module_ir::Id intern();
    bool identifier() const;
    bool equals(const char *text) const;
    bool integer(std::int64_t low, std::int64_t high, std::int64_t &value) const;
    module_ir::Type type() const;
    module_ir::Op op() const;
    Kind child(bool array);
    void close();
    void scalar();
    module_ir::Literal primitive();
    bool validateDefinitions();
    bool validateInstance(module_ir::Id instance);
    module_ir::Id findSymbol(module_ir::Id owner, module_ir::Id id,
                             module_ir::SymbolKind kind) const;
    module_ir::Id findPin(module_ir::Id id, module_ir::Id version, module_ir::Id digest);
    module_ir::Type expressionType(module_ir::Id node, module_ir::Id owner, bool constraint,
                                   const module_ir::Rule *rule = nullptr);
    module_ir::Literal evaluate(module_ir::Id node, const Context &context);
    module_ir::Literal substitute(module_ir::Literal value, const Context &context);
    bool expand(module_ir::Id station);
    bool expandContext(Context &context);
    module_ir::Id copyExpression(module_ir::Id node, Context &context);
    module_ir::Id resolveInterface(module_ir::Id id, const Context &context, module_ir::Type type,
                                   bool output);
    module_ir::Id copyRule(const module_ir::Rule &rule, Context &context);
    bool validateExpanded(module_ir::Id station);
    bool emitStation(module_ir::Id station);
    bool write(const char *text);
    bool quoted(const char *text);
    bool name(const module_ir::Name &name);
    bool number(std::int64_t value);
    bool emitExpression(module_ir::Id node);
    bool emitLiteral(const module_ir::Literal &value);
    bool emitRule(const module_ir::ExpandedRule &rule);
    template <typename T> module_ir::Id append(module_ir::Pool<T> &pool) {
        if (!pool.data || pool.count == pool.capacity || pool.count >= 65534) {
            fail(ModuleError::OperatingProfileLimit);
            return 0;
        }
        pool.data[pool.count] = T{};
        return static_cast<module_ir::Id>(++pool.count);
    }
};
} // namespace configuration
} // namespace larp
