#include "DeviceRuntime/RulesCommitOrdering.h"
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
namespace q = larp::devices::rules_progress;
namespace r = larp::execution::rules;
using larp::devices::ProgressOutcome;
namespace {
unsigned checks = 0;
void check(bool ok, const char *msg) {
    ++checks;
    if (!ok) {
        std::cerr << "FAIL " << msg << '\n';
        std::exit(1);
    }
}
r::Identity id(unsigned x) {
    r::Identity v;
    v.bytes[0] = static_cast<std::uint8_t>(x);
    return v;
}
q::CommitBinding binding() {
    q::CommitBinding b;
    b.operation = id(1);
    b.context.event = id(2);
    b.context.run = id(3);
    b.context.station = id(4);
    b.context.box = id(5);
    b.context.sourceEpoch = id(6);
    b.context.deployment.bytes[31] = 7;
    b.context.binding.bytes[31] = 8;
    b.context.content.bytes[31] = 9;
    b.context.bindingGeneration = UINT64_MAX;
    b.request.bytes[31] = 10;
    b.catalog.bytes[31] = 11;
    b.commitment.bytes[31] = 12;
    b.expected.progress = UINT64_MAX;
    b.expected.profile = UINT64_MAX - 1;
    b.expected.trust = 23;
    b.expected.facts = 24;
    b.expected.ledger = 255;
    b.expected.resources = 256;
    b.expected.rights = 65535;
    b.expected.capacity = 65536;
    b.expected.lastIssued = UINT32_MAX;
    b.expected.activeTime = UINT64_C(4294967296);
    return b;
}
struct Metadata final : q::ICommitMetadata {
    q::CommitMetadata value;
    q::MetadataScope scope = q::MetadataScope::Current;
    unsigned calls = 0;
    std::function<void()> callback;
    Metadata() {
        const auto b = binding();
        value.context = b.context;
        value.fence = b.expected;
    }
    q::MetadataScope read(q::CommitMetadata &out) override {
        ++calls;
        out = value;
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        return scope;
    }
};
struct Fixture {
    Metadata source;
    q::SerializedOrdering order;
    Fixture() : order(source, id(13)) {}
    void ready() {
        q::OrderingSnapshot p;
        check(order.beginRecovery(p) == q::OrderingInventory::Empty,
              "initial positive empty inventory");
        check(order.mutationsBlocked(), "initial recovery remains closed");
        check(order.prepareRecovery(p, ProgressOutcome::Recovered) ==
                  q::OrderingResolution::Resolved,
              "prepare positive empty replay");
        order.publishRecovery(p);
        check(!order.mutationsBlocked(), "only explicit publication opens empty owner");
    }
};
void alter(q::CommitBinding &b, unsigned n) {
    switch (n) {
    case 0:
        b.kind = q::CommitKind::Control;
        break;
    case 1:
        b.operation.bytes[15] ^= 1;
        break;
    case 2:
        b.parent = id(71);
        break;
    case 3:
        b.context.event.bytes[15] ^= 1;
        break;
    case 4:
        b.context.run.bytes[15] ^= 1;
        break;
    case 5:
        b.context.station.bytes[15] ^= 1;
        break;
    case 6:
        b.context.box.bytes[15] ^= 1;
        break;
    case 7:
        b.context.sourceEpoch.bytes[15] ^= 1;
        break;
    case 8:
        b.context.deployment.bytes[31] ^= 1;
        break;
    case 9:
        b.context.binding.bytes[31] ^= 1;
        break;
    case 10:
        b.context.content.bytes[31] ^= 1;
        break;
    case 11:
        --b.context.bindingGeneration;
        break;
    case 12:
        --b.expected.progress;
        break;
    case 13:
        --b.expected.profile;
        break;
    case 14:
        ++b.expected.trust;
        break;
    case 15:
        ++b.expected.facts;
        break;
    case 16:
        ++b.expected.ledger;
        break;
    case 17:
        ++b.expected.resources;
        break;
    case 18:
        ++b.expected.rights;
        break;
    case 19:
        ++b.expected.capacity;
        break;
    case 20:
        ++b.expected.lastIssued;
        break;
    case 21:
        ++b.expected.activeTime;
        break;
    case 22:
        b.request.bytes[31] ^= 1;
        break;
    case 23:
        b.catalog.bytes[31] ^= 1;
        break;
    case 24:
        b.commitment.bytes[31] ^= 1;
        break;
    }
}
void leaseMutate(q::OrderingLease &l, unsigned n) {
    if (n == 0)
        l.boot.bytes[15] ^= 1;
    else if (n == 1)
        ++l.generation;
    else
        alter(l.binding, n - 2);
}
void exact() {
    Fixture f;
    auto b = binding();
    q::OrderingLease out;
    out.generation = 999;
    check(f.order.hold(b, out) == q::Ordering::Unavailable && out.generation == 999 &&
              !f.source.calls,
          "startup closed, output not a permit");
    f.ready();
    q::OrderingLease first;
    check(f.order.hold(b, first) == q::Ordering::Held && first.generation &&
              q::sameCommitBinding(first.binding, b),
          "all actual full width binding copied");
    check(f.order.mutationsBlocked() && f.source.calls == 1,
          "metadata held after exact copied snapshot");
    f.source.scope = q::MetadataScope::Unavailable;
    q::OrderingLease same;
    check(f.order.hold(b, same) == q::Ordering::Held && q::sameOrderingLease(first, same) &&
              f.source.calls == 1,
          "same live lease immutable without current metadata substitution");
    for (unsigned i = 0; i < 25; ++i) {
        auto changed = b;
        alter(changed, i);
        check(!q::sameCommitBinding(b, changed), "every binding field participates in equality");
        q::OrderingLease untouched;
        untouched.generation = 987;
        const auto result = f.order.hold(changed, untouched);
        check(result != q::Ordering::Held && untouched.generation == 987,
              "same operation cannot rebind any field");
    }
    for (unsigned i = 0; i < 27; ++i) {
        auto stale = first;
        leaseMutate(stale, i);
        check(!q::sameOrderingLease(stale, first) &&
                  f.order.resolved(stale, ProgressOutcome::Committed) ==
                      q::OrderingResolution::Stale &&
                  f.order.mutationsBlocked(),
              "stale mutated lease cannot release metadata");
    }
    check(f.order.resolved(first, ProgressOutcome::Committed) == q::OrderingResolution::Resolved &&
              !f.order.mutationsBlocked(),
          "exact committed lease resolves");
    f.source.scope = q::MetadataScope::Current;
    q::OrderingLease second;
    check(f.order.hold(b, second) == q::Ordering::Held && second.generation > first.generation,
          "same operation reacquired only with fresh generation");
    check(f.order.resolved(first, ProgressOutcome::Denied) == q::OrderingResolution::Stale &&
              f.order.mutationsBlocked(),
          "old lease cannot resolve same op new generation");
    check(f.order.resolved(second, ProgressOutcome::Duplicate) == q::OrderingResolution::Resolved,
          "duplicate exact lease resolves");
}
void metadata() {
    for (unsigned i = 3; i <= 21; ++i) {
        Fixture f;
        f.ready();
        auto changed = binding();
        alter(changed, i);
        f.source.value.context = changed.context;
        f.source.value.fence = changed.expected;
        q::OrderingLease out;
        check(f.order.hold(binding(), out) == q::Ordering::Denied && !f.order.mutationsBlocked(),
              "exact current context/fence mismatch denied without hold");
    }
    {
        Fixture f;
        f.ready();
        f.source.scope = q::MetadataScope::Unavailable;
        q::OrderingLease x;
        check(f.order.hold(binding(), x) == q::Ordering::Unavailable && !f.order.mutationsBlocked(),
              "unavailable is not denied/held");
        f.source.scope = q::MetadataScope::Current;
        check(f.order.hold(binding(), x) == q::Ordering::Held,
              "healthy reuse after missing metadata");
    }
    for (unsigned kind = 1; kind <= 5; ++kind) {
        Fixture f;
        f.ready();
        auto b = binding();
        b.kind = static_cast<q::CommitKind>(kind);
        if (kind == 3 || kind == 4)
            b.parent = id(8);
        q::OrderingLease lease;
        check((f.order.hold(b, lease) == q::Ordering::Held) == (kind <= 4),
              "supported binding kinds and parent shape only");
    }
    for (unsigned i = 0; i < 8; ++i) {
        Fixture f;
        f.ready();
        auto b = binding();
        if (i == 0)
            b.operation = {};
        if (i == 1)
            b.context.event = {};
        if (i == 2)
            b.context.run = {};
        if (i == 3)
            b.context.station = {};
        if (i == 4)
            b.context.box = {};
        if (i == 5)
            b.context.sourceEpoch = {};
        if (i == 6)
            b.context.bindingGeneration = 0;
        if (i == 7) {
            b.kind = q::CommitKind::Claim;
            b.parent = b.operation;
        }
        q::OrderingLease lease;
        check(f.order.hold(b, lease) == q::Ordering::Unavailable && f.source.calls == 0,
              "invalid identity shape before metadata callback");
    }
    Metadata source;
    q::SerializedOrdering noBoot(source, r::Identity{});
    q::OrderingSnapshot x;
    check(noBoot.beginRecovery(x) == q::OrderingInventory::Unavailable && noBoot.mutationsBlocked(),
          "missing boot is not positive empty inventory");
}
void recovery() {
    for (auto terminal : {ProgressOutcome::Committed, ProgressOutcome::Denied}) {
        Fixture f;
        f.ready();
        q::OrderingLease lease;
        check(f.order.hold(binding(), lease) == q::Ordering::Held, "hold before uncertainty");
        for (auto uncertain :
             {ProgressOutcome::WriteUncertain, ProgressOutcome::IoFailure, ProgressOutcome::Corrupt,
              ProgressOutcome::Unsupported, ProgressOutcome::Full, ProgressOutcome::Recovered})
            check(f.order.resolved(lease, uncertain) == q::OrderingResolution::Unavailable &&
                      f.order.mutationsBlocked(),
                  "nonterminal/uncertain outcome retains exact hold");
        q::OrderingSnapshot pin;
        check(f.order.beginRecovery(pin) == q::OrderingInventory::Held &&
                  q::sameOrderingLease(pin.lease, lease) && pin.generation > lease.generation,
              "replay pin distinct from original held lease");
        q::OrderingSnapshot read;
        check(f.order.readRecovery(read) == q::OrderingInventory::Held &&
                  q::sameOrderingSnapshot(pin, read),
              "pin before replay is immutable");
        for (unsigned i = 0; i < 30; ++i) {
            auto wrong = pin;
            if (i == 0)
                wrong.inventory = q::OrderingInventory::Empty;
            else if (i == 1)
                wrong.boot.bytes[15] ^= 1;
            else if (i == 2)
                ++wrong.generation;
            else
                leaseMutate(wrong.lease, i - 3);
            check(f.order.prepareRecovery(wrong, terminal) == q::OrderingResolution::Stale,
                  "each pin/held lease field guarded");
            f.order.publishRecovery(wrong);
            f.order.abortRecovery(wrong);
            check(f.order.mutationsBlocked(), "wrong pin cannot publish or abort true recovery");
        }
        const auto calls = f.source.calls;
        check(f.order.prepareRecovery(pin, ProgressOutcome::Recovered) ==
                  q::OrderingResolution::Unavailable,
              "held pin cannot claim empty recovered");
        check(f.order.prepareRecovery(pin, terminal) == q::OrderingResolution::Resolved &&
                  f.order.mutationsBlocked(),
              "fallible prepare keeps metadata closed");
        check(f.order.readRecovery(read) == q::OrderingInventory::Held &&
                  q::sameOrderingSnapshot(pin, read),
              "pin after replay/prepare unchanged");
        check(f.order.prepareRecovery(pin, terminal) == q::OrderingResolution::Stale,
              "prepare cannot reserve twice");
        f.source.callback = []() {
            check(false, "publish must never read source/perform callback");
        };
        f.order.publishRecovery(pin);
        check(!f.order.mutationsBlocked() && f.source.calls == calls,
              "no-fail publish performs only own metadata update");
        check(f.order.resolved(lease, ProgressOutcome::Committed) == q::OrderingResolution::Stale,
              "historical lease cannot release a future hold");
    }
    for (bool prepare : {false, true}) {
        Fixture f;
        f.ready();
        q::OrderingLease l;
        check(f.order.hold(binding(), l) == q::Ordering::Held, "abort fixture hold");
        q::OrderingSnapshot pin;
        check(f.order.beginRecovery(pin) == q::OrderingInventory::Held, "abort fixture pin");
        if (prepare)
            check(f.order.prepareRecovery(pin, ProgressOutcome::Committed) ==
                      q::OrderingResolution::Resolved,
                  "prepared abort fixture");
        f.order.abortRecovery(pin);
        check(f.order.mutationsBlocked(), "abort remains recovery-required");
        q::OrderingLease x;
        check(f.order.hold(binding(), x) == q::Ordering::Unavailable,
              "abort cannot regain live hold");
        q::OrderingSnapshot next;
        check(f.order.beginRecovery(next) == q::OrderingInventory::Held &&
                  next.generation > pin.generation && q::sameOrderingLease(next.lease, l),
              "abort retains old hold and produces fresh replay pin");
        f.order.publishRecovery(pin);
        check(f.order.mutationsBlocked(), "aborted old pin cannot publish new recovery");
    }
    Fixture empty;
    q::OrderingSnapshot pin;
    check(empty.order.beginRecovery(pin) == q::OrderingInventory::Empty,
          "positive initial empty recovery");
    check(empty.order.prepareRecovery(pin, ProgressOutcome::Denied) ==
              q::OrderingResolution::Unavailable,
          "empty cannot pretend held denial");
    empty.order.publishRecovery(pin);
    check(empty.order.mutationsBlocked(), "publish before prepare remains closed");
    check(empty.order.prepareRecovery(pin, ProgressOutcome::Recovered) ==
              q::OrderingResolution::Resolved,
          "actual positive empty result");
    empty.order.publishRecovery(pin);
    check(!empty.order.mutationsBlocked(), "empty publication opens only after valid prepare");
}
void callbacks() {
    Fixture f;
    f.ready();
    auto b = binding();
    const auto original = b;
    f.source.callback = [&]() {
        check(f.order.mutationsBlocked(), "callback sees busy gate closed");
        q::OrderingLease l;
        q::OrderingSnapshot p;
        check(f.order.hold(original, l) == q::Ordering::Unavailable, "recursive hold closed");
        check(f.order.beginRecovery(p) == q::OrderingInventory::Unavailable &&
                  f.order.readRecovery(p) == q::OrderingInventory::Unavailable,
              "recursive recovery cannot acquire pin");
        check(f.order.resolved(l, ProgressOutcome::Committed) == q::OrderingResolution::Stale &&
                  f.order.prepareRecovery(p, ProgressOutcome::Recovered) ==
                      q::OrderingResolution::Stale,
              "recursive resolution/prepare closed");
        f.order.publishRecovery(p);
        f.order.abortRecovery(p);
        b.operation = id(99);
        b.request.bytes[0] ^= 1;
    };
    q::OrderingLease lease;
    check(f.order.hold(b, lease) == q::Ordering::Held &&
              q::sameCommitBinding(lease.binding, original),
          "binding copied before arbitrary source callback");
    check(f.order.resolved(lease, ProgressOutcome::Denied) == q::OrderingResolution::Resolved,
          "callback cannot disturb outer lease");
}
void transitions() {
    auto claim = binding();
    claim.kind = q::CommitKind::Claim;
    claim.parent = id(70);
    auto result = claim;
    result.kind = q::CommitKind::Result;
    result.operation = id(71);
    result.commitment.bytes[31] ^= 1;
    for (unsigned n = 0; n < 25; ++n) {
        if (n == 1 || n == 24)
            continue; // New result op/hash are intentionally different.
        Fixture f;
        f.ready();
        q::OrderingLease lease, output;
        output.generation = 777;
        check(f.order.hold(claim, lease) == q::Ordering::Held,
              "claim setup for invalid transition");
        auto wrong = result;
        alter(wrong, n);
        check(f.order.transition(lease, wrong, output) == q::Ordering::Denied &&
                  output.generation == 777 && f.order.mutationsBlocked(),
              "changed original scope cannot transition a claim");
        q::OrderingLease retained;
        check(f.order.hold(claim, retained) == q::Ordering::Held &&
                  q::sameOrderingLease(lease, retained),
              "failed transition retains exact claim lease");
    }
    for (unsigned n = 0; n < 27; ++n) {
        Fixture f;
        f.ready();
        q::OrderingLease lease, output;
        check(f.order.hold(claim, lease) == q::Ordering::Held, "claim setup for stale transition");
        auto wrong = lease;
        leaseMutate(wrong, n);
        check(f.order.transition(wrong, result, output) == q::Ordering::Denied &&
                  f.order.mutationsBlocked(),
              "every old lease field fenced before phase CAS");
    }
    for (unsigned mode = 0; mode < 4; ++mode) {
        Fixture f;
        f.ready();
        q::OrderingLease lease, output;
        check(f.order.hold(claim, lease) == q::Ordering::Held,
              "claim setup invalid result identity");
        auto wrong = result;
        if (mode == 0)
            wrong.operation = claim.operation;
        if (mode == 1)
            wrong.operation = claim.parent;
        if (mode == 2)
            wrong.operation = {};
        if (mode == 3)
            wrong.kind = static_cast<q::CommitKind>(255);
        check(f.order.transition(lease, wrong, output) == q::Ordering::Denied &&
                  f.order.mutationsBlocked(),
              "result cannot reuse claim/parent/null operation or unknown kind");
    }
    for (auto recovered : {ProgressOutcome::Committed, ProgressOutcome::Denied}) {
        Fixture f;
        f.ready();
        q::OrderingLease lease;
        check(f.order.hold(claim, lease) == q::Ordering::Held, "actual claim lease");
        const auto old = lease;
        const auto calls = f.source.calls;
        f.source.scope = q::MetadataScope::Unavailable;
        f.source.callback = []() { check(false, "transition must perform no source callback"); };
        check(f.order.transition(lease, result, lease) == q::Ordering::Held &&
                  q::sameCommitBinding(lease.binding, result) &&
                  lease.generation > old.generation && f.source.calls == calls &&
                  f.order.mutationsBlocked(),
              "alias-safe exact in-place Claim to Result remains continuously held without "
              "callbacks");
        check(f.order.resolved(old, ProgressOutcome::Committed) == q::OrderingResolution::Stale,
              "old claim resolution cannot release result phase");
        q::OrderingLease ignored;
        check(f.order.hold(claim, ignored) == q::Ordering::Denied &&
                  f.order.transition(old, result, ignored) == q::Ordering::Denied,
              "old claim cannot reacquire or transition result generation");
        check(f.order.resolved(lease, ProgressOutcome::WriteUncertain) ==
                      q::OrderingResolution::Unavailable &&
                  f.order.mutationsBlocked(),
              "Result Unknown never exposes Empty inventory");
        q::OrderingSnapshot pin, reread;
        check(f.order.beginRecovery(pin) == q::OrderingInventory::Held &&
                  pin.lease.binding.kind == q::CommitKind::Result &&
                  q::sameOrderingLease(pin.lease, lease),
              "Result Unknown recovery inventory identifies result, not original claim");
        check(f.order.readRecovery(reread) == q::OrderingInventory::Held &&
                  q::sameOrderingSnapshot(pin, reread),
              "result pin immutable through replay");
        check(f.order.transition(lease, result, ignored) == q::Ordering::Denied,
              "no live transition during recovery");
        check(f.order.prepareRecovery(pin, recovered) == q::OrderingResolution::Resolved,
              "result replay exact pin prepares committed or proven denied resolution");
        f.order.publishRecovery(pin);
        check(!f.order.mutationsBlocked() && f.source.calls == calls,
              "result replay no-fail publish alone releases gate without callbacks");
    }
    Fixture absent;
    absent.ready();
    q::OrderingLease none, out;
    check(absent.order.transition(none, result, out) == q::Ordering::Denied &&
              !absent.order.mutationsBlocked(),
          "no fabricated transition without a held claim");
}

} // namespace
int main() {
    exact();
    metadata();
    recovery();
    callbacks();
    transitions();
    std::cout << checks
              << " independent SerializedOrdering assertions; "
                 "SINGLE_WORKER_SYNTHETIC_METADATA_NO_AUTHORITY_IO\n";
}
