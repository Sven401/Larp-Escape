#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace larp {
namespace runtime {
inline bool isLedOriginValid(std::uint32_t origin, std::size_t ledCount) {
    return static_cast<std::size_t>(origin) < ledCount;
}

inline bool tryWrapLedIndex(std::int32_t position, std::size_t ledCount, std::size_t &index) {
    index = 0U;
    if (ledCount == 0U ||
        ledCount > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }
    const std::int32_t count = static_cast<std::int32_t>(ledCount);
    std::int32_t wrapped = position % count;
    if (wrapped < 0) {
        wrapped += count;
    }
    index = static_cast<std::size_t>(wrapped);
    return true;
}
} // namespace runtime
} // namespace larp
