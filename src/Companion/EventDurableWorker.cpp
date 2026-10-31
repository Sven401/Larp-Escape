#include "Companion/EventDurableWorker.h"

#include <algorithm>
#include <limits>

namespace larp {
namespace companion {
namespace event_transport {
namespace {
bool same(const Fact &a, const Fact &b) {
    return a.id == b.id && a.run == b.run && a.digest == b.digest;
}
bool same(const Receipt &a, const Receipt &b) {
    return same(a.fact, b.fact) && a.confirmer == b.confirmer && a.recipient == b.recipient &&
           a.receiptId == b.receiptId && a.stage == b.stage && a.reason == b.reason;
}
bool same(const WorkTicket &a, const WorkTicket &b) {
    return a.incarnation == b.incarnation && a.generation == b.generation &&
           a.sequence == b.sequence;
}
bool valid(Publication result) {
    return result == Publication::Unavailable || result == Publication::Denied ||
           result == Publication::Durable;
}
bool valid(Acceptance result) {
    return result == Acceptance::Unavailable || result == Acceptance::Rejected ||
           result == Acceptance::Deferred || result == Acceptance::Conflict ||
           result == Acceptance::Durable || result == Acceptance::AlreadyDurable;
}
bool valid(ReceiptCommit result) {
    return result == ReceiptCommit::Unavailable || result == ReceiptCommit::Unmatched ||
           result == ReceiptCommit::Retained || result == ReceiptCommit::Conflict ||
           result == ReceiptCommit::TargetDurable;
}
} // namespace

EventDurableWorker::EventDurableWorker(const Id &incarnation) : incarnation_(incarnation) {
    for (const auto byte : incarnation_)
        active_ = active_ || byte != 0;
    if (active_)
        status_ = HandoffStatus::Idle;
}

EventDurableWorker::Slot *EventDurableWorker::submit(WorkKind kind, const Fact &fact,
                                                     const Id &target, const Receipt &receipt,
                                                     const std::uint8_t *bytes, std::size_t size) {
    if (!active_) {
        status_ = HandoffStatus::Inactive;
        return nullptr;
    }
    if (exhausted_) {
        status_ = HandoffStatus::Exhausted;
        return nullptr;
    }
    if (bytes == nullptr || size == 0 || size > MessageLimit) {
        status_ = HandoffStatus::Invalid;
        return nullptr;
    }
    for (auto &slot : slots_) {
        const auto &request = slot.request;
        if (slot.state == State::Empty || request.kind != kind || !same(request.fact, fact) ||
            request.target != target || !same(request.receipt, receipt) || request.size != size ||
            !std::equal(bytes, bytes + size, request.bytes.begin()))
            continue;
        if (slot.state == State::Completed) {
            status_ = HandoffStatus::Completed;
            slot.state = State::Empty; // One-shot result, never an authority cache.
            return &slot;
        }
        status_ = HandoffStatus::Pending;
        return nullptr;
    }
    if (sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        exhausted_ = true;
        status_ = HandoffStatus::Exhausted;
        return nullptr;
    }
    for (auto &slot : slots_) {
        if (slot.state != State::Empty)
            continue;
        slot.request.kind = kind;
        slot.request.ticket.incarnation = incarnation_;
        slot.request.ticket.generation = generation_;
        slot.request.ticket.sequence = ++sequence_;
        slot.request.fact = fact;
        slot.request.target = target;
        slot.request.receipt = receipt;
        slot.request.size = size;
        std::copy_n(bytes, size, slot.request.bytes.begin());
        slot.publication = Publication::Unavailable;
        slot.acceptance = Acceptance::Unavailable;
        slot.receipt = ReceiptCommit::Unavailable;
        slot.state = State::Pending;
        status_ = HandoffStatus::Queued;
        return nullptr;
    }
    status_ = HandoffStatus::Full;
    return nullptr;
}

Publication EventDurableWorker::mayPublish(const Fact &fact, const Id &target,
                                           const std::uint8_t *bytes, std::size_t size) {
    const auto slot = submit(WorkKind::Publish, fact, target, Receipt{}, bytes, size);
    return slot == nullptr ? Publication::Unavailable : slot->publication;
}
Acceptance EventDurableWorker::accept(const Fact &fact, const std::uint8_t *bytes,
                                      std::size_t size) {
    const auto slot = submit(WorkKind::Accept, fact, Id{}, Receipt{}, bytes, size);
    return slot == nullptr ? Acceptance::Unavailable : slot->acceptance;
}
ReceiptCommit EventDurableWorker::recordReceipt(const Receipt &receipt, const std::uint8_t *bytes,
                                                std::size_t size) {
    const auto slot = submit(WorkKind::Receipt, Fact{}, Id{}, receipt, bytes, size);
    return slot == nullptr ? ReceiptCommit::Unavailable : slot->receipt;
}

bool EventDurableWorker::take(WorkRequest &request) {
    request.ticket = WorkTicket{};
    request.size = 0;
    if (!active_ || exhausted_)
        return false;
    for (std::size_t count = 0; count < WorkerCapacity; ++count) {
        const auto index = (nextTake_ + count) % WorkerCapacity;
        auto &slot = slots_[index];
        if (slot.state != State::Pending)
            continue;
        nextTake_ = (index + 1) % WorkerCapacity;
        slot.state = State::Running;
        request = slot.request;
        return true;
    }
    return false;
}
EventDurableWorker::Slot *EventDurableWorker::running(const WorkTicket &ticket, WorkKind kind) {
    if (!active_ || exhausted_ || ticket.incarnation != incarnation_ ||
        ticket.generation != generation_ || ticket.sequence == 0)
        return nullptr;
    for (auto &slot : slots_)
        if (slot.state == State::Running && slot.request.kind == kind &&
            same(slot.request.ticket, ticket))
            return &slot;
    return nullptr;
}
bool EventDurableWorker::complete(const WorkTicket &ticket, Publication result) {
    auto slot = running(ticket, WorkKind::Publish);
    if (slot == nullptr || !valid(result))
        return false;
    slot->publication = result;
    slot->state = State::Completed;
    return true;
}
bool EventDurableWorker::complete(const WorkTicket &ticket, Acceptance result) {
    auto slot = running(ticket, WorkKind::Accept);
    if (slot == nullptr || !valid(result))
        return false;
    slot->acceptance = result;
    slot->state = State::Completed;
    return true;
}
bool EventDurableWorker::complete(const WorkTicket &ticket, ReceiptCommit result) {
    auto slot = running(ticket, WorkKind::Receipt);
    if (slot == nullptr || !valid(result))
        return false;
    slot->receipt = result;
    slot->state = State::Completed;
    return true;
}
bool EventDurableWorker::isCurrent(const WorkTicket &ticket) const {
    if (!active_ || exhausted_ || ticket.incarnation != incarnation_ ||
        ticket.generation != generation_ || ticket.sequence == 0)
        return false;
    for (const auto &slot : slots_)
        if (slot.state == State::Running && same(slot.request.ticket, ticket))
            return true;
    return false;
}
bool EventDurableWorker::invalidate() {
    if (!active_)
        return false;
    for (auto &slot : slots_)
        slot.state = State::Empty;
    if (exhausted_ || generation_ == std::numeric_limits<std::uint64_t>::max()) {
        exhausted_ = true;
        status_ = HandoffStatus::Exhausted;
        return false;
    }
    ++generation_;
    status_ = HandoffStatus::Idle;
    return true;
}
std::size_t EventDurableWorker::outstanding() const {
    std::size_t count = 0;
    for (const auto &slot : slots_)
        count += slot.state != State::Empty;
    return count;
}
HandoffStatus EventDurableWorker::status() const {
    return status_;
}
bool EventDurableWorker::exhausted() const {
    return exhausted_;
}

ServiceResult serviceOne(EventDurableWorker &bridge, DurablePort &semanticPort) {
    if (&semanticPort == &bridge)
        return ServiceResult::CompletionRejected;
    WorkRequest request;
    if (!bridge.take(request))
        return ServiceResult::Idle;
    bool completed = false;
    switch (request.kind) {
    case WorkKind::Publish:
        completed = bridge.complete(request.ticket,
                                    semanticPort.mayPublish(request.fact, request.target,
                                                            request.bytes.data(), request.size));
        break;
    case WorkKind::Accept:
        completed = bridge.complete(
            request.ticket, semanticPort.accept(request.fact, request.bytes.data(), request.size));
        break;
    case WorkKind::Receipt:
        completed = bridge.complete(
            request.ticket,
            semanticPort.recordReceipt(request.receipt, request.bytes.data(), request.size));
        break;
    }
    return completed ? ServiceResult::Completed : ServiceResult::CompletionRejected;
}

} // namespace event_transport
} // namespace companion
} // namespace larp
