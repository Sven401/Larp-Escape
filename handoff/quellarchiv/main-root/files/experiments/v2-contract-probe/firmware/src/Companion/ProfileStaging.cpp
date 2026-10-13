#include "Companion/ProfileStaging.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
class Call {
  public:
    explicit Call(bool &busy) : busy_(busy) {
        busy_ = true;
    }
    ~Call() {
        busy_ = false;
    }

  private:
    bool &busy_;
};
} // namespace
ProfileStaging::ProfileStaging(IProfileStagingStore &store, const IGameControlChannel &authority,
                               IProfileImportClock &clock) noexcept
    : store_(store), authority_(authority), clock_(clock) {}
ProfileReceiveOutcome ProfileStaging::fail(ProfileReceiveOutcome outcome) noexcept {
    phase_ = ProfileReceivePhase::Failed;
    outcome_ = outcome;
    if (!store_.discard())
        outcome_ = ProfileReceiveOutcome::StorageFailure;
    return outcome_;
}
bool ProfileStaging::current() noexcept {
    if (clock_.nowMilliseconds() - receivedAt_ >= ProfileSourceLimits::DeadlineMilliseconds) {
        fail(ProfileReceiveOutcome::Expired);
        return false;
    }
    TrustedAuthority current;
    if (!original_.gameMaster || !original_.principal || !original_.epoch ||
        !authority_.readAuthority(current) || !(current == original_)) {
        fail(ProfileReceiveOutcome::Unauthorized);
        return false;
    }
    return true;
}
ProfileReceiveOutcome ProfileStaging::start(const ProfileSourceEnvelope &envelope,
                                            const TrustedAuthority &original,
                                            std::uint32_t receivedAt) noexcept {
    if (busy_ || phase_ == ProfileReceivePhase::Receiving ||
        phase_ == ProfileReceivePhase::Complete)
        return ProfileReceiveOutcome::Busy;
    Call call(busy_);
    const auto selected = envelope;
    const auto route = original;
    if (!selected.valid())
        return ProfileReceiveOutcome::InvalidEnvelope;
    envelope_ = selected;
    original_ = route;
    receivedAt_ = receivedAt;
    received_ = read_ = 0;
    crc_ = readCrc_ = 0xffffffffu;
    ended_ = false;
    if (!current())
        return outcome_;
    // A failed/previous temporary backend must be explicitly cleaned before open.
    if (!store_.discard())
        return fail(ProfileReceiveOutcome::StorageFailure);
    if (!current())
        return outcome_;
    if (!store_.begin(envelope_.bytes))
        return fail(ProfileReceiveOutcome::StorageFailure);
    if (!current())
        return outcome_;
    phase_ = ProfileReceivePhase::Receiving;
    return outcome_ = ProfileReceiveOutcome::Receiving;
}
ProfileReceiveOutcome ProfileStaging::data(const char *bytes, std::size_t count,
                                           const TrustedAuthority &original) noexcept {
    if (busy_)
        return ProfileReceiveOutcome::Busy;
    Call call(busy_);
    const auto route = original;
    if (!(route == original_))
        return ProfileReceiveOutcome::WrongRoute;
    if (phase_ != ProfileReceivePhase::Receiving)
        return ProfileReceiveOutcome::NotReceiving;
    // Freeze callback-owned bytes before virtual clock/authority observations.
    char owned[ProfileFrameLimits::ValueBytes];
    const bool bounded =
        bytes && count <= ProfileFrameLimits::ValueBytes && count > ProfileFrameLimits::HeaderBytes;
    if (bounded)
        std::memcpy(owned, bytes, count);
    if (!current())
        return outcome_;
    if (!bounded)
        return fail(ProfileReceiveOutcome::InvalidFrame);
    ProfileDataFrame frame;
    if (!decodeProfileDataFrame(owned, count, frame) || frame.transfer != envelope_.transfer ||
        frame.offset != received_ || frame.count > envelope_.bytes - received_)
        return fail(ProfileReceiveOutcome::InvalidFrame);
    if (!store_.append(frame.payload, frame.count))
        return fail(ProfileReceiveOutcome::StorageFailure);
    if (!current())
        return outcome_;
    for (std::size_t i = 0; i < frame.count; ++i)
        crc_ = profileSourceCrcByte(crc_, static_cast<std::uint8_t>(frame.payload[i]));
    received_ += frame.count;
    return outcome_ = ProfileReceiveOutcome::Receiving;
}
ProfileReceiveOutcome ProfileStaging::finish(const TrustedAuthority &original,
                                             std::uint16_t transfer) noexcept {
    if (busy_)
        return ProfileReceiveOutcome::Busy;
    Call call(busy_);
    const auto route = original;
    if (!(route == original_) || transfer != envelope_.transfer)
        return ProfileReceiveOutcome::WrongRoute;
    if (phase_ != ProfileReceivePhase::Receiving)
        return ProfileReceiveOutcome::NotReceiving;
    if (!current())
        return outcome_;
    if (received_ != envelope_.bytes)
        return fail(ProfileReceiveOutcome::Incomplete);
    if ((crc_ ^ 0xffffffffu) != envelope_.crc32)
        return fail(ProfileReceiveOutcome::DigestMismatch);
    if (!store_.seal())
        return fail(ProfileReceiveOutcome::StorageFailure);
    if (!current())
        return outcome_;
    phase_ = ProfileReceivePhase::Complete;
    return outcome_ = ProfileReceiveOutcome::Complete;
}
ProfileReceiveOutcome ProfileStaging::service() noexcept {
    if (busy_)
        return ProfileReceiveOutcome::Busy;
    Call call(busy_);
    if (phase_ == ProfileReceivePhase::Receiving || phase_ == ProfileReceivePhase::Complete)
        current();
    return outcome_;
}
ProfileReceiveOutcome ProfileStaging::discard() noexcept {
    if (busy_)
        return ProfileReceiveOutcome::Busy;
    Call call(busy_);
    phase_ = ProfileReceivePhase::Empty;
    ended_ = false;
    if (!store_.discard()) {
        phase_ = ProfileReceivePhase::Failed;
        return outcome_ = ProfileReceiveOutcome::StorageFailure;
    }
    return outcome_ = ProfileReceiveOutcome::Discarded;
}
configuration::ReadStatus ProfileStaging::readByte(std::uint8_t &byte) {
    using configuration::ReadStatus;
    if (busy_ || phase_ != ProfileReceivePhase::Complete)
        return ReadStatus::Failure;
    Call call(busy_);
    if (!current())
        return ReadStatus::Failure;
    if (ended_)
        return ReadStatus::End;
    std::uint8_t value = 0;
    const auto status = store_.readByte(value);
    if (!current())
        return ReadStatus::Failure;
    if (status == ReadStatus::End && read_ == envelope_.bytes) {
        if ((readCrc_ ^ 0xffffffffu) != envelope_.crc32) {
            fail(ProfileReceiveOutcome::DigestMismatch);
            return ReadStatus::Failure;
        }
        ended_ = true;
        return ReadStatus::End;
    }
    if (status != ReadStatus::Byte || read_ == envelope_.bytes) {
        fail(ProfileReceiveOutcome::StorageFailure);
        return ReadStatus::Failure;
    }
    ++read_;
    readCrc_ = profileSourceCrcByte(readCrc_, value);
    byte = value;
    return ReadStatus::Byte;
}
} // namespace companion
} // namespace larp
