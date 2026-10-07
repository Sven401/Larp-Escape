#include "Esp32ProfileStagingStore.h"
#include <cerrno>
namespace {
const char *const TemporaryPath = "/larp-profiles/profile-source.tmp";
}
Esp32TemporaryFilePort::~Esp32TemporaryFilePort() {
    if (file_)
        std::fclose(file_);
    if (owner_.temporaryOwner_ == this) {
        owner_.temporaryOwner_ = nullptr;
        owner_.temporaryCleanupRequired_ = true;
    }
}
bool Esp32TemporaryFilePort::claim(bool cleanup) noexcept {
    if (!owner_.mounted_ || (!cleanup && owner_.temporaryCleanupRequired_) ||
        (owner_.temporaryOwner_ && owner_.temporaryOwner_ != this))
        return false;
    owner_.temporaryOwner_ = this;
    return true;
}
bool Esp32TemporaryFilePort::openTemporary() noexcept {
    if (file_ || !claim())
        return false;
    // Previous-boot temporary bytes are never imported or resumed.
    file_ = std::fopen(TemporaryPath, "wb");
    writing_ = file_ != nullptr;
    owner_.temporaryCleanupRequired_ = !writing_;
    return writing_;
}
bool Esp32TemporaryFilePort::writeTemporary(const char *bytes, std::size_t count) noexcept {
    if (owner_.temporaryOwner_ != this || owner_.temporaryCleanupRequired_ || !file_ || !writing_)
        return false;
    const bool good = std::fwrite(bytes, 1, count, file_) == count && !std::ferror(file_);
    owner_.temporaryCleanupRequired_ = !good;
    return good;
}
bool Esp32TemporaryFilePort::sealTemporary() noexcept {
    if (owner_.temporaryOwner_ != this || owner_.temporaryCleanupRequired_ || !file_ || !writing_)
        return false;
    const bool flushed = std::fflush(file_) == 0;
    const bool closed = std::fclose(file_) == 0;
    file_ = nullptr;
    writing_ = false;
    if (!flushed || !closed) {
        owner_.temporaryCleanupRequired_ = true;
        return false;
    }
    // Logical immutability plus an actual read-only descriptor; no rename/save.
    file_ = std::fopen(TemporaryPath, "rb");
    owner_.temporaryCleanupRequired_ = file_ == nullptr;
    return file_ != nullptr;
}
larp::configuration::ReadStatus Esp32TemporaryFilePort::readTemporary(std::uint8_t &byte) noexcept {
    using larp::configuration::ReadStatus;
    if (owner_.temporaryOwner_ != this || owner_.temporaryCleanupRequired_ || !file_ || writing_)
        return ReadStatus::Failure;
    const int value = std::fgetc(file_);
    if (value == EOF) {
        if (std::ferror(file_)) {
            owner_.temporaryCleanupRequired_ = true;
            return ReadStatus::Failure;
        }
        return ReadStatus::End;
    }
    byte = static_cast<std::uint8_t>(value);
    return ReadStatus::Byte;
}
bool Esp32TemporaryFilePort::discardTemporary() noexcept {
    if (!owner_.mounted_)
        return !file_ && !owner_.temporaryOwner_;
    if (!claim(true))
        return false;
    const bool closed = !file_ || std::fclose(file_) == 0;
    file_ = nullptr;
    writing_ = false;
    errno = 0;
    const bool removed = std::remove(TemporaryPath) == 0 || errno == ENOENT;
    owner_.temporaryCleanupRequired_ = !(closed && removed);
    if (closed && removed)
        owner_.temporaryOwner_ = nullptr;
    // Cleanup failure keeps the exclusive claim until a successful retry.
    return closed && removed;
}
