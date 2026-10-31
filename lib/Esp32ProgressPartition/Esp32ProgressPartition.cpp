#include "Esp32ProgressPartition.h"
#include <cstring>

namespace {
constexpr std::size_t PartitionBytes = 256U * 1024U;
constexpr std::size_t PageBytes = 4096U;
const char Label[] = "larp_progress";
} // namespace

bool Esp32ProgressPartition::begin() {
    if (attempted_)
        return false;
    attempted_ = true;
    const auto *candidate = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                     ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, Label);
    if (!candidate || candidate->type != ESP_PARTITION_TYPE_DATA ||
        candidate->subtype != ESP_PARTITION_SUBTYPE_DATA_UNDEFINED ||
        std::strncmp(candidate->label, Label, sizeof(candidate->label)) != 0 ||
        candidate->size != PartitionBytes || candidate->address % PageBytes != 0 ||
        candidate->erase_size != PageBytes || candidate->encrypted || candidate->readonly) {
        failed_ = true;
        return false;
    }
    partition_ = candidate;
    return true;
}
std::size_t Esp32ProgressPartition::capacity() const {
    return partition_ && !failed_ ? PartitionBytes : 0;
}
bool Esp32ProgressPartition::inBounds(std::size_t offset, std::size_t count) const {
    return partition_ && !failed_ && offset <= PartitionBytes && count <= PartitionBytes - offset;
}
bool Esp32ProgressPartition::read(std::size_t offset, std::uint8_t *bytes, std::size_t count) {
    if (!bytes || count == 0 || !inBounds(offset, count))
        return false;
    if (esp_partition_read(partition_, offset, bytes, count) != ESP_OK) {
        failed_ = true;
        return false;
    }
    return true;
}
bool Esp32ProgressPartition::program(std::size_t offset, const std::uint8_t *bytes,
                                     std::size_t count) {
    if (!bytes || count == 0 || !inBounds(offset, count))
        return false;
    // Check the complete target before any write: programming never overwrites
    // even an identical record and never relies on a flash erase side effect.
    std::uint8_t check[128];
    for (std::size_t done = 0; done < count;) {
        const auto part = count - done < sizeof(check) ? count - done : sizeof(check);
        if (!read(offset + done, check, part))
            return false;
        for (std::size_t i = 0; i < part; ++i) {
            if (check[i] != 0xff) {
                failed_ = true;
                return false;
            }
        }
        done += part;
    }
    // The unencrypted partition API is synchronous; no buffered FS/NVS layer.
    // Any error can mean partial programming. Keep this adapter poisoned.
    if (esp_partition_write(partition_, offset, bytes, count) != ESP_OK) {
        failed_ = true;
        return false;
    }
    for (std::size_t done = 0; done < count;) {
        const auto part = count - done < sizeof(check) ? count - done : sizeof(check);
        if (!read(offset + done, check, part) || std::memcmp(check, bytes + done, part) != 0) {
            failed_ = true;
            return false;
        }
        done += part;
    }
    return true;
}
bool Esp32ProgressPartition::sync() {
    // All programs completed synchronously and were read back. There is no
    // separate partition flush API. Physical power-loss durability is unproved.
    return partition_ && !failed_;
}
