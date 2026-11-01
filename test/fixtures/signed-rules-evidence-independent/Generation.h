#pragma once
#include "GameExecution/ManualOverrideOwner.h"
// Actual admission producer with explicitly synthetic ports. This is a
// mathematical T2/generation boundary, never a GM/event/D1 authorization claim.
struct GenerationPorts final : xr::IManualClock,
                               xr::IManualAuthority,
                               xr::IManualIdentitySource,
                               xr::IManualTransaction {
    unsigned next = 0x94;
    xr::OverrideAdmission copied;
    bool now(std::uint64_t &n) override {
        n = 1000;
        return true;
    }
    bool make(xr::Identity &id) override {
        id = {};
        id.bytes[0] = static_cast<std::uint8_t>(next++);
        return true;
    }
    xr::ManualAuthorityResult check(const xr::Context &, const xr::OverrideAuthority &) override {
        return xr::ManualAuthorityResult::Current;
    }
    xr::ManualResult submit(const xr::OverrideAdmission &a, xr::IManualFinalFence &f) override {
        copied = a;
        xr::ManualResult r;
        r.operation = a.record().operation;
        r.status = f.check(a.record()) == xr::ManualAuthorityResult::Current
                       ? xr::ManualStatus::Committed
                       : xr::ManualStatus::Rejected;
        return r;
    }
    xr::ManualResult status(const xr::Context &, const xr::Identity &id) override {
        xr::ManualResult r;
        r.operation = id;
        r.status = xr::ManualStatus::Unknown;
        return r;
    }
};
int runGeneration() {
    std::string line;
    std::getline(std::cin, line);
    const auto raw = unhex(line);
    std::vector<std::uint8_t> bytes(raw.begin(), raw.end());
    ProgramMemory program;
    requireCodec(rp::decodeCatalog(view(bytes, bytes.size()), program.program).scope ==
                     rp::Scope::DecodedUnauthenticated,
                 "actual retained uint64-max binding catalog");
    requireCodec(program.program.context.bindingGeneration == UINT64_MAX,
                 "actual full binding generation from artifact bytes");
    StateMemory storageState;
    xr::State before;
    xr::Fence fence;
    fence.progress = fence.profile = fence.trust = fence.facts = fence.ledger = fence.resources =
        fence.rights = fence.capacity = 1;
    requireCodec(
        xr::Planner::initialize(program.program, fence, storageState.pools, before).decision ==
                xr::Decision::Ready &&
            xr::Planner::control(before, xr::Mode::Running).decision == xr::Decision::Ready,
        "actual T2 initial copied state");
    GenerationPorts ports;
    Hash hash;
    std::vector<xr::ManualReplay> replays(4);
    std::vector<std::uint8_t> catalog(32768);
    xr::ManualWorkspace work;
    storage(work.replays, replays, 4);
    storage(work.catalogBytes, catalog, 32768);
    work.maximumActive = 1;
    xr::Identity boot;
    boot.bytes[0] = 0x91;
    xr::ManualOverrideOwner owner(program.program, before, boot, 30, hash, ports, ports, ports,
                                  ports, work);
    xr::OverrideAuthority actor;
    actor.peer.bytes[0] = 0x61;
    actor.principal = actor.roleEpoch = actor.connectionToken = actor.registration =
        actor.lifecycle = actor.registryRevision = actor.eventRevision = 1;
    actor.connectionHandle = 1;
    xr::Identity rule;
    for (std::size_t i = 0; i < program.program.rules.count; ++i)
        if (program.program.rules.data[i].ir.policy == 3)
            rule = program.program.rules.data[i].stable;
    xr::ManualView v;
    requireCodec(owner.view(rule, actor, v).status == xr::ManualStatus::Prepared,
                 "actual Owner view under bounded synthetic policy");
    xr::ManualPrepareRequest request;
    request.ownerBoot = boot;
    request.phase.bytes[0] = 0x92;
    request.rule = rule;
    request.view = v.digest;
    request.authority = actor;
    const auto prepared = owner.prepare(request);
    requireCodec(prepared.status == xr::ManualStatus::Prepared, "actual Owner prepare");
    xr::ManualConfirmRequest confirm;
    confirm.ownerBoot = boot;
    confirm.token = prepared.token;
    confirm.phase.bytes[0] = 0x93;
    confirm.authority = actor;
    requireCodec(owner.confirm(confirm).status == xr::ManualStatus::Committed &&
                     ports.copied.scope() == xr::OverrideAdmissionScope::Live,
                 "only actual Owner creates live admission in bounded synthetic automaton");
    xr::Input input;
    input.context = before.context;
    input.kind = xr::InputKind::Override;
    input.overrideAdmission = ports.copied;
    PlannerMemory runtime;
    xr::Capacities caps;
    caps.receipts = 1024;
    caps.facts = 256;
    caps.latches = 64;
    caps.outbox = caps.intents = 8;
    requireCodec(
        runtime.planner.plan(program.program, before, input, caps, runtime.plan, runtime.workspace)
                    .decision == xr::Decision::Ready &&
            runtime.plan.emissions.count == 1 &&
            runtime.plan.emissions.data[0].type == xr::FactType::LocalOverride &&
            runtime.plan.emissions.data[0].generation != program.program.context.bindingGeneration,
        "actual T2 plan distinguishes rule generation from binding generation");
    SignedSet facts;
    captureSignedEvidence(program.program, before, runtime.plan, facts);
    std::cout << "{\"generationStats\":" << codecChecks << "}\n";
    return 0;
}
