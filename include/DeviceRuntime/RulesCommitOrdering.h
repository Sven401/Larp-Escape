#pragma once
#include "DeviceRuntime/ProgressJournal.h"
#include "GameExecution/RulesPlanner.h"
namespace larp {
namespace devices {
namespace rules_progress {
namespace rules = execution::rules;
enum class CommitKind : std::uint8_t { Application = 1, Control = 2, Claim = 3, Result = 4 };
struct CommitBinding {
    CommitKind kind = CommitKind::Application;
    rules::Identity operation, parent;
    rules::Context context;
    rules::Fence expected;
    rules::Digest request, catalog, commitment;
};
struct OrderingLease {
    CommitBinding binding;
    rules::Identity boot;
    std::uint64_t generation = 0;
};
enum class Ordering : std::uint8_t { Held, Denied, Unavailable };
enum class OrderingResolution : std::uint8_t { Resolved, Stale, Unavailable };
enum class OrderingInventory : std::uint8_t { Empty, Held, Unavailable };
struct OrderingSnapshot {
    OrderingInventory inventory = OrderingInventory::Unavailable;
    rules::Identity boot;
    std::uint64_t generation = 0; // Recovery pin, distinct from the old held lease.
    OrderingLease lease;
};
bool sameCommitBinding(const CommitBinding &, const CommitBinding &);
bool sameOrderingLease(const OrderingLease &, const OrderingLease &);
bool sameOrderingSnapshot(const OrderingSnapshot &, const OrderingSnapshot &);
class ICommitOrdering {
  public:
    virtual ~ICommitOrdering() = default;
    // Serialized owner lease, never a spinlock across IO. Denied/Unavailable
    // acquire no lease. Held returns the exact immutable original binding.
    virtual Ordering hold(const CommitBinding &, OrderingLease &) = 0;
    // Exact Claim->Result phase handoff while continuously held. No Empty
    // interval, source callback, new actor or changed original parent scope.
    virtual Ordering transition(const OrderingLease &, const CommitBinding &, OrderingLease &) = 0;
    virtual OrderingResolution resolved(const OrderingLease &, ProgressOutcome) = 0;
    // Pins metadata closed for the ENTIRE qualified semantic reconstruction.
    // Positive Empty is an actual inventory with nonzero boot/generation, not
    // missing data. No Role/CurrentSolved/Running authority follows from it.
    virtual OrderingInventory beginRecovery(OrderingSnapshot &) = 0;
    virtual OrderingInventory readRecovery(OrderingSnapshot &) = 0;
    // Fallible exact-pin CAS BEFORE RAM publication. Keeps the pin closed.
    virtual OrderingResolution prepareRecovery(const OrderingSnapshot &, ProgressOutcome) = 0;
    // Only the supported exclusive serialized profile may implement this as
    // no-fail own metadata publication. No IO/queue/new authority here. A later
    // SDK/cross-task bridge must qualify that guarantee independently.
    virtual void publishRecovery(const OrderingSnapshot &) noexcept = 0;
    virtual void abortRecovery(const OrderingSnapshot &) noexcept = 0;
};
struct CommitMetadata {
    rules::Context context;
    rules::Fence fence;
};
enum class MetadataScope : std::uint8_t { Unavailable, Current };
class ICommitMetadata {
  public:
    virtual ~ICommitMetadata() = default;
    // Actual copied metadata, not user rights or caller-supplied admin flags.
    // All relevant external mutations obey this same serialized owner's gate.
    virtual MetadataScope read(CommitMetadata &) = 0;
};
// Concrete zero-heap single-worker state machine. It orders already separately
// authorized work; it never authenticates actors or activates a program. Boot
// comes from the actual metadata-owner lifecycle, not a display ID/hash fallback.
class SerializedOrdering final : public ICommitOrdering {
  public:
    SerializedOrdering(ICommitMetadata &source, const rules::Identity &boot)
        : source_(source), boot_(boot) {}
    SerializedOrdering(const SerializedOrdering &) = delete;
    SerializedOrdering &operator=(const SerializedOrdering &) = delete;
    Ordering hold(const CommitBinding &, OrderingLease &) override;
    Ordering transition(const OrderingLease &, const CommitBinding &, OrderingLease &) override;
    OrderingResolution resolved(const OrderingLease &, ProgressOutcome) override;
    OrderingInventory beginRecovery(OrderingSnapshot &) override;
    OrderingInventory readRecovery(OrderingSnapshot &) override;
    OrderingResolution prepareRecovery(const OrderingSnapshot &, ProgressOutcome) override;
    void publishRecovery(const OrderingSnapshot &) noexcept override;
    void abortRecovery(const OrderingSnapshot &) noexcept override;
    // Serialization diagnostic ONLY, never an authorization permit. Actual
    // registry/event/resource writers still check their original actor proofs.
    bool mutationsBlocked() const noexcept {
        return busy_ || held_ || recovering_ || recoveryRequired_;
    }

  private:
    ICommitMetadata &source_;
    rules::Identity boot_;
    std::uint64_t generation_ = 1;
    OrderingLease lease_;
    OrderingSnapshot pin_;
    bool busy_ = false, held_ = false, recovering_ = false;
    bool prepared_ = false, recoveryRequired_ = true;
};
} // namespace rules_progress
} // namespace devices
} // namespace larp
