#pragma once

#include <cstdint>

namespace larp {
namespace domain {
enum class GridColumn : std::uint8_t { A = 0, B, C, D, E, F, G, H, I };

struct GridPosition {
    std::uint8_t row;
    GridColumn column;

    bool operator==(const GridPosition &other) const {
        return row == other.row && column == other.column;
    }

    bool operator!=(const GridPosition &other) const {
        return !(*this == other);
    }

    bool operator<(const GridPosition &other) const {
        if (row != other.row) {
            return row < other.row;
        }
        return column < other.column;
    }
};
} // namespace domain
} // namespace larp
