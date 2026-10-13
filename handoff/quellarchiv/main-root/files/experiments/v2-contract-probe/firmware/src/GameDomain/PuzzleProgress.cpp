#include "GameDomain/PuzzleProgress.h"

namespace larp {
namespace domain {
PuzzleProgress::PuzzleProgress() : crystals_(), count_(0) {}

CrystalRecordResult PuzzleProgress::record(const CrystalDefinition &crystal) {
    for (std::size_t index = 0; index < count_; ++index) {
        if (crystals_[index].position == crystal.position) {
            return CrystalRecordResult::Duplicate;
        }
    }

    if (count_ >= crystals_.size()) {
        return CrystalRecordResult::Full;
    }

    crystals_[count_] = {crystal.position, crystal.audioTrack, false};
    ++count_;
    return CrystalRecordResult::Added;
}

std::size_t PuzzleProgress::count() const {
    return count_;
}

bool PuzzleProgress::isComplete() const {
    return count_ == RequiredCrystalCount;
}

bool PuzzleProgress::hasPendingAudio() const {
    for (std::size_t index = 0; index < count_; ++index) {
        if (!crystals_[index].audioPlayed) {
            return true;
        }
    }
    return false;
}

const RecordedCrystal *PuzzleProgress::crystalAt(std::size_t index) const {
    return index < count_ ? &crystals_[index] : nullptr;
}

bool PuzzleProgress::markAudioPlayed(std::size_t index) {
    if (index >= count_) {
        return false;
    }

    crystals_[index].audioPlayed = true;
    return true;
}

void PuzzleProgress::reset() {
    count_ = 0;
}
} // namespace domain
} // namespace larp
