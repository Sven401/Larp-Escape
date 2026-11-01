#pragma once
void statusFences() {
    for (bool wrong : {false, true}) {
        Fixture f;
        const auto p = f.prepare();
        f.transaction.outcome = xr::ManualStatus::Confirming;
        const auto pending = f.owner->confirm(f.confirmRequest(p));
        f.transaction.statusOutcome = xr::ManualStatus::Unknown;
        f.transaction.statusWrongOperation = wrong;
        if (!wrong)
            f.transaction.callback = [&]() {
                f.authority.outcome = xr::ManualAuthorityResult::Closed;
            };
        const auto r = f.owner->status(f.state.context, f.authority.actor, pending.operation);
        check(r.status == xr::ManualStatus::Rejected,
              "failed status disclosure fence returns no receipt");
        check(f.replay[0].status == xr::ManualStatus::Unknown,
              "unknown/incoherent status latches before failed disclosure");
        const auto calls = f.transaction.calls;
        check(f.owner->service().error == xr::ManualError::RecoveryRequired &&
                  f.transaction.calls == calls,
              "failed status disclosure cannot bypass global unknown");
    }
    {
        Fixture f;
        const auto p = f.prepare();
        const auto request = f.confirmRequest(p);
        const auto committed = f.owner->confirm(request);
        f.authority.actor.peer = id(0x7001);
        ++f.authority.actor.principal;
        ++f.authority.actor.roleEpoch;
        ++f.authority.actor.connectionToken;
        ++f.authority.actor.registration;
        ++f.authority.actor.lifecycle;
        ++f.authority.actor.registryRevision;
        ++f.authority.actor.eventRevision;
        check(equal(f.owner->status(f.state.context, f.authority.actor, committed.operation),
                    committed),
              "current new reader can read original terminal identity only");
        check(f.owner->confirm(request).status == xr::ManualStatus::Rejected,
              "old original authority never rebound to new reader");
        auto rebound = request;
        rebound.authority = f.authority.actor;
        check(f.owner->confirm(rebound).error == xr::ManualError::Conflict,
              "old token cannot be retargeted to new actor");
        f.authority.outcome = xr::ManualAuthorityResult::Closed;
        const auto denied =
            f.owner->status(f.state.context, f.authority.actor, committed.operation);
        check(denied.status == xr::ManualStatus::Rejected &&
                  equal(denied.operation, xr::Identity{}),
              "revoked status reader receives no terminal operation");
    }
    std::cout << "PASS status-fences\n";
}
void mutate(xr::OverrideRecord &r, unsigned field) {
    switch (field) {
    case 0:
        r.context.event.bytes[0] ^= 1;
        break;
    case 1:
        r.context.run.bytes[0] ^= 1;
        break;
    case 2:
        r.context.station.bytes[0] ^= 1;
        break;
    case 3:
        r.context.box.bytes[0] ^= 1;
        break;
    case 4:
        r.context.sourceEpoch.bytes[0] ^= 1;
        break;
    case 5:
        r.context.deployment.bytes[0] ^= 1;
        break;
    case 6:
        r.context.binding.bytes[0] ^= 1;
        break;
    case 7:
        r.context.content.bytes[0] ^= 1;
        break;
    case 8:
        ++r.context.bindingGeneration;
        break;
    case 9:
        r.program.bytes[0] ^= 1;
        break;
    case 10:
        ++r.expected.progress;
        break;
    case 11:
        ++r.expected.profile;
        break;
    case 12:
        ++r.expected.trust;
        break;
    case 13:
        ++r.expected.facts;
        break;
    case 14:
        ++r.expected.ledger;
        break;
    case 15:
        ++r.expected.resources;
        break;
    case 16:
        ++r.expected.rights;
        break;
    case 17:
        ++r.expected.capacity;
        break;
    case 18:
        ++r.expected.lastIssued;
        break;
    case 19:
        ++r.expected.activeTime;
        break;
    case 20:
        r.authority.peer.bytes[0] ^= 1;
        break;
    case 21:
        ++r.authority.principal;
        break;
    case 22:
        ++r.authority.roleEpoch;
        break;
    case 23:
        ++r.authority.connectionToken;
        break;
    case 24:
        ++r.authority.registration;
        break;
    case 25:
        ++r.authority.lifecycle;
        break;
    case 26:
        ++r.authority.connectionHandle;
        break;
    case 27:
        ++r.authority.registryRevision;
        break;
    case 28:
        ++r.authority.eventRevision;
        break;
    case 29:
        r.ownerBoot.bytes[0] ^= 1;
        break;
    case 30:
        r.token.bytes[0] ^= 1;
        break;
    case 31:
        r.prepareId.bytes[0] ^= 1;
        break;
    case 32:
        r.confirmId.bytes[0] ^= 1;
        break;
    case 33:
        r.operation.bytes[0] ^= 1;
        break;
    case 34:
        r.rule.bytes[0] ^= 1;
        break;
    case 35:
        r.prepareDigest.bytes[0] ^= 1;
        break;
    case 36:
        r.confirmDigest.bytes[0] ^= 1;
        break;
    case 37:
        ++r.original;
        break;
    case 38:
        ++r.replacement;
        break;
    case 39:
        ++r.originalGeneration;
        break;
    case 40:
        ++r.replacementGeneration;
        break;
    case 41:
        ++r.preparedAt;
        break;
    case 42:
        ++r.expiresAt;
        break;
    case 43:
        ++r.confirmedAt;
        break;
    default:
        check(false, "known record mutation");
    }
}
void finalRecords() {
    for (unsigned field = 0; field < 44; ++field) {
        Fixture f;
        const auto p = f.prepare();
        f.transaction.alter = [field](xr::OverrideRecord &r) { mutate(r, field); };
        check(f.owner->confirm(f.confirmRequest(p)).status == xr::ManualStatus::Rejected,
              "every original record field is exact at active Final");
        check(f.transaction.retained->check(f.transaction.captured) ==
                  xr::ManualAuthorityResult::Stale,
              "retained Final capability expires at callback return");
    }
    {
        Fixture f;
        const auto one = f.prepare(1), two = f.prepare(2);
        f.transaction.outcome = xr::ManualStatus::Confirming;
        check(f.owner->confirm(f.confirmRequest(one, 1)).status == xr::ManualStatus::Confirming,
              "first legitimate pending record");
        check(f.owner->confirm(f.confirmRequest(two, 2)).status == xr::ManualStatus::Confirming,
              "second legitimate pending record");
        const auto other = f.replay[1].record;
        f.transaction.alter = [&](xr::OverrideRecord &r) { r = other; };
        check(f.owner->service().status == xr::ManualStatus::Rejected &&
                  f.replay[1].status == xr::ManualStatus::Confirming,
              "legitimate other pending record cannot substitute actual active submit");
    }
    std::cout << "PASS final-record\n";
}
void reenter(Fixture &f) {
    const auto records = f.work.replays.count;
    const auto identities = f.ids.calls, submits = f.transaction.calls,
               statuses = f.transaction.statusCalls;
    xr::ManualView v;
    v.ownerBoot = id(0xdead);
    xr::ManualPrepareRequest p;
    p.ownerBoot = f.boot;
    p.phase = id(0xbeef);
    xr::ManualConfirmRequest c;
    c.ownerBoot = f.boot;
    c.phase = id(0xbefe);
    const auto one = f.owner->view(f.rule, f.authority.actor, v), two = f.owner->prepare(p),
               three = f.owner->confirm(c),
               four = f.owner->status(f.state.context, f.authority.actor, id(0xbada)),
               five = f.owner->service();
    for (const auto &r : {one, two, three, four, five})
        check(r.status == xr::ManualStatus::Deferred && r.error == xr::ManualError::Busy,
              "all callback reentries rejected Busy");
    check(equal(v.ownerBoot, id(0xdead)) && records == f.work.replays.count &&
              identities == f.ids.calls && submits == f.transaction.calls &&
              statuses == f.transaction.statusCalls,
          "reentry leaves original requests/work/output untouched");
}
void callbackReentry() {
    for (unsigned port = 0; port < 5; ++port) {
        Fixture f;
        const auto req = f.request();
        auto nested = [&]() { reenter(f); };
        if (port == 0)
            f.hash.callback = nested;
        if (port == 1)
            f.clock.callback = nested;
        if (port == 2)
            f.authority.callback = nested;
        if (port == 3)
            f.ids.callback = nested;
        const auto p = f.owner->prepare(req);
        check(p.status == xr::ManualStatus::Prepared,
              "prepare survives hash/clock/authority/identity reentry");
        if (port == 4)
            f.transaction.callback = nested;
        check(f.owner->confirm(f.confirmRequest(p)).status == xr::ManualStatus::Committed,
              "confirm survives transaction reentry");
    }
    {
        Fixture f;
        const auto p = f.prepare();
        f.transaction.outcome = xr::ManualStatus::Unknown;
        const auto u = f.owner->confirm(f.confirmRequest(p));
        f.transaction.callback = [&]() { reenter(f); };
        f.transaction.statusOutcome = xr::ManualStatus::Committed;
        check(f.owner->status(f.state.context, f.authority.actor, u.operation).status ==
                  xr::ManualStatus::Committed,
              "status callback reentry retains outer response");
    }
    std::cout << "PASS callback-reentry\n";
}
void capacitiesRetire() {
    Fixture f(30, 1, 2);
    const auto firstRequest = f.request(1);
    const auto first = f.owner->prepare(firstRequest);
    const auto secondRequest = f.request(2);
    const auto ids = f.ids.calls;
    check(f.owner->prepare(secondRequest).error == xr::ManualError::Capacity &&
              f.ids.calls == ids && f.work.replays.count == 1,
          "active capacity +1 no eviction/identity");
    f.clock.time = 41;
    const auto second = f.owner->prepare(secondRequest);
    check(second.status == xr::ManualStatus::Prepared && f.work.replays.count == 2 &&
              f.replay[0].status == xr::ManualStatus::Expired,
          "retire expired active slot while preserving replay identity");
    check(f.owner->prepare(firstRequest).status == xr::ManualStatus::Expired &&
              f.replay[0].record.expiresAt == first.expiresAt,
          "retired prepare replay never refreshes grant");
    f.clock.time = 72;
    const auto third = f.owner->prepare(f.request(3));
    check(third.error == xr::ManualError::Capacity && f.work.replays.count == 2,
          "full replay ledger no eviction even after active retirement");
    {
        Fixture overlap;
        overlap.work.catalogBytes.data = reinterpret_cast<std::uint8_t *>(overlap.replay.data());
        overlap.work.catalogBytes.capacity = overlap.replay.size() * sizeof(xr::ManualReplay);
        xr::ManualView view;
        check(overlap.owner->view(overlap.rule, overlap.authority.actor, view).error ==
                      xr::ManualError::InvalidArgument &&
                  overlap.authority.calls == 0 && overlap.ids.calls == 0,
              "overlapping workspace rejected before callbacks");
    }
    std::cout << "PASS capacities-retire\n";
}
void bootAndCopy() {
    {
        Fixture f;
        auto r = f.request();
        const auto saved = r;
        f.clock.callback = [&]() {
            r.phase = id(0xbad);
            r.rule = {};
            r.ownerBoot = {};
            r.authority = {};
        };
        const auto p = f.owner->prepare(r);
        check(p.status == xr::ManualStatus::Prepared &&
                  equal(f.replay[0].record.prepareId, saved.phase),
              "prepare copies complete input before callbacks");
        auto c = f.confirmRequest(p);
        const auto phase = c.phase;
        f.clock.callback = [&]() {
            c.phase = id(0xbad);
            c.token = {};
            c.authority = {};
        };
        check(f.owner->confirm(c).status == xr::ManualStatus::Committed &&
                  equal(f.transaction.captured.confirmId, phase),
              "confirm copies complete input before callbacks");
    }
    {
        Fixture f;
        const auto old = f.request();
        const auto p = f.owner->prepare(old);
        const auto confirm = f.confirmRequest(p);
        f.owner.reset();
        f.boot = id(0x5001);
        f.work.replays.count = 0;
        f.owner.reset(new xr::ManualOverrideOwner(f.program.program, f.state, f.boot, 30, f.hash,
                                                  f.clock, f.authority, f.ids, f.transaction,
                                                  f.work));
        check(f.owner->prepare(old).status == xr::ManualStatus::Rejected &&
                  f.owner->confirm(confirm).status == xr::ManualStatus::Rejected,
              "old ownerBoot rejected after modeled RAM loss");
        auto retarget = old;
        retarget.ownerBoot = f.boot;
        check(f.owner->prepare(retarget).error == xr::ManualError::Stale,
              "old view cannot attach to new incarnation even with new outer Boot");
        auto token = confirm;
        token.ownerBoot = f.boot;
        check(f.owner->confirm(token).error == xr::ManualError::UnknownOperation,
              "old token cannot attach after RAM replay loss");
    }
    std::cout << "PASS boot-and-copy\n";
}
xr::Capacities capacities() {
    xr::Capacities c;
    c.receipts = 1024;
    c.facts = 256;
    c.latches = 64;
    c.outbox = c.intents = 8;
    return c;
}
void admissionScopes() {
    Fixture f;
    const auto p = f.prepare();
    check(f.owner->confirm(f.confirmRequest(p)).status == xr::ManualStatus::Committed,
          "actual private admission producer under explicit synthetic ports");
    xr::Input input;
    input.context = f.state.context;
    input.kind = xr::InputKind::Override;
    input.overrideAdmission = f.transaction.copied;
    PlannerMemory live;
    check(live.planner.plan(f.program.program, f.state, input, capacities(), live.plan,
                            live.workspace)
                      .decision == xr::Decision::Ready &&
              live.plan.emissions.count &&
              live.plan.emissions.data[0].type == xr::FactType::LocalOverride,
          "actual Owner-produced live admission reaches actual T2 pure planner only");
    PlannerMemory historical;
    StateMemory inactive;
    xr::State recovered;
    auto stored = input;
    stored.overrideAdmission = {};
    const auto replay = historical.planner.replayHistorical(
        f.program.program, f.state, stored, &f.transaction.captured, capacities(), historical.plan,
        historical.workspace, inactive.pools, recovered);
    check(replay.scope == xr::ReplayScope::ReplayedInactive && !historical.plan.ready &&
              historical.plan.receipt.input.overrideAdmission.scope() ==
                  xr::OverrideAdmissionScope::Historical,
          "actual pure historical producer yields inactive result, not new admission proof");
    PlannerMemory normal;
    const auto oldInput = historical.plan.receipt.input;
    const auto historicalLive = normal.planner.plan(f.program.program, recovered, oldInput,
                                                    capacities(), normal.plan, normal.workspace);
    check(historicalLive.decision == xr::Decision::Rejected &&
              historicalLive.error == xr::Error::Context && !normal.plan.ready,
          "Historical scope rejected in Live path before matching historical ledger");
    auto local = oldInput;
    local.kind = xr::InputKind::Local;
    const auto scopedLocal = normal.planner.plan(f.program.program, recovered, local, capacities(),
                                                 normal.plan, normal.workspace);
    check(scopedLocal.decision == xr::Decision::Rejected && scopedLocal.error == xr::Error::Context,
          "Historical scope rejected before nonoverride input validation");
    local.overrideAdmission = {};
    const auto emptyLocal = normal.planner.plan(f.program.program, recovered, local, capacities(),
                                                normal.plan, normal.workspace);
    check(emptyLocal.decision == xr::Decision::Rejected && emptyLocal.error == xr::Error::Reference,
          "control with Empty admission reaches ordinary input validation");
    std::cout << "PASS admission-scopes PURE_SYNTHETIC_AUTHORITY_NO_HISTORICAL_D1_PROOF\n";
}
void finalClock() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        Fixture f;
        const auto p = f.prepare();
        f.clock.time = 39;
        f.transaction.callback = [&]() {
            if (mode == 0)
                f.clock.time = 40;
            if (mode == 1)
                f.clock.time = 41;
            if (mode == 2)
                f.clock.available = false;
        };
        const auto r = f.owner->confirm(f.confirmRequest(p));
        check(r.status == (mode == 0 ? xr::ManualStatus::Committed : xr::ManualStatus::Rejected),
              "fresh actual Final clock inclusive / expired / unavailable");
    }
    std::cout << "PASS final-clock\n";
}
void hashIdentities() {
    for (int mode = 1; mode <= 5; ++mode) {
        Fixture f;
        const auto request = f.request();
        f.hash.failure = mode;
        check(f.owner->prepare(request).status != xr::ManualStatus::Prepared &&
                  f.work.replays.count == 0 && f.transaction.calls == 0,
              "real hash failure/length/abort before grant");
        f.hash.failure = 0;
        check(f.owner->prepare(request).status == xr::ManualStatus::Prepared,
              "hash failure healthy reuse");
    }
    for (bool zero : {false, true}) {
        Fixture f;
        const auto r = f.request();
        f.ids.available = zero;
        f.ids.zero = zero;
        check(f.owner->prepare(r).error == xr::ManualError::Identity && f.ids.calls == 1 &&
                  f.work.replays.count == 0,
              "identity failure/zero no retry loop");
    }
    std::cout << "PASS hash-identities\n";
}

