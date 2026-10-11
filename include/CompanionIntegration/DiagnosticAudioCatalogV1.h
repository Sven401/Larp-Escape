#pragma once

#include <cstddef>
#include <cstdint>

namespace larp {
namespace diagnostic {

enum class AudioCatalogSourceV1 : std::uint8_t { Reichstagsgame, StarWarsCon };

struct AudioCatalogEntryV1 {
    AudioCatalogSourceV1 source;
    const char *group;
    const char *clip;
    std::uint16_t track;
};

// Logical catalogue only. An entry does not prove that an SD file exists or
// that the DFPlayer accepted, played, or audibly rendered its track.
std::size_t audioCatalogCountV1() noexcept;
bool audioCatalogEntryV1(std::size_t index, AudioCatalogEntryV1 &out) noexcept;

// A deliberately narrower direct diagnostic input range than uint16_t.
// The range is not an SD inventory or a DFPlayer capability claim.
bool diagnosticDirectTrackInRangeV1(std::uint16_t track) noexcept;

} // namespace diagnostic
} // namespace larp
