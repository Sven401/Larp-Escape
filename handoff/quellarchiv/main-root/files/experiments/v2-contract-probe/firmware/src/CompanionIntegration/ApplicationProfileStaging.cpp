#include "CompanionIntegration/ApplicationProfileStaging.h"
#include <cstring>
namespace larp {
namespace integration {
namespace {
struct Call {
    explicit Call(bool &busy) : busy(busy) {
        busy = true;
    }
    ~Call() {
        busy = false;
    }
    bool &busy;
};
} // namespace
ApplicationProfileStaging::~ApplicationProfileStaging() {
    application_.abandonProfileFile(this);
}
bool ApplicationProfileStaging::begin(std::uint32_t bytes) noexcept {
    if (busy_ || phase_ != Phase::Empty || !bytes ||
        bytes > configuration::ImportLimits::DocumentBytes)
        return false;
    Call call(busy_);
    if (!application_.claimProfileFile(this, false))
        return false;
    limit_ = bytes;
    written_ = 0;
    phase_ = Phase::Failed;
    if (!backend_.begin(bytes))
        return false;
    phase_ = Phase::Receiving;
    return true;
}
bool ApplicationProfileStaging::append(const char *bytes, std::size_t count) noexcept {
    if (busy_ || phase_ != Phase::Receiving || !bytes || !count ||
        count > companion::ProfileFrameLimits::PayloadBytes || count > limit_ - written_ ||
        !application_.ownsProfileFile(this) || application_.busy())
        return false;
    Call call(busy_);
    char owned[companion::ProfileFrameLimits::PayloadBytes];
    std::memcpy(owned, bytes, count);
    if (!backend_.append(owned, count)) {
        phase_ = Phase::Failed;
        return false;
    }
    written_ += count;
    return true;
}
bool ApplicationProfileStaging::seal() noexcept {
    if (busy_ || phase_ != Phase::Receiving || written_ != limit_ ||
        !application_.ownsProfileFile(this) || application_.busy())
        return false;
    Call call(busy_);
    phase_ = Phase::Failed;
    if (!backend_.seal())
        return false;
    phase_ = Phase::Sealed;
    return true;
}
bool ApplicationProfileStaging::discard() noexcept {
    if (busy_)
        return false;
    Call call(busy_);
    if (!application_.ownsProfileFile(this) && !application_.claimProfileFile(this, true))
        return false;
    phase_ = Phase::Failed;
    if (!backend_.discard())
        return false;
    phase_ = Phase::Empty;
    limit_ = written_ = 0;
    application_.releaseProfileFile(this);
    return true;
}
configuration::ReadStatus ApplicationProfileStaging::readByte(std::uint8_t &byte) {
    using configuration::ReadStatus;
    if (busy_ || phase_ != Phase::Sealed || !application_.ownsProfileFile(this))
        return ReadStatus::Failure;
    Call call(busy_);
    std::uint8_t value = 0;
    const auto status = backend_.readByte(value);
    if (status == ReadStatus::Byte) {
        byte = value;
        return status;
    }
    if (status == ReadStatus::End)
        return status;
    phase_ = Phase::Failed;
    return ReadStatus::Failure;
}
} // namespace integration
} // namespace larp