void ramRelease() {
    Fixture f;
    const auto winner = f.prepare(1), waiting = f.prepare(2);
    check(f.owner->confirm(f.confirmRequest(winner, 1)).status == xr::ManualStatus::Committed,
          "synthetic transaction supplies actual owner admission for RAM-only release");
    xr::Input input;
    input.kind = xr::InputKind::Override;
    input.context = f.state.context;
    input.overrideAdmission = f.transaction.copied;
    PlannerMemory runtime;
    check(runtime.planner
                  .plan(f.program.program, f.state, input, capacities(), runtime.plan,
                        runtime.workspace)
                  .decision == xr::Decision::Ready,
          "actual pure planner release");
    StateMemory nextBank;
    xr::State next;
    check(xr::Planner::commitRam(f.program.program, f.state, runtime.plan, nextBank.pools, next)
                      .decision == xr::Decision::Ready &&
              next.latches.count == 1 && next.solved.count == 1,
          "actual copied RAM state has shared release latch");
    f.state = next;
    const auto calls = f.transaction.calls, identities = f.ids.calls;
    const auto request = f.confirmRequest(waiting, 2);
    const auto released = f.owner->confirm(request);
    check(released.status == xr::ManualStatus::AlreadyReleased && f.transaction.calls == calls &&
              f.ids.calls == identities,
          "real shared latch wins pending grant without second submit or operation");
    check(equal(f.replay[1].record.confirmId, request.phase),
          "first valid AlreadyReleased confirm is bound");
    f.clock.time = 100;
    check(equal(f.owner->confirm(request), released),
          "known AlreadyReleased terminal reply stays identical after expiry");
    auto changed = request;
    changed.phase = id(0x2003);
    check(f.owner->confirm(changed).error == xr::ManualError::Conflict,
          "changed valid terminal confirm phase conflicts");
    std::cout << "PASS ram-release PURE_RAM_NO_D1_OR_AUTHORITY_PROOF\n";
}

