#pragma once
#include "../manual-owner-review/Memory.h"
#include "DeviceRuntime/RulesProgressOwner.h"
// Requires existing audit CompilerSupport storage()/xr namespace. All storage is
// host-owned and exclusive; these capacities are NOT a proposed device budget.
namespace owner_review {
namespace runtime = larp::devices::rules_progress;
namespace codec = larp::execution::rules_progress;
struct Memory {
    runtime::Workspace work;
    ProgramMemory historical;
    StateMemory banks[3];
    PlannerMemory prepared, decoded, computed;
    std::vector<codec::SignedFact> preparedProofs, preparedOutbox, decodedProofs, decodedOutbox;
    std::vector<std::uint8_t> preparedBytes, decodedBytes, application, decodedApplication,
        comparison, history, physical;
    std::vector<runtime::CatalogEntry> catalogs;
    std::vector<runtime::ApplicationEntry> applications;
    std::vector<runtime::ControlEntry> controls;
    std::vector<runtime::IntentEntry> intents;
    std::vector<runtime::IntentReservation> reservations;
    Memory()
        : preparedBytes(32768), decodedBytes(32768), application(32768), decodedApplication(32768),
          comparison(32768), history(256 * 1024), physical(16 * 1024) {
        work.historicalProgram = historical.program;
        for (unsigned i = 0; i < 3; ++i)
            work.banks[i].storage = banks[i].pools;
        work.prepared.plan = prepared.plan;
        work.decoded.plan = decoded.plan;
        work.computed = computed.plan;
        work.planner = prepared.workspace;
        storage(work.prepared.proofs, preparedProofs, 256);
        storage(work.prepared.outbox, preparedOutbox, 8);
        storage(work.decoded.proofs, decodedProofs, 256);
        storage(work.decoded.outbox, decodedOutbox, 8);
        bind(work.preparedEvidence.bytes, preparedBytes);
        bind(work.decodedStorage.bytes, decodedBytes);
        bind(work.applicationBytes, application);
        bind(work.decodedApplication, decodedApplication);
        bind(work.comparison, comparison);
        bind(work.history, history);
        bind(work.physicalBytes, physical);
        storage(work.catalogs, catalogs, 63);
        storage(work.applications, applications, 63);
        storage(work.controls, controls, 63);
        storage(work.intents, intents, 32);
        storage(work.envelope.intents, reservations, 32);
    }
    template <class T>
    static void bind(larp::configuration::module_ir::Pool<T> &out, std::vector<T> &bytes) {
        out.data = bytes.data();
        out.capacity = bytes.size();
        out.count = 0;
    }
    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;
};
} // namespace owner_review
