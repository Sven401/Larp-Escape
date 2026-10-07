#include "TemporaryProfileFile.h"
bool TemporaryProfileFile::begin(std::uint32_t bytes) noexcept {
    if (file_ || !bytes || bytes > larp::configuration::ImportLimits::DocumentBytes)
        return false;
    file_ = std::tmpfile();
    if (!file_)
        return false;
    limit_ = bytes;
    written_ = read_ = 0;
    sealed_ = ended_ = false;
    return true;
}
bool TemporaryProfileFile::append(const char *bytes, std::size_t count) noexcept {
    if (!file_ || sealed_ || !bytes || !count ||
        count > larp::companion::ProfileFrameLimits::PayloadBytes || count > limit_ - written_)
        return false;
    if (std::fwrite(bytes, 1, count, file_) != count || std::ferror(file_))
        return false;
    written_ += count;
    return true;
}
bool TemporaryProfileFile::seal() noexcept {
    if (!file_ || sealed_ || written_ != limit_ || std::fflush(file_) != 0 ||
        std::fseek(file_, 0, SEEK_SET) != 0)
        return false;
    sealed_ = true;
    return true;
}
bool TemporaryProfileFile::discard() noexcept {
    auto *selected = file_;
    file_ = nullptr;
    limit_ = written_ = read_ = 0;
    sealed_ = ended_ = false;
    return !selected || std::fclose(selected) == 0;
}
larp::configuration::ReadStatus TemporaryProfileFile::readByte(std::uint8_t &byte) {
    using larp::configuration::ReadStatus;
    if (!file_ || !sealed_)
        return ReadStatus::Failure;
    if (ended_)
        return ReadStatus::End;
    const int value = std::fgetc(file_);
    if (value == EOF) {
        if (std::ferror(file_) || read_ != limit_)
            return ReadStatus::Failure;
        ended_ = true;
        return ReadStatus::End;
    }
    if (read_ == limit_)
        return ReadStatus::Failure;
    ++read_;
    byte = static_cast<std::uint8_t>(value);
    return ReadStatus::Byte;
}
