#pragma once
#include "GameDomain/GridPosition.h"
#include <array>
#include <cstddef>
#include <initializer_list>

namespace larp {
namespace ports {
// The installed matrix has at most 9*9 contacts. A scan owns its values and
// never allocates. Positions (including invalid ones) retain input order so
// the controller can apply its existing acceptance/error policy.
class CrystalInputBatch {
  public:
    CrystalInputBatch() : positions_(), count_(0U) {}
    CrystalInputBatch(std::initializer_list<domain::GridPosition> positions) : CrystalInputBatch() {
        for (const auto &position : positions)
            push_back(position);
    }
    CrystalInputBatch &operator=(std::initializer_list<domain::GridPosition> positions) {
        clear();
        for (const auto &position : positions)
            push_back(position);
        return *this;
    }
    bool push_back(domain::GridPosition position) {
        if (count_ == positions_.size())
            return false;
        positions_[count_++] = position;
        return true;
    }
    void clear() {
        count_ = 0U;
    }
    bool empty() const {
        return count_ == 0U;
    }
    std::size_t size() const {
        return count_;
    }
    std::size_t capacity() const {
        return positions_.size();
    }
    const domain::GridPosition *begin() const {
        return positions_.data();
    }
    const domain::GridPosition *end() const {
        return positions_.data() + count_;
    }
    // As with an array, callers must keep index < size().
    const domain::GridPosition &operator[](std::size_t index) const {
        return positions_[index];
    }

  private:
    std::array<domain::GridPosition, 81U> positions_;
    std::size_t count_;
};
} // namespace ports
} // namespace larp
