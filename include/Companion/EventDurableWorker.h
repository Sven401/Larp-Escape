#pragma once

#include "Companion/EventTransport.h"

namespace larp {
namespace companion {
namespace event_transport {

constexpr std::size_t WorkerCapacity = 4;
enum class WorkKind { Publish, Accept, Receipt };
struct WorkTicket {
    Id incarnation{};
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
};
struct WorkRequest {
    WorkKind kind = WorkKind::Publish;
    WorkTicket ticket{};
    Fact fact{};
    Id target{};
    Receipt receipt{};
    std::array<std::uint8_t, MessageLimit> bytes{};
    std::size_t size = 0;
};
enum class HandoffStatus { Inactive, Idle, Queued, Pending, Completed, Full, Invalid, Exhausted };

// Volatile copied handoff, not a journal or semantic D1 implementation.
// ALL accesses require external serialization; not thread/ISR safe or reentrant.
// take/complete must share the producer's serialization. Dispatch the copied
// request outside the time-critical D2 owner, never call flash from D2 methods.
// The owner supplies an incarnation never reused while an old request/result
// could survive. Null/default construction denies all work. This API neither
// generates nor persists that identity; lifecycle coordination remains external.
// Drain accesses before destruction, and use a fresh incarnation on reconstruction.
class EventDurableWorker : public DurablePort {
  public:
    Publication mayPublish(const Fact &, const Id &, const std::uint8_t *, std::size_t) override;
    Acceptance accept(const Fact &, const std::uint8_t *, std::size_t) override;
    ReceiptCommit recordReceipt(const Receipt &, const std::uint8_t *, std::size_t) override;

    // Copies one pending request; the occupied slot remains reserved until its
    // result is consumed by an exact producer retry, or invalidate() is called.
    bool take(WorkRequest &request);
    bool complete(const WorkTicket &, Publication);
    bool complete(const WorkTicket &, Acceptance);
    bool complete(const WorkTicket &, ReceiptCommit);
    bool isCurrent(const WorkTicket &) const;

    // Invalidate pending, running and completed results before ANY change that
    // makes an answer stale becomes visible: authority/context, D1 conflict
    // quarantine, backend recovery or lost readiness. Never touches D1 data.
    // Does not cancel a write:
    // the worker must check isCurrent AND real authority at the commit point,
    // with changes serialized until the write outcome is known. Not a substitute
    // for D1's missing cross-task authority fence/reducer.
    bool invalidate();
    std::size_t outstanding() const;
    // Last producer call/invalidation; take/complete do not overwrite it.
    HandoffStatus status() const;
    bool exhausted() const;

    explicit EventDurableWorker(const Id &incarnation = Id{});
    EventDurableWorker(const EventDurableWorker &) = delete;
    EventDurableWorker &operator=(const EventDurableWorker &) = delete;
    EventDurableWorker(EventDurableWorker &&) = delete;
    EventDurableWorker &operator=(EventDurableWorker &&) = delete;

  private:
    enum class State { Empty, Pending, Running, Completed };
    struct Slot {
        WorkRequest request{};
        State state = State::Empty;
        Publication publication = Publication::Unavailable;
        Acceptance acceptance = Acceptance::Unavailable;
        ReceiptCommit receipt = ReceiptCommit::Unavailable;
    };
    Slot *submit(WorkKind kind, const Fact &fact, const Id &target, const Receipt &receipt,
                 const std::uint8_t *bytes, std::size_t size);
    Slot *running(const WorkTicket &, WorkKind);
    std::array<Slot, WorkerCapacity> slots_{};
    Id incarnation_{};
    std::size_t nextTake_ = 0;
    std::uint64_t generation_ = 1;
    std::uint64_t sequence_ = 0;
    HandoffStatus status_ = HandoffStatus::Inactive;
    bool active_ = false;
    bool exhausted_ = false;
};

enum class ServiceResult { Idle, Completed, CompletionRejected };
// Convenience dispatcher for a serialized STORAGE-WORKER execution context.
// Calls the supplied semantic D1 port synchronously, never ProgressJournal
// directly. Default DurablePort returns Unavailable. No production port is
// supplied here; Journal::Committed alone cannot authorize any positive result.
// Multi-threaded integration must instead serialize take/complete separately and
// coordinate the actual commit fence; this helper installs no locks or tasks.
ServiceResult serviceOne(EventDurableWorker &bridge, DurablePort &semanticPort);

static_assert(sizeof(EventDurableWorker) <= 12 * 1024, "D2 worker handoff exceeds fixed budget");
static_assert(sizeof(WorkRequest) <= 3 * 1024, "D2 worker request exceeds copy budget");

} // namespace event_transport
} // namespace companion
} // namespace larp
