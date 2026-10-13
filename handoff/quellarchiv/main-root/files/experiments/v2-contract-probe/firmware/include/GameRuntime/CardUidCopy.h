#pragma once
#include "GameDomain/GameDefinitions.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace runtime {
// This game's role cards have exactly seven bytes. Rejected input never leaves
// a partially copied or old UID that could be mistaken for a fresh card.
inline bool tryCopyCardUid(const std::uint8_t *bytes, std::size_t length, domain::CardUid &uid) {
    uid = {};
    if (bytes == nullptr || length != uid.size())
        return false;
    std::copy_n(bytes, uid.size(), uid.begin());
    return true;
}
} // namespace runtime
} // namespace larp
