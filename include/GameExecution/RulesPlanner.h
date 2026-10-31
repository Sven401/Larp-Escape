#pragma once
#include "GameConfiguration/PackageModules.h"

namespace larp {
namespace execution {
namespace rules {
using configuration::module_ir::Id;
using configuration::module_ir::Op;
using configuration::module_ir::Type;
template <typename T> struct Span {
    const T *data = nullptr;
    std::size_t count = 0;
};
using configuration::module_ir::Pool;
struct Identity {
    std::uint8_t bytes[16] = {};
};
struct Digest {
    std::uint8_t bytes[32] = {};
};
struct Context {
    Identity event, run, station, box, sourceEpoch;
    Digest deployment, binding, content;
};
struct FactId {
    Identity event, origin, epoch;
    std::uint64_t sequence = 0;
};
// A projection of an already admitted D0 fact. This pure kernel performs no
// signature/role verification; its owner must supply verified immutable records.
struct FactRef {
    FactId id;
    Digest digest, deployment, binding;
    Identity run, station, subject;
};
struct Evidence {
    Span<FactRef> refs;
};
struct RuleBinding {
    const char *qualified = nullptr;
    Identity ruleId;
};
struct PortBinding {
    const char *name = nullptr;
    Identity exclusive; // Opaque deployment-bound physical exclusive resource.
    Span<const char *> resources;
};
struct SubjectBinding {
    const char *name = nullptr;
    Identity subject, station, box, epoch;
    Digest binding;
    Span<Identity> epochs; // Optional complete list; epoch is the one-entry convenience input.
};
enum class Error : std::uint8_t {
    None,
    InvalidProgram,
    Type,
    Reference,
    Conflict,
    Overflow,
    Budget,
    OperatingProfile,
    DependencyLimit,
    Context,
    InvalidSnapshot,
    Stale,
    Unsupported,
    Unavailable,
    Stopped,
    RecoveryRequired
};
enum class Decision : std::uint8_t { Rejected, Deferred, Ready, Duplicate, StaleTimer };
struct Result {
    Decision decision = Decision::Rejected;
    Error error = Error::None;
    Id rule = 0, node = 0, action = 0;
};
struct Symbol {
    Id name = 0;
    Type type = Type::Invalid;
    std::int32_t initial = 0;
};
struct Port {
    Id name = 0;
    Type type = Type::Invalid;
    bool input = false;
    Identity exclusive;
    std::uint32_t firstResource = 0;
    std::uint16_t resourceCount = 0;
};
struct Subject {
    Id name = 0;
    SubjectBinding binding;
};
struct Resource {
    Id name = 0;
    Type type = Type::Invalid;
};
struct Rule {
    configuration::module_ir::Rule ir;
    Id name = 0;
    Identity stable;
};
struct Program {
    Context context;
    Pool<char> strings;
    Pool<std::uint32_t> offsets;
    Pool<configuration::module_ir::Node> nodes;
    Pool<configuration::module_ir::Action> actions;
    Pool<Rule> rules;
    Pool<Symbol> values, timers;
    Pool<Port> ports;
    Pool<Subject> subjects;
    Pool<Resource> resources;
    Pool<Identity> epochs;
    Pool<Id> allowedResources;
    Pool<Id> triggers;
    bool ready = false;
};
// Destination is staging storage, never the active immutable program. Published
// only by the compiler's non-failing final consumer commit. No scratch borrow.
class ProgramBuilder : public configuration::IExpandedStationConsumer {
  public:
    ProgramBuilder(Program &destination, const char *station, const Context &context,
                   Span<RuleBinding> rules, Span<SubjectBinding> subjects, Span<PortBinding> ports);
    bool begin(const std::uint8_t contentDigest[32]) override;
    bool station(const configuration::ModuleCompileWorkspace &source,
                 const configuration::ExpandedStationWorkspace &expanded, Id station) override;
    bool prepare() override;
    void commit() override;
    void abort() override;
    Result result() const {
        return result_;
    }

