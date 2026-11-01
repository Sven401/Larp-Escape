#pragma once
#include "GameExecution/RulesPlanner.h"
#include <array>
namespace larp {
namespace devices {
// Physically partition-local allocator metadata, NOT a device/station identity,
// key, authority, Local input, control approval or anti-rollback anchor.
struct LocalGenerationSnapshot {
    std::uint64_t generation = 0;
    execution::rules::Identity boot;
};
struct LocalGenerationRecord {
    std::array<std::uint8_t, 64> bytes = {};
    bool operator==(const LocalGenerationRecord &other) const noexcept {
        return bytes == other.bytes;
    }
};
bool encodeLocalGeneration(const LocalGenerationSnapshot &, LocalGenerationRecord &) noexcept;
bool decodeLocalGeneration(const LocalGenerationRecord &, LocalGenerationSnapshot &) noexcept;
enum class LocalGenerationStatus : std::uint8_t {
    Unavailable,
    Reserved,
    Missing,
    Corrupt,
    Overflow,
    Conflict,
    Uncertain
};
} // namespace devices
} // namespace larp
