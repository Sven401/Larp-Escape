#include "Esp32ProfileStore.h"
#include <cerrno>
#include <esp_littlefs.h>
#include <unistd.h>
namespace {
const char *const Paths[] = {"/larp-profiles/profile-a.larp", "/larp-profiles/profile-b.larp"};
} // namespace
using larp::configuration::ReadStatus;
using larp::devices::JournalCommit;
using larp::devices::JournalOpen;
Esp32ProfileStore::~Esp32ProfileStore() {
    if (file_)
        std::fclose(file_);
    // Mount is process-lifetime: do not unmount beneath another live service.
}
bool Esp32ProfileStore::begin() {
    if (mounted_ || file_)
        return false;
    esp_vfs_littlefs_conf_t config = {};
    config.base_path = "/larp-profiles";
    config.partition_label = "spiffs";
    config.format_if_mount_failed = false;
    config.dont_mount = false;
    config.grow_on_mount = false;
    mounted_ = esp_vfs_littlefs_register(&config) == ESP_OK;
    return mounted_;
}
JournalOpen Esp32ProfileStore::openRead(std::uint8_t slot) {
    if (!mounted_ || file_ || slot > 1)
        return JournalOpen::Failure;
    errno = 0;
    file_ = std::fopen(Paths[slot], "rb");
    writing_ = false;
    return file_             ? JournalOpen::Present
           : errno == ENOENT ? JournalOpen::Missing
                             : JournalOpen::Failure;
}
ReadStatus Esp32ProfileStore::readByte(std::uint8_t &byte) {
    if (!file_ || writing_)
        return ReadStatus::Failure;
    const int value = std::fgetc(file_);
    if (value == EOF)
        return std::ferror(file_) ? ReadStatus::Failure : ReadStatus::End;
    byte = static_cast<std::uint8_t>(value);
    return ReadStatus::Byte;
}
bool Esp32ProfileStore::closeRead() {
    if (!file_ || writing_)
        return false;
    const bool good = std::fclose(file_) == 0;
    file_ = nullptr;
    return good;
}
bool Esp32ProfileStore::beginWrite(std::uint8_t slot) {
    if (!mounted_ || file_ || slot > 1)
        return false;
    file_ = std::fopen(Paths[slot], "wb");
    writing_ = true;
    return file_ != nullptr;
}
bool Esp32ProfileStore::write(const char *bytes, std::size_t count) {
    return file_ && writing_ && std::fwrite(bytes, 1, count, file_) == count;
}
JournalCommit Esp32ProfileStore::finishWrite() {
    if (!file_ || !writing_)
        return JournalCommit::Uncertain;
    const bool flushed = std::fflush(file_) == 0;
    const bool synced = flushed && fsync(fileno(file_)) == 0;
    const bool closed = std::fclose(file_) == 0;
    file_ = nullptr;
    writing_ = false;
    return flushed && synced && closed ? JournalCommit::Confirmed : JournalCommit::Uncertain;
}
void Esp32ProfileStore::abortWrite() {
    if (file_)
        std::fclose(file_);
    file_ = nullptr;
    writing_ = false;
}