  private:
    Program &program_;
    const char *station_;
    Context context_;
    Span<RuleBinding> bindings_;
    Span<SubjectBinding> subjectBindings_;
    Span<PortBinding> portBindings_;
    Result result_;
    bool found_ = false, prepared_ = false;
    Id intern(const char *text);
    Id qualified(const configuration::module_ir::Name &name,
                 const configuration::ModuleCompileWorkspace &source);
    bool validate();
};
struct Value {
    Type type = Type::Invalid;
    std::int32_t value = 0;
    Evidence evidence;
};
struct Timer {
    bool active = false;
    std::uint64_t deadline = 0, generation = 0;
    Evidence evidence;
};
enum class FactClass : std::uint8_t { Unverified, CurrentSolved, Historical, Control, Receipt };
struct Fact {
    Id subject = 0;
    FactRef ref;
    FactClass kind = FactClass::Unverified;
};
struct Latch {
    Identity rule;
    std::uint32_t generation = 0;
    Evidence evidence;
};
enum class InputKind : std::uint8_t { Local, Timer, Fact };
struct Input {
    Context context;
    InputKind kind = InputKind::Local;
    std::uint64_t bootGeneration = 0, sequence = 0;
    Id port = 0, timer = 0;
    std::uint64_t armGeneration = 0;
    Type type = Type::Invalid;
    bool known = true;
    std::int32_t value = 0;
    Fact fact;
};
struct Receipt {
    Context context;
    Input input;
};
struct Fence {
    std::uint64_t progress = 0, profile = 0, trust = 0, facts = 0, ledger = 0, resources = 0;
    std::uint64_t rights = 0, capacity = 0;
    std::uint64_t lastIssued = 0, activeTime = 0;
};
enum class Mode : std::uint8_t { Running, Paused, Stopped, RecoveryRequired };
struct State {
    Context context;
    Fence fence;
    Mode mode = Mode::Stopped;
    Span<Value> values;
    Span<Timer> timers;
    Span<Fact> facts;
    Span<Latch> latches;
    Span<Receipt> ledger;
    Span<Id> solved; // Locally produced subjects, including not-yet-looped-back facts.
};
struct Capacities {
    std::size_t receipts = 0, facts = 0, latches = 0;
    std::size_t outbox = 0, intents = 0;
    bool factsAvailable = true, ledgerAvailable = true, resourcesAvailable = true;
};
struct ValueWrite {
    Id target = 0;
    Value value;
};
struct TimerWrite {
    Id target = 0;
    Timer value;
};
struct Emission {
    Id subject = 0;
    Identity rule;
    std::uint32_t generation = 0;
    FactId candidate;
    Evidence dependencies;
};
struct Intent {
    Id port = 0, resource = 0;
    Identity rule, exclusive;
    Evidence evidence;
};
struct Plan {
    bool ready = false;
    Context context;
    Fence expected;
    Receipt receipt;
    Id consumedTimer = 0;
    Pool<ValueWrite> values;
    Pool<TimerWrite> timers;
    Pool<Latch> latches;
    Pool<Emission> emissions;
    Pool<Intent> intents;
    Pool<FactRef> evidence;
    bool admitsFact = false;
    Fact fact;
};
// Proof alternatives are exact inclusion-minimal antichains, not greedy OR
// certificates. Capacity exhaustion rejects/defer the entire operation.
struct Proof {
    std::uint32_t first = 0, next = 0;
    std::uint16_t count = 0;
};
struct Evaluation {
    Type type = Type::Invalid;
    bool known = false, evaluated = false, proving = false, proved = false;
    std::int32_t value = 0;
    std::uint32_t family = 0;
};
struct PlannerWorkspace {
    Pool<FactRef> catalog;
    Pool<Proof> proofs;
    Pool<Id> proofRefs;
    Evaluation evaluations[512];
    std::size_t proofWorkLimit = 262144;
};
struct Due {
    Id timer = 0;
    std::uint64_t deadline = 0, generation = 0;
};
struct StateStorage {
    Pool<Value> values;
    Pool<Timer> timers;
    Pool<Fact> facts;
    Pool<Latch> latches;
    Pool<Receipt> ledger;
    Pool<Id> solved;
    Pool<FactRef> evidence;
};
// All sources/programs/state/storage belong to one exclusive synchronous owner.
// No callbacks, hardware, allocation, durable commit or authentication here.
class Planner {
  public:
    Result plan(const Program &program, const State &state, const Input &input,
                const Capacities &capacity, Plan &destination, PlannerWorkspace &workspace);
    static Result initialize(const Program &program, const Fence &fence, StateStorage &storage,
                             State &destination);
    static Result commitRam(const Program &program, const State &current, const Plan &plan,
                            StateStorage &staging, State &destination);
    static Result due(const Program &program, const State &state, Pool<Due> &destination);
    static Result advanceClock(State &state, std::uint64_t activeTime);
    static Result control(State &state, Mode mode);

  private:
    const Program *program_ = nullptr;
    const State *state_ = nullptr;
    const Input *input_ = nullptr;
    Plan *plan_ = nullptr;
    PlannerWorkspace *work_ = nullptr;
    Result result_;
    std::size_t visits_ = 0, proofWork_ = 0, actions_ = 0;
    void fail(Error error, bool deferred = false);
    Id admit(const FactRef &ref);
    bool validFact(const Fact &fact) const;
    bool snapshot();
    Evaluation evaluate(Id node);
    std::uint32_t proof(Id node);
    std::uint32_t singleton(Evidence evidence);
    std::uint32_t insert(std::uint32_t family, const Id *refs, std::size_t count);
    std::uint32_t combine(std::uint32_t left, std::uint32_t right, bool alternatives);
    bool select(std::uint32_t family, Evidence &destination, bool newFact = false);
    bool action(const Rule &rule, Id action, std::uint32_t cause);
};
const char *name(const Program &program, Id id);
} // namespace rules
} // namespace execution
} // namespace larp
