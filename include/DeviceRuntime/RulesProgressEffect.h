#pragma once
#include "DeviceRuntime/ProgressJournal.h"
#include "DeviceRuntime/RulesProgressTransaction.h"

namespace larp {
namespace devices {
namespace rules_progress {
enum class EffectPhase : std::uint8_t { Claim = 1, Result = 2 };
// A handler return is NOT physical observation and never creates D0 observed.
enum class HandlerResult : std::uint8_t { None = 0, Returned = 1, Failed = 2, Unknown = 3 };
struct Effect {
    rules::Identity application;
    rules::Digest catalog;
    std::uint16_t intentIndex = 0;
    rules::Identity ownerBoot;
    rules::Fence expected;
    EffectPhase phase = EffectPhase::Claim;
    HandlerResult result = HandlerResult::None;
};
enum class EffectError : std::uint8_t { None, Argument, Alias, Capacity, Canonical, Value, Header };
struct EffectResult {
    bool valid = false;
    EffectError error = EffectError::None;
    std::size_t offset = 0;
};
// Exact canonical map0..7/domain larp.rules.effect.v1, bounded to the unchanged
// physical Journal payload. This codec performs no IO or crypto and asserts no
// source/GM/actor/physical eligibility. The full original causes are immutable
// ParentEnvelope.Application.Plan.Intent.Evidence, never shortened here.
// Aliased/invalid storage rejects before touching possibly shared objects;
// after valid preflight, failure clears decoded Effect / encoded count.
bool effectDomain(const std::uint8_t *, std::size_t);
EffectResult encodeEffect(const Effect &, rules::Pool<std::uint8_t> &);
EffectResult decodeEffect(const std::uint8_t *, std::size_t, Effect &);
// Checks the actual physical kind/zero allocation and the exact frozen phase ID
// for this intent. The Owner MUST additionally check actual raw SHA/payload,
// parent application/catalog/context, once-only Claim->Result ordering, original
// immutable scope, capacity and current local eligibility before a Claim.
// Never compare physical expectedRevision with logical expected.progress.
EffectResult validateEffectRecord(const Effect &, const ProgressRecord &,
                                  const IntentReservation &);
} // namespace rules_progress
} // namespace devices
} // namespace larp
