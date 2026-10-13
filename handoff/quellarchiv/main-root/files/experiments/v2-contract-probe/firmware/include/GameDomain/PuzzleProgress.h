#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "GameDomain/GameDefinitions.h"

namespace larp {
namespace domain {
enum class CrystalRecordResult : std::uint8_t { Added, Duplicate, Full };

struct RecordedCrystal {
    GridPosition position;
    AudioTrack audioTrack;
    bool audioPlayed;
};

class PuzzleProgress {
  public:
    static const std::size_t RequiredCrystalCount = 3;

    PuzzleProgress();

    CrystalRecordResult record(const CrystalDefinition &crystal);
    std::size_t count() const;
    bool isComplete() const;
    bool hasPendingAudio() const;
    const RecordedCrystal *crystalAt(std::size_t index) const;
    bool markAudioPlayed(std::size_t index);
    void reset();

  private:
    std::array<RecordedCrystal, RequiredCrystalCount> crystals_;
    std::size_t count_;
};
} // namespace domain
} // namespace larp
