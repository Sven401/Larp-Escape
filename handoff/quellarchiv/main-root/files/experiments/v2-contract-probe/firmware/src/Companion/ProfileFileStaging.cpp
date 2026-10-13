#include "Companion/ProfileFileStaging.h"
#include <cstring>
namespace larp {
namespace companion {
namespace {
class FileCall {
  public:
    explicit FileCall(bool &busy) : busy_(busy) {
        busy_ = true;
    }
    ~FileCall() {
        busy_ = false;
    }

  private:
    bool &busy_;
};
} // namespace
bool ProfileFileStaging::begin(std::uint32_t bytes) noexcept {
    if (busy_ || started_ || failed_ || !bytes ||
        bytes > configuration::ImportLimits::DocumentBytes)
        return false;
    FileCall call(busy_);
    started_ = true;
    limit_ = bytes;
    written_ = read_ = 0;
    sealed_ = ended_ = false;
    failed_ = !file_.openTemporary();
    return !failed_;
}
bool ProfileFileStaging::append(const char *bytes, std::size_t count) noexcept {
    if (busy_ || !started_ || sealed_ || failed_ || !bytes || !count ||
        count > ProfileFrameLimits::PayloadBytes || count > limit_ - written_)
        return false;
    FileCall call(busy_);
    char selected[ProfileFrameLimits::PayloadBytes];
    std::memcpy(selected, bytes, count);
    if (!file_.writeTemporary(selected, count)) {
        failed_ = true;
        return false;
    }
    written_ += static_cast<std::uint32_t>(count);
    return true;
}
bool ProfileFileStaging::seal() noexcept {
    if (busy_ || !started_ || sealed_ || failed_ || written_ != limit_)
        return false;
    FileCall call(busy_);
    if (!file_.sealTemporary()) {
        failed_ = true;
        return false;
    }
    sealed_ = true;
    return true;
}
bool ProfileFileStaging::discard() noexcept {
    if (busy_)
        return false;
    FileCall call(busy_);
    if (!file_.discardTemporary()) {
        failed_ = true;
        return false;
    }
    limit_ = written_ = read_ = 0;
    started_ = sealed_ = ended_ = failed_ = false;
    return true;
}
configuration::ReadStatus ProfileFileStaging::readByte(std::uint8_t &byte) {
    using configuration::ReadStatus;
    if (busy_ || !started_ || !sealed_ || failed_)
        return ReadStatus::Failure;
    if (ended_)
        return ReadStatus::End;
    FileCall call(busy_);
    std::uint8_t selected = 0;
    const auto result = file_.readTemporary(selected);
    if (result == ReadStatus::Byte && read_ < limit_) {
        ++read_;
        byte = selected;
        return ReadStatus::Byte;
    }
    if (result == ReadStatus::End && read_ == limit_) {
        ended_ = true;
        return ReadStatus::End;
    }
    failed_ = true;
    return ReadStatus::Failure;
}
} // namespace companion
} // namespace larp
