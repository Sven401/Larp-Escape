#include "Companion/EventTransport.h"

#include <algorithm>
#include <limits>

namespace larp {
namespace companion {
namespace event_transport {
namespace {
bool nonzero(const Id &id) {
    for (const auto byte : id)
        if (byte != 0)
            return true;
    return false;
}
bool valid(const Fact &fact) {
    return nonzero(fact.id.event) && nonzero(fact.id.origin) && nonzero(fact.id.epoch) &&
           fact.id.sequence != 0 && nonzero(fact.run);
}
bool same(const Fact &left, const Fact &right) {
    return left.id == right.id && left.run == right.run && left.digest == right.digest;
}
bool bytesValid(const std::uint8_t *bytes, std::size_t size) {
    return bytes != nullptr && size != 0 && size <= MessageLimit;
}
bool durableStage(Stage stage) {
    return stage == Stage::Durable || stage == Stage::Applied || stage == Stage::Observed;
}
} // namespace

bool operator==(const FactId &left, const FactId &right) {
    return left.event == right.event && left.origin == right.origin && left.epoch == right.epoch &&
           left.sequence == right.sequence;
}
Core::Core(OriginVerifier &verifier, DurablePort &durable)
    : verifier_(verifier), durable_(durable) {}

Admission Core::enqueue(const std::uint8_t *bytes, std::size_t size, const Id &target) {
    if (!bytesValid(bytes, size) || !nonzero(target))
        return Admission::Invalid;
    Fact fact;
    const auto verification = verifier_.verifyFact(bytes, size, fact);
    if (verification == Verification::Unavailable)
        return Admission::Unavailable;
    if (verification != Verification::Verified || !valid(fact))
        return Admission::Unauthorized;
    for (auto &slot : outgoing_) {
        if (slot.message.size != 0 && slot.fact.id == fact.id) {
            if (!same(slot.fact, fact)) {
                // Quarantine every target for this conflicting identity.
                for (auto &other : outgoing_)
                    if (other.message.size != 0 && other.fact.id == fact.id) {
                        other.blocked = true;
                        other.conflict = true;
                        other.token = 0;
                    }
                return Admission::Conflict;
            }
            if (slot.blocked)
                return slot.conflict ? Admission::Conflict : Admission::Unauthorized;
        }
    }
    const auto publication = durable_.mayPublish(fact, target, bytes, size);
    if (publication == Publication::Unavailable)
        return Admission::Unavailable;
    if (publication != Publication::Durable)
        return Admission::Unauthorized;
    for (const auto &slot : outgoing_)
        if (slot.message.size != 0 && slot.fact.id == fact.id && slot.target == target)
            return slot.blocked ? Admission::Conflict : Admission::Duplicate;
    for (auto &slot : outgoing_) {
        if (slot.message.size == 0) {
            slot.message.next = 0;
            slot.message.delay = DispatchIntervalMs;
            slot.token = 0;
            slot.blocked = false;
            slot.conflict = false;
            std::copy_n(bytes, size, slot.message.bytes.begin());
            slot.message.size = size;
            slot.fact = fact;
            slot.target = target;
            return Admission::Queued;
        }
    }
    return Admission::Full;
}

Admission Core::receive(const std::uint8_t *bytes, std::size_t size) {
    if (!bytesValid(bytes, size))
        return Admission::Invalid;
    for (auto &slot : incoming_) {
        if (slot.size == 0) {
            slot.next = 0;
            slot.delay = DispatchIntervalMs;
            std::copy_n(bytes, size, slot.bytes.begin());
            slot.size = size;
            return Admission::Queued;
        }
    }
    return Admission::Full;
}

bool Core::time(std::uint64_t now) {
    if (now < lastTime_ || now > std::numeric_limits<std::uint64_t>::max() - RetryLimitMs)
        return false;
    lastTime_ = now;
    return true;
}
bool Core::schedule(Message &message, std::uint64_t now) {
    if (now > std::numeric_limits<std::uint64_t>::max() - message.delay)
        return false;
    message.next = now + message.delay;
    message.delay = std::min(message.delay * 2, RetryLimitMs);
    return true;
}

Poll Core::poll(std::uint64_t now, Bearer bearer, bool contact, Attempt &attempt) {
    attempt.size = 0;
    attempt.token = 0;
    if (!time(now))
        return Poll::ClockError;
    if (!contact || now < nextSend_)
        return Poll::Idle;
    if (bearer != Bearer::EspNow && bearer != Bearer::Wlan)
        return Poll::Blocked;
    if (token_ == std::numeric_limits<std::uint64_t>::max())
        return Poll::Exhausted;
    for (std::size_t count = 0; count < QueueCapacity; ++count) {
        const auto index = (nextOut_ + count) % QueueCapacity;
        auto &slot = outgoing_[index];
        if (slot.message.size == 0 || slot.blocked || now < slot.message.next)
            continue;
        nextOut_ = (index + 1) % QueueCapacity;
        nextSend_ = now + DispatchIntervalMs;
        slot.token = 0;
        if (!schedule(slot.message, now))
            return Poll::ClockError;
        Fact current;
        // Revalidate before EACH attempt, including a bearer change.
        const auto verification =
            verifier_.verifyFact(slot.message.bytes.data(), slot.message.size, current);
        if (verification == Verification::Unavailable)
            return Poll::Blocked;
        if (verification != Verification::Verified || !same(current, slot.fact)) {
            slot.blocked = true;
            return Poll::Blocked;
        }
        const auto publication = durable_.mayPublish(slot.fact, slot.target,
                                                     slot.message.bytes.data(), slot.message.size);
        if (publication == Publication::Unavailable)
            return Poll::Blocked;
        if (publication != Publication::Durable) {
            slot.blocked = true;
            return Poll::Blocked;
        }
        slot.token = ++token_;
        std::copy_n(slot.message.bytes.begin(), slot.message.size, attempt.bytes.begin());
        attempt.size = slot.message.size;
        attempt.fact = slot.fact;
        attempt.target = slot.target;
        attempt.bearer = bearer;
        attempt.token = slot.token;
        return Poll::Ready;
    }
    return Poll::Idle;
}

bool Core::linkResult(std::uint64_t token, LinkResult result) {
    if (token == 0 || (result != LinkResult::Accepted && result != LinkResult::Failed))
        return false;
    for (auto &slot : outgoing_) {
        if (slot.message.size != 0 && slot.token == token) {
            slot.token = 0; // At most one diagnostic completion for this attempt.
            return true;    // Neither success nor failure changes the retry deadline.
        }
    }
    return false;
}

Poll Core::processIncoming(std::uint64_t now, IncomingResult &result) {
    result = IncomingResult{};
    if (!time(now))
        return Poll::ClockError;
    if (now < nextReceive_)
        return Poll::Idle;
    for (std::size_t count = 0; count < QueueCapacity; ++count) {
        const auto index = (nextIn_ + count) % QueueCapacity;
        auto &slot = incoming_[index];
        if (slot.size == 0 || now < slot.next)
            continue;
        nextIn_ = (index + 1) % QueueCapacity;
        nextReceive_ = now + DispatchIntervalMs;
        result.verification = verifier_.verifyFact(slot.bytes.data(), slot.size, result.fact);
        if (result.verification == Verification::Unavailable) {
            result.fact = Fact{};
            if (!schedule(slot, now))
                return Poll::ClockError;
            return Poll::Blocked;
        }
        if (result.verification != Verification::Verified || !valid(result.fact)) {
            slot.size = 0;
            result.fact = Fact{};
            if (result.verification == Verification::Verified)
                result.verification = Verification::Rejected;
            return Poll::Blocked;
        }
        result.acceptance = durable_.accept(result.fact, slot.bytes.data(), slot.size);
        if (result.acceptance == Acceptance::Deferred ||
            result.acceptance == Acceptance::Unavailable) {
            if (!schedule(slot, now))
                return Poll::ClockError;
        } else {
            slot.size = 0;
        }
        return Poll::Ready;
    }
    return Poll::Idle;
}

ReceiptResult Core::receipt(std::uint64_t now, const std::uint8_t *bytes, std::size_t size) {
    if (!time(now))
        return ReceiptResult::ClockError;
    if (now < nextReceive_)
        return ReceiptResult::RateLimited;
    nextReceive_ = now + DispatchIntervalMs;
    if (!bytesValid(bytes, size))
        return ReceiptResult::Invalid;
    Receipt receipt;
    const auto verification = verifier_.verifyReceipt(bytes, size, receipt);
    if (verification == Verification::Unavailable)
        return ReceiptResult::Unavailable;
    if (verification != Verification::Verified || !valid(receipt.fact) ||
        !nonzero(receipt.receiptId))
        return ReceiptResult::Unauthorized;
    // D1 must see late and contradictory receipts even after slot release or
    // reconstruction. A volatile queue is not the authoritative receipt index.
    const auto committed = durable_.recordReceipt(receipt, bytes, size);
    if (committed == ReceiptCommit::Unavailable)
        return ReceiptResult::Unavailable;
    if (committed == ReceiptCommit::Unmatched)
        return ReceiptResult::Unmatched;
    for (auto &slot : outgoing_) {
        if (slot.message.size == 0 || !same(slot.fact, receipt.fact) ||
            slot.target != receipt.confirmer || slot.fact.id.origin != receipt.recipient)
            continue;
        if (committed == ReceiptCommit::Conflict) {
            slot.blocked = true;
            slot.conflict = true;
            slot.token = 0;
            return ReceiptResult::Conflict;
        }
        if (!slot.blocked && committed == ReceiptCommit::TargetDurable &&
            durableStage(receipt.stage)) {
            slot.message.size = 0;
            slot.token = 0;
            return ReceiptResult::Released; // Volatile scheduling only; never D1 data.
        }
        return ReceiptResult::Retained;
    }
    return committed == ReceiptCommit::Conflict ? ReceiptResult::Conflict
                                                : ReceiptResult::Unmatched;
}
std::size_t Core::outgoingCount() const {
    std::size_t count = 0;
    for (const auto &slot : outgoing_)
        count += slot.message.size != 0;
    return count;
}
std::size_t Core::incomingCount() const {
    std::size_t count = 0;
    for (const auto &slot : incoming_)
        count += slot.size != 0;
    return count;
}

} // namespace event_transport
} // namespace companion
} // namespace larp
