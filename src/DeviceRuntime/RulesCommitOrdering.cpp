#include "DeviceRuntime/RulesCommitOrdering.h"
#include <cstring>
namespace larp {
namespace devices {
namespace rules_progress {
namespace {
template <typename T> bool equal(const T &a, const T &b) {
    return !std::memcmp(a.bytes, b.bytes, sizeof(a.bytes));
}
bool nonzero(const rules::Identity &id) {
    for (auto byte : id.bytes)
        if (byte)
            return true;
    return false;
}
struct Operation {
    bool &busy;
    explicit Operation(bool &value) : busy(value) {
        busy = true;
    }
    ~Operation() {
        busy = false;
    }
};
bool valid(const CommitBinding &b) {
    return nonzero(b.operation) && nonzero(b.context.event) && nonzero(b.context.run) &&
           nonzero(b.context.station) && nonzero(b.context.box) && nonzero(b.context.sourceEpoch) &&
           b.context.bindingGeneration &&
           ((b.kind == CommitKind::Application || b.kind == CommitKind::Control)
                ? !nonzero(b.parent)
                : ((b.kind == CommitKind::Claim || b.kind == CommitKind::Result) &&
                   nonzero(b.parent) && !equal(b.parent, b.operation)));
}
} // namespace
bool sameCommitBinding(const CommitBinding &a, const CommitBinding &b) {
    return a.kind == b.kind && equal(a.operation, b.operation) && equal(a.parent, b.parent) &&
           rules::sameContext(a.context, b.context) && rules::sameFence(a.expected, b.expected) &&
           equal(a.request, b.request) && equal(a.catalog, b.catalog) &&
           equal(a.commitment, b.commitment);
}
bool sameOrderingLease(const OrderingLease &a, const OrderingLease &b) {
    return equal(a.boot, b.boot) && a.generation == b.generation &&
           sameCommitBinding(a.binding, b.binding);
}
bool sameOrderingSnapshot(const OrderingSnapshot &a, const OrderingSnapshot &b) {
    return a.inventory == b.inventory && equal(a.boot, b.boot) && a.generation == b.generation &&
           (a.inventory != OrderingInventory::Held || sameOrderingLease(a.lease, b.lease));
}
Ordering SerializedOrdering::hold(const CommitBinding &input, OrderingLease &output) {
    const auto binding = input; // Copy before any callback/output alias reset.
    if (busy_ || recovering_ || recoveryRequired_ || !nonzero(boot_) || !valid(binding))
        return Ordering::Unavailable;
    Operation operation(busy_);
    if (held_) {
        if (!sameCommitBinding(binding, lease_.binding))
            return Ordering::Denied;
        output = lease_;
        return Ordering::Held;
    }
    CommitMetadata metadata;
    if (source_.read(metadata) != MetadataScope::Current)
        return Ordering::Unavailable;
    if (!rules::sameContext(metadata.context, binding.context) ||
        !rules::sameFence(metadata.fence, binding.expected))
        return Ordering::Denied;
    if (generation_ == UINT64_MAX)
        return Ordering::Unavailable;
    OrderingLease next;
    next.binding = binding;
    next.boot = boot_;
    next.generation = ++generation_;
    lease_ = next;
    held_ = true;
    output = next;
    return Ordering::Held;
}
Ordering SerializedOrdering::transition(const OrderingLease &input, const CommitBinding &requested,
                                        OrderingLease &output) {
    const auto original = input;
    const auto next = requested;
    if (busy_ || recovering_ || recoveryRequired_ || !held_ ||
        !sameOrderingLease(original, lease_) || !valid(next) ||
        original.binding.kind != CommitKind::Claim || next.kind != CommitKind::Result ||
        !equal(original.binding.parent, next.parent) ||
        !equal(original.binding.request, next.request) ||
        !equal(original.binding.catalog, next.catalog) ||
        !rules::sameContext(original.binding.context, next.context) ||
        !rules::sameFence(original.binding.expected, next.expected) ||
        equal(original.binding.operation, next.operation))
        return Ordering::Denied;
    if (generation_ == UINT64_MAX)
        return Ordering::Unavailable;
    // Pure in-place phase CAS. The gate remains held for every instruction.
    OrderingLease advanced;
    advanced.binding = next;
    advanced.boot = boot_;
    advanced.generation = ++generation_;
    lease_ = advanced;
    output = advanced;
    return Ordering::Held;
}
OrderingResolution SerializedOrdering::resolved(const OrderingLease &input,
                                                ProgressOutcome outcome) {
    const auto lease = input;
    if (busy_ || recovering_ || recoveryRequired_ || !held_ || !sameOrderingLease(lease, lease_))
        return OrderingResolution::Stale;
    if (outcome != ProgressOutcome::Committed && outcome != ProgressOutcome::Duplicate &&
        outcome != ProgressOutcome::Denied)
        return OrderingResolution::Unavailable;
    if (generation_ == UINT64_MAX)
        return OrderingResolution::Unavailable;
    held_ = false;
    lease_ = {};
    ++generation_;
    return OrderingResolution::Resolved;
}
OrderingInventory SerializedOrdering::beginRecovery(OrderingSnapshot &output) {
    if (busy_ || recovering_ || !nonzero(boot_) || generation_ == UINT64_MAX)
        return OrderingInventory::Unavailable;
    Operation operation(busy_);
    recovering_ = true;
    prepared_ = false;
    recoveryRequired_ = true;
    pin_ = {};
    pin_.inventory = held_ ? OrderingInventory::Held : OrderingInventory::Empty;
    pin_.boot = boot_;
    pin_.generation = ++generation_;
    if (held_)
        pin_.lease = lease_;
    output = pin_;
    return pin_.inventory;
}
OrderingInventory SerializedOrdering::readRecovery(OrderingSnapshot &output) {
    if (busy_ || !recovering_)
        return OrderingInventory::Unavailable;
    output = pin_;
    return pin_.inventory;
}
OrderingResolution SerializedOrdering::prepareRecovery(const OrderingSnapshot &input,
                                                       ProgressOutcome outcome) {
    const auto pin = input;
    if (busy_ || !recovering_ || prepared_ || !sameOrderingSnapshot(pin, pin_))
        return OrderingResolution::Stale;
    if (generation_ == UINT64_MAX ||
        (pin_.inventory == OrderingInventory::Empty
             ? outcome != ProgressOutcome::Recovered
             : (outcome != ProgressOutcome::Committed && outcome != ProgressOutcome::Denied)))
        return OrderingResolution::Unavailable;
    prepared_ = true;
    return OrderingResolution::Resolved;
}
void SerializedOrdering::publishRecovery(const OrderingSnapshot &pin) noexcept {
    if (busy_ || !recovering_ || !prepared_ || !sameOrderingSnapshot(pin, pin_))
        return;
    // prepareRecovery reserved this increment. No callback, IO or decision.
    held_ = false;
    lease_ = {};
    ++generation_;
    recovering_ = prepared_ = recoveryRequired_ = false;
    pin_ = {};
}
void SerializedOrdering::abortRecovery(const OrderingSnapshot &pin) noexcept {
    if (busy_ || !recovering_ || !sameOrderingSnapshot(pin, pin_))
        return;
    // Keep the old held lease unchanged; only the provisional resolution dies.
    prepared_ = recovering_ = false;
    recoveryRequired_ = true;
    pin_ = {};
}
} // namespace rules_progress
} // namespace devices
} // namespace larp
