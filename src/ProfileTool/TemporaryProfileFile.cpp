#include "TemporaryProfileFile.h"
bool HostTemporaryFilePort::openTemporary() noexcept {
    if (file_)
        return false;
    file_ = std::tmpfile();
    sealed_ = false;
    return file_ != nullptr;
}
bool HostTemporaryFilePort::writeTemporary(const char *bytes, std::size_t count) noexcept {
    return file_ && !sealed_ && std::fwrite(bytes, 1, count, file_) == count && !std::ferror(file_);
}
bool HostTemporaryFilePort::sealTemporary() noexcept {
    if (!file_ || sealed_ || std::fflush(file_) != 0 || std::fseek(file_, 0, SEEK_SET) != 0)
        return false;
    sealed_ = true;
    return true;
}
larp::configuration::ReadStatus HostTemporaryFilePort::readTemporary(std::uint8_t &byte) noexcept {
    using larp::configuration::ReadStatus;
    if (!file_ || !sealed_)
        return ReadStatus::Failure;
    const int value = std::fgetc(file_);
    if (value == EOF)
        return std::ferror(file_) ? ReadStatus::Failure : ReadStatus::End;
    byte = static_cast<std::uint8_t>(value);
    return ReadStatus::Byte;
}
bool HostTemporaryFilePort::discardTemporary() noexcept {
    auto *selected = file_;
    file_ = nullptr;
    sealed_ = false;
    return !selected || std::fclose(selected) == 0;
}
