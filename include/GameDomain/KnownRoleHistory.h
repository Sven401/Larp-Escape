#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>

#include "GameDomain/GameDefinitions.h"

namespace larp {
namespace domain {
// Insertion-ordered history of unique known role cards for one game. Capacity
// follows the role catalogue, so observing cards never allocates runtime memory.
// Iteration is read-only: callers cannot replace entries with unknown/duplicate
// cards. push_back returns true only when a known card was newly remembered;
// rejection leaves every existing entry intact. clear makes all slots reusable.
class KnownRoleHistory {
  public:
    using value_type = CardUid;
    using size_type = std::size_t;
    using const_iterator = const CardUid *;
    static const size_type Capacity =
        std::tuple_size<typename std::decay<decltype(defaultRoleDefinitions())>::type>::value;

    KnownRoleHistory() : entries(), entryCount(0) {}

    bool push_back(const CardUid &uid) {
        if (findRoleByUid(uid) == nullptr || contains(uid) || entryCount >= Capacity)
            return false;
        entries[entryCount++] = uid;
        return true;
    }

    bool contains(const CardUid &uid) const {
        for (size_type position = 0; position < entryCount; ++position) {
            if (entries[position] == uid)
                return true;
        }
        return false;
    }

    size_type size() const {
        return entryCount;
    }
    size_type capacity() const {
        return Capacity;
    }
    bool empty() const {
        return entryCount == 0;
    }
    const_iterator begin() const {
        return entries.data();
    }
    const_iterator end() const {
        return entries.data() + entryCount;
    }
    void clear() {
        entryCount = 0;
    }

  private:
    std::array<CardUid, Capacity> entries;
    size_type entryCount;
};
} // namespace domain
} // namespace larp
