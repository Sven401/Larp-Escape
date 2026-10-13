#pragma once

#include <cstddef>
#include <cstdint>

#include "GameApplication/GameSnapshot.h"

namespace larp {
namespace companion {
// Includes the terminating null byte. No heap or transport is required.
constexpr std::size_t maxStatusJsonBytes = 512U;

enum class StatusEncodeResult : std::uint8_t { Success, InvalidSnapshot, BufferTooSmall };

// Stable version-1 names; unknown enumeration values return nullptr.
const char *phaseWireName(application::GamePhase phase);
const char *roleWireName(domain::RoleId role);

// written excludes the terminating null byte. On failure written is zero and
// buffer[0] is null when a non-null buffer with positive capacity was supplied.
// A missing role is serialized as null; its unused roleId is ignored.
// roundCount is completed rounds, not the number of the current round.
// A retained role remains visible in standby: it does not assert a card is present.
StatusEncodeResult encodeStatus(const application::GameSnapshot &snapshot, char *buffer,
                                std::size_t capacity, std::size_t &written);
} // namespace companion
} // namespace larp
