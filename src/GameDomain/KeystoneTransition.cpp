#include "GameDomain/KeystoneTransition.h"

namespace larp {
namespace domain {
KeystoneTransition classifyKeystoneTransition(const CardUid &candidate,
                                              const CardUid &previousKeystone,
                                              bool candidateIsKnownRole) {
    const CardUid emptyUid = {};
    if (candidate == emptyUid) {
        return KeystoneTransition::EmptyRead;
    }

    if (candidate == previousKeystone) {
        return KeystoneTransition::RepeatedPreviousKeystone;
    }

    if (previousKeystone == emptyUid) {
        return candidateIsKnownRole ? KeystoneTransition::RoleAccepted
                                    : KeystoneTransition::UnknownRole;
    }

    return KeystoneTransition::BoardNotCleared;
}

bool hasEnoughEmptyKeystoneReads(std::uint8_t consecutiveEmptyReads) {
    return consecutiveEmptyReads >= 5;
}
} // namespace domain
} // namespace larp
