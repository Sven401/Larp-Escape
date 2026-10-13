#pragma once

#include <cstdint>

#include "GameDomain/GameDefinitions.h"

namespace larp {
namespace domain {
enum class KeystoneTransition : unsigned char {
    EmptyRead,
    RepeatedPreviousKeystone,
    RoleAccepted,
    UnknownRole,
    BoardNotCleared
};

KeystoneTransition classifyKeystoneTransition(const CardUid &candidate,
                                              const CardUid &previousKeystone,
                                              bool candidateIsKnownRole);
bool hasEnoughEmptyKeystoneReads(std::uint8_t consecutiveEmptyReads);
} // namespace domain
} // namespace larp
