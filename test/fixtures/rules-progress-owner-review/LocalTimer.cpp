// Reuse the actual independent runtime/crypto fixture ports; no fake Admission.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type" // Renamed preserved main is not invoked.
#define main preservedOwnerCampaignMain
#include "main.cpp"
#undef main
#pragma GCC diagnostic pop
using namespace owner_review;
xr::Input localInput(World &world, std::uint64_t sequence, int value = 1) {
    xr::Input input;
    input.context = world.program.program.context;
    input.kind = xr::InputKind::Local;
    // Explicit synthetic OriginalInput label; NOT derived from the 128-bit boot clock.
    input.bootGeneration = 0x100000005ULL;
    input.sequence = sequence;
    input.value = value;
    for (std::size_t i = 0; i < world.program.program.ports.count; ++i)
        if (world.program.program.ports.data[i].input) {
            input.port = static_cast<Id>(i + 1);
            input.type = world.program.program.ports.data[i].type;
            break;
        }
    requireCodec(input.port != 0, "actual compiler input port");
    return input;
}
void correlated(Engine &e, const xr::Input &input, const ev::Result &original, unsigned op) {
    auto &w = e.world;
    auto records = e.low.snapshot().records;
    auto programs = e.store.programs;
    auto count = e.memory.work.applications.count, intents = e.memory.work.intents.count,
         history = e.memory.work.history.count;
    auto calls = w.sources.locals + w.sources.inputs + w.sources.lookups,
         metadata = w.metadata.calls;
    const auto r = e.owner.submit(input, id(op), request(op + 500));
    std::cerr << "CORRELATION kind=" << static_cast<unsigned>(input.kind)
              << " outcome=" << static_cast<unsigned>(r.journal.outcome)
              << " error=" << static_cast<unsigned>(r.error)
              << " original=" << hex(r.originalApplication.bytes, 16) << '\n';
    requireCodec(
        r.error == ev::Error::None && r.journal.outcome == rb::Outcome::Duplicate &&
            !std::memcmp(r.originalApplication.bytes, original.originalApplication.bytes, 16),
        "same source input under new correlation resolves original Final");
    requireCodec(r.journal.journal.revision == original.journal.journal.revision &&
                     r.journal.journal.index == original.journal.journal.index &&
                     r.journal.journal.firstIssued == original.journal.journal.firstIssued &&
                     r.journal.journal.lastIssued == original.journal.journal.lastIssued,
                 "exact original revision/index/issued range");
    requireCodec(e.low.snapshot().records == records && e.store.programs == programs &&
                     e.memory.work.applications.count == count &&
                     e.memory.work.intents.count == intents &&
                     e.memory.work.history.count == history &&
                     calls == w.sources.locals + w.sources.inputs + w.sources.lookups &&
                     metadata == w.metadata.calls && !w.ordering.mutationsBlocked(),
                 "correlation does no write lease source sign intent or receipt");
}
int main(int argc, char **argv) {
    requireCodec(argc == 2, "choose actual local or timer campaign");
    bool timer = std::string(argv[1]) == "timer";
    std::string raw;
    std::getline(std::cin, raw);
    World w(unhex(raw));
    w.enableSynthetic();
    std::unique_ptr<Engine> e(new Engine(w, true));
    activate(*e);
    xr::Input input = localInput(w, 1);
    const auto admitted = e->owner.submit(input, id(401), request(401));
    requireCodec(admitted.error == ev::Error::None, "first local intake admitted");
    pump(e->owner);
    auto first = e->owner.submit(input, id(401), request(401));
    requireCodec(first.error == ev::Error::None && first.journal.outcome == rb::Outcome::Duplicate,
                 "first actual local commit checked via original duplicate");
    requireCodec(e->owner.status(w.program.program.context, id(401)).status ==
                     xr::ManualStatus::Committed,
                 "first local uptake explicitly Committed");
    if (timer) {
        requireCodec(e->owner.state().timers.count == 1 && e->owner.state().timers.data[0].active &&
                         e->owner.state().timers.data[0].generation > 0,
                     "actual committed arm action creates active generation");
        auto t = e->owner.state().timers.data[0];
        w.clock.ticks += t.deadline - e->owner.state().fence.activeTime;
        input = {};
        input.context = w.program.program.context;
        input.kind = xr::InputKind::Timer;
        input.timer = 1;
        input.armGeneration = t.generation;
        const auto r = e->owner.submit(input, id(402), request(402));
        requireCodec(r.error == ev::Error::None, "actual due armed timer admitted");
        pump(e->owner);
        first = e->owner.submit(input, id(402), request(402));
        requireCodec(e->owner.status(w.program.program.context, id(402)).status ==
                             xr::ManualStatus::Committed &&
                         first.journal.outcome == rb::Outcome::Duplicate &&
                         !e->owner.state().timers.data[0].active &&
                         e->memory.work.intents.count == 1,
                     "actual timer first commit consumes arm and creates exactly one effect");
    } else
        requireCodec(e->owner.state().solved.count == 1 && e->owner.state().latches.count == 1 &&
                         e->owner.state().fence.lastIssued == 1 &&
                         e->memory.work.intents.count == 1,
                     "actual local first commit has own fact latch intent");
    std::cerr << "FIRST_COMMITTED kind=" << static_cast<unsigned>(input.kind)
              << " original=" << hex(first.originalApplication.bytes, 16)
              << " revision=" << first.journal.journal.revision
              << " range=" << first.journal.journal.firstIssued << ":"
              << first.journal.journal.lastIssued << '\n';
    correlated(*e, input, first, 403);

    const auto baseline = e->low.snapshot().records;
    requireCodec(e->owner.submit(input, id(102), request(999)).error == ev::Error::Conflict,
                 "Control operation collision before correlation");
    const auto claim = e->memory.work.intents.data[0].reservation.claim;
    requireCodec(e->owner.submit(input, claim, request(999)).error == ev::Error::Conflict,
                 "reserved Claim operation collision before correlation");
    auto changed = input;
    changed.value++;
    requireCodec(e->owner.submit(changed, id(404), request(404)).error == ev::Error::Conflict,
                 "same source ID altered value conflicts");
    changed = input;
    changed.known = !changed.known;
    requireCodec(e->owner.submit(changed, id(405), request(405)).error == ev::Error::Conflict,
                 "same source ID altered knownness conflicts");
    for (unsigned field = 0; field < 3; ++field) {
        changed = input;
        if (field == 0)
            changed.context.event = id(0xEE00);
        if (field == 1)
            changed.context.run = id(0xEE01);
        if (field == 2)
            changed.context.station = id(0xEE02);
        const auto rejected = e->owner.submit(changed, id(410 + field), request(410 + field));
        requireCodec(rejected.error != ev::Error::None &&
                         rejected.journal.outcome != rb::Outcome::Duplicate,
                     "foreign event/run/station never deduplicates across namespace");
    }
    requireCodec(e->low.snapshot().records == baseline,
                 "all conflict/namespace probes perform no write");
    if (timer) {
        auto arm = localInput(w, 2);
        requireCodec(e->owner.submit(arm, id(420), request(420)).error == ev::Error::None,
                     "real rearm input admitted");
        pump(e->owner);
        auto next = e->owner.state().timers.data[0];
        requireCodec(next.active && next.generation > input.armGeneration,
                     "real arm action advances generation");
        correlated(*e, input, first, 421);
        requireCodec(e->owner.state().timers.data[0].active &&
                         e->owner.state().timers.data[0].generation == next.generation,
                     "old consumed duplicate never consumes new arm");
        const auto cancel = localInput(w, 3, 2);
        requireCodec(e->owner.submit(cancel, id(422), request(422)).error == ev::Error::None,
                     "real cancel input admitted");
        pump(e->owner);
        requireCodec(!e->owner.state().timers.data[0].active, "actual cancel committed");
        auto stale = input;
        stale.armGeneration = next.generation;
        const auto staleResult = e->owner.submit(stale, id(423), request(423));
        requireCodec(staleResult.error != ev::Error::None &&
                         staleResult.journal.outcome != rb::Outcome::Duplicate,
                     "cancelled unconsumed arm never inherits earlier generation receipt");
        arm = localInput(w, 4);
        requireCodec(e->owner.submit(arm, id(424), request(424)).error == ev::Error::None,
                     "real postcancel arm admitted");
        pump(e->owner);
        next = e->owner.state().timers.data[0];
        requireCodec(next.active && next.generation > stale.armGeneration,
                     "cancel/rearm creates distinct timer identity");
        w.clock.ticks += next.deadline - e->owner.state().fence.activeTime;
        auto due = input;
        due.armGeneration = next.generation;
        requireCodec(e->owner.submit(due, id(425), request(425)).error == ev::Error::None,
                     "distinct genuinely due generation admitted");
        pump(e->owner);
        requireCodec(e->owner.status(w.program.program.context, id(425)).status ==
                             xr::ManualStatus::Committed &&
                         e->memory.work.intents.count == 2,
                     "second genuine timer generation commits separate effect");
    } else {
        auto fresh = input;
        fresh.bootGeneration++;
        requireCodec(e->owner.submit(fresh, id(430), request(430)).error == ev::Error::None,
                     "new synthetic local bootgeneration is distinct input");
        pump(e->owner);
        requireCodec(e->owner.status(w.program.program.context, id(430)).status ==
                             xr::ManualStatus::Committed &&
                         e->owner.state().latches.count == 1 && e->memory.work.intents.count == 1,
                     "new local generation receipt does not bypass once latch");
        fresh = input;
        fresh.sequence++;
        requireCodec(e->owner.submit(fresh, id(431), request(431)).error == ev::Error::None,
                     "new local sequence admitted");
        pump(e->owner);
        requireCodec(e->owner.status(w.program.program.context, id(431)).status ==
                             xr::ManualStatus::Committed &&
                         e->owner.state().fence.lastIssued == 1,
                     "new local sequence is not prior duplicate and issues no second once Fact");
    }
    w.sources.available = false;
    w.sources.saved.clear();
    const auto sourceCalls = w.sources.locals + w.sources.inputs + w.sources.lookups;
    e->store.loseSession();
    e.reset(new Engine(w, false));
    requireCodec(e->owner.recover().error == ev::Error::None,
                 "fresh backend session cold full semantic replay");
    requireCodec(w.sources.locals + w.sources.inputs + w.sources.lookups == sourceCalls,
                 "cold history uses no today's original source lookup");
    correlated(*e, input, first, 440);
    std::cout << "{\"checks\":" << checks << "}\n";
}
