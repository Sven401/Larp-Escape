#include "Esp32LocalGenerationStore.h"
#include <cstring>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>
#include <nvs_flash.h>
namespace {
constexpr const char *Partition = "larpkeys";
constexpr const char *Namespace = "larp-local-v1";
constexpr const char *Key = "boot-v1";
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex = nullptr;
bool blocked = false;
struct Lock {
    bool held = mutex && xSemaphoreTake(mutex, 0) == pdTRUE;
    ~Lock() {
        if (held)
            xSemaphoreGive(mutex);
    }
};
struct Handle {
    nvs_handle_t value = 0;
    bool opened = false;
    void close() {
        if (opened) {
            nvs_close(value);
            opened = false;
        }
    }
    ~Handle() {
        close();
    }
};
using namespace larp::devices;
LocalGenerationStatus read(Handle &handle, LocalGenerationRecord &out) {
    std::size_t count = 0;
    const auto size = nvs_get_blob(handle.value, Key, nullptr, &count);
    if (size == ESP_ERR_NVS_NOT_FOUND)
        return LocalGenerationStatus::Missing;
    if (size != ESP_OK)
        return LocalGenerationStatus::Unavailable;
    if (count != out.bytes.size())
        return LocalGenerationStatus::Corrupt;
    count = out.bytes.size();
    if (nvs_get_blob(handle.value, Key, out.bytes.data(), &count) != ESP_OK)
        return LocalGenerationStatus::Unavailable;
    return count == out.bytes.size() ? LocalGenerationStatus::Reserved
                                     : LocalGenerationStatus::Corrupt;
}
} // namespace
larp::devices::LocalGenerationStatus
Esp32LocalGenerationStore::reserve(const larp::execution::rules::Identity &boot,
                                   LocalGenerationSnapshot &out) noexcept {
    out = {};
    if (!mutex)
        mutex = xSemaphoreCreateMutexStatic(&mutexStorage);
    Lock lock;
    if (!lock.held || blocked)
        return LocalGenerationStatus::Unavailable;
    const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                     ESP_PARTITION_SUBTYPE_DATA_NVS, Partition);
    if (!partition || partition->type != ESP_PARTITION_TYPE_DATA ||
        partition->subtype != ESP_PARTITION_SUBTYPE_DATA_NVS ||
        std::strncmp(partition->label, Partition, sizeof(partition->label)) ||
        partition->address != 0x310000 || partition->size != 0x6000 ||
        partition->erase_size != 4096 || partition->encrypted || partition->readonly)
        return LocalGenerationStatus::Unavailable;
    if (nvs_flash_init_partition(Partition) != ESP_OK)
        return LocalGenerationStatus::Unavailable;
    Handle initial;
    const auto opened = nvs_open_from_partition(Partition, Namespace, NVS_READONLY, &initial.value);
    if (opened == ESP_ERR_NVS_NOT_FOUND)
        return LocalGenerationStatus::Missing;
    if (opened != ESP_OK)
        return LocalGenerationStatus::Unavailable;
    initial.opened = true;
    LocalGenerationRecord expected;
    const auto observed = read(initial, expected);
    if (observed != LocalGenerationStatus::Reserved)
        return observed;
    LocalGenerationSnapshot next;
    if (!decodeLocalGeneration(expected, next))
        return LocalGenerationStatus::Corrupt;
    if (next.generation == UINT64_MAX)
        return LocalGenerationStatus::Overflow;
    ++next.generation;
    next.boot = boot;
    LocalGenerationRecord candidate;
    if (!encodeLocalGeneration(next, candidate))
        return LocalGenerationStatus::Corrupt;
    initial.close();
    Handle writer;
    if (nvs_open_from_partition(Partition, Namespace, NVS_READWRITE, &writer.value) != ESP_OK)
        return LocalGenerationStatus::Unavailable;
    writer.opened = true;
    LocalGenerationRecord current;
    const auto compared = read(writer, current);
    if (compared != LocalGenerationStatus::Reserved)
        return compared;
    if (!(current == expected))
        return LocalGenerationStatus::Conflict;
    // From the first set call onward failure can mean an effective write.
    // Fence this process; no new instance/handle can retry the reservation.
    if (nvs_set_blob(writer.value, Key, candidate.bytes.data(), candidate.bytes.size()) != ESP_OK ||
        nvs_commit(writer.value) != ESP_OK) {
        blocked = true;
        return LocalGenerationStatus::Uncertain;
    }
    writer.close();
    Handle verify;
    if (nvs_open_from_partition(Partition, Namespace, NVS_READONLY, &verify.value) != ESP_OK) {
        blocked = true;
        return LocalGenerationStatus::Uncertain;
    }
    verify.opened = true;
    if (read(verify, current) != LocalGenerationStatus::Reserved || !(current == candidate)) {
        blocked = true;
        return LocalGenerationStatus::Uncertain;
    }
    // Readback is not hardware power-loss qualification or anti-rollback.
    out = next;
    return LocalGenerationStatus::Reserved;
}