void releaseDisposition() {
    using D = xr::ReleaseDisposition;
    for (auto d : {D::Released, D::NoRelease, D::Unresolved}) {
        for (auto status : {xr::ManualStatus::Committed, xr::ManualStatus::Duplicate}) {
            Fixture f;
            f.transaction.release = d;
            f.transaction.outcome = status;
            const auto p = f.prepare();
            const auto request = f.confirmRequest(p);
            const auto result = f.owner->confirm(request);
            check(result.status == status && result.release == d,
                  "durability preserves separate transaction release disposition");
            f.transaction.release = d == D::Released ? D::NoRelease : D::Released;
            check(equal(f.owner->confirm(request), result) &&
                      equal(f.owner->status(f.state.context, f.authority.actor, result.operation),
                            result),
                  "terminal duplicate/status cannot revise original release answer");
            const auto calls = f.transaction.calls;
            f.owner->service();
            check(f.transaction.calls == calls, "terminal receipt never resubmitted");
            f.authority.outcome = xr::ManualAuthorityResult::Closed;
            check(f.owner->status(f.state.context, f.authority.actor, result.operation).release ==
                          D::Unresolved &&
                      f.replay[0].release == d,
                  "revoked reader receives no release claim and stored disposition is unchanged");
        }
        for (bool service : {false, true}) {
            Fixture f;
            f.transaction.outcome = xr::ManualStatus::Confirming;
            const auto p = f.prepare();
            const auto pending = f.owner->confirm(f.confirmRequest(p));
            check(pending.release == D::Unresolved, "pending never claims release");
            f.transaction.release = d;
            f.transaction.outcome = f.transaction.statusOutcome = xr::ManualStatus::Committed;
            const auto final =
                service ? f.owner->service()
                        : f.owner->status(f.state.context, f.authority.actor, pending.operation);
            check(final.status == xr::ManualStatus::Committed && final.release == d &&
                      equal(final.operation, pending.operation),
                  "status/service adopt copied original release disposition");
        }
    }
    Fixture f;
    // Actual false fallback guard, then a fresh actual catalog SHA taken by Owner.
    Id guard = 0;
    for (std::size_t i = 0; i < f.program.program.rules.count; ++i)
        if (f.program.program.rules.data[i].ir.trigger == 4)
            guard = f.program.program.rules.data[i].ir.guard;
    check(guard && guard <= f.program.program.nodes.count, "actual fallback guard present");
    const auto saved = f.program.program.nodes.data[guard - 1];
    auto &node = f.program.program.nodes.data[guard - 1];
    node.op = Op::Literal;
    node.type = Type::Boolean;
    node.value = 0;
    node.count = 0;
    node.ref = 0;
    for (auto &child : node.children)
        child = 0;
    const auto noFire = f.prepare(1), waiting = f.prepare(2);
    f.transaction.release = D::NoRelease;
    const auto request = f.confirmRequest(noFire, 1);
    const auto receipt = f.owner->confirm(request);
    xr::Input input;
    input.context = f.state.context;
    input.kind = xr::InputKind::Override;
    input.overrideAdmission = f.transaction.copied;
    PlannerMemory runtime;
    check(runtime.planner
                      .plan(f.program.program, f.state, input, capacities(), runtime.plan,
                            runtime.workspace)
                      .decision == xr::Decision::Ready &&
              runtime.plan.emissions.count == 0 && runtime.plan.latches.count == 0,
          "actual false guard yields ready receipt without replacement output/latch");
    check(receipt.status == xr::ManualStatus::Committed && receipt.release == D::NoRelease,
          "committed synthetic no-fire receipt never claims release");
    // Model a later original group's release in a copied real RAM state.
    xr::Latch latch;
    latch.rule = f.rule;
    f.state.latches.data = &latch;
    f.state.latches.count = 1;
    check(
        equal(f.owner->confirm(request), receipt) &&
            equal(f.owner->status(f.state.context, f.authority.actor, receipt.operation), receipt),
        "later original latch cannot relabel an old committed NoRelease receipt");
    const auto already = f.owner->confirm(f.confirmRequest(waiting, 2));
    check(already.status == xr::ManualStatus::AlreadyReleased && already.release == D::Released,
          "new AlreadyReleased refers to existing group without a new T2 claim");
    f.program.program.nodes.data[guard - 1] = saved;
    std::cout << "PASS release-disposition SYNTHETIC_TRANSACTION_NO_HISTORICAL_PRODUCER\n";
}
