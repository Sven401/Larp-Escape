#pragma once
#include "DeviceRuntime/RulesProgressEvidence.h"
namespace larp {
namespace devices {
namespace rules_progress {
constexpr std::size_t MaximumIntentReservations = 32;
struct IntentReservation {
    std::uint16_t index = 0;
    rules::Identity claim, result;
};
struct Transaction {
    rules::Identity operation;
    rules::Digest request, catalog;
    rules::Span<std::uint8_t> application;
    rules::Pool<IntentReservation> intents;
};
enum class TransactionError : std::uint8_t { None, Argument, Alias, Capacity, Canonical, Value };
struct TransactionResult {
    bool valid = false;
    TransactionError error = TransactionError::None;
    std::size_t offset = 0;
};
// Strict canonical map0..5/domain larp.rules.transaction.v1. Inner Application
// bytes remain exact/unmodified. This codec does not verify inner semantics,
// signatures, rights, clock, range or the outer Bundle-Key: actual Owner must.
// Whole wrapper INCLUDING inner bytes and every reserved phase-ID <=32768.
// Decoder copies to its exclusive own application pool; no borrowed raw views.
TransactionResult encodeTransaction(const Transaction &, rules::Pool<std::uint8_t> &);
TransactionResult decodeTransaction(const std::uint8_t *, std::size_t, Transaction &,
                                    rules::Pool<std::uint8_t> &ownedApplication);
} // namespace rules_progress
} // namespace devices
} // namespace larp
