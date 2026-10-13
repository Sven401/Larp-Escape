#include "Esp32DeviceIdentityStore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>
#include <nvs_flash.h>
namespace {
const char *const Namespace = "larp-companion";
const char *const Key = "device-id-v1";
StaticSemaphore_t namespaceMutexStorage;
SemaphoreHandle_t namespaceMutex = nullptr;
class NamespaceCall {
  public:
    NamespaceCall() : locked_(namespaceMutex && xSemaphoreTake(namespaceMutex, 0) == pdTRUE) {}
    ~NamespaceCall() {
        if (locked_)
            xSemaphoreGive(namespaceMutex);
    }
    bool locked() const noexcept {
        return locked_;
    }

  private:
    bool locked_;
};
class NvsHandle {
  public:
    ~NvsHandle() {
        if (opened)
            nvs_close(value);
    }
    nvs_handle_t value = 0;
    bool opened = false;
};
} // namespace
using namespace larp::companion;
bool Esp32DeviceIdentityStore::begin() noexcept {
    if (initialized_)
        return true;
    // Single trusted startup owner must create this mutex before workers.
    if (!namespaceMutex)
        namespaceMutex = xSemaphoreCreateMutexStatic(&namespaceMutexStorage);
    NamespaceCall call;
    if (!call.locked())
        return false;
    initialized_ = nvs_flash_init() == ESP_OK;
    return initialized_; // No erase/reinit fallback on a full/corrupt partition.
}
IdentityRecordRead Esp32DeviceIdentityStore::read(DeviceIdentityRecord &destination) noexcept {
    if (!initialized_)
        return IdentityRecordRead::Failure;
    NamespaceCall call;
    if (!call.locked())
        return IdentityRecordRead::Failure;
    NvsHandle handle;
    const auto opened = nvs_open(Namespace, NVS_READONLY, &handle.value);
    if (opened == ESP_ERR_NVS_NOT_FOUND)
        return IdentityRecordRead::Missing;
    if (opened != ESP_OK)
        return IdentityRecordRead::Failure;
    handle.opened = true;
    std::size_t length = 0;
    auto result = nvs_get_blob(handle.value, Key, nullptr, &length);
    if (result == ESP_ERR_NVS_NOT_FOUND)
        return IdentityRecordRead::Missing;
    DeviceIdentityRecord selected;
    if (result != ESP_OK || length != selected.bytes.size())
        return IdentityRecordRead::Failure;
    result = nvs_get_blob(handle.value, Key, selected.bytes.data(), &length);
    if (result != ESP_OK || length != selected.bytes.size())
        return IdentityRecordRead::Failure;
    destination = selected;
    return IdentityRecordRead::Present;
}
IdentityRecordCreate
Esp32DeviceIdentityStore::createIfMissing(const DeviceIdentityRecord &caller) noexcept {
    const auto record = caller;
    Identity identity;
    if (!initialized_ || !decodeDeviceIdentityRecord(record, identity))
        return IdentityRecordCreate::Failure;
    NamespaceCall call;
    if (!call.locked())
        return IdentityRecordCreate::Failure;
    NvsHandle handle;
    if (nvs_open(Namespace, NVS_READWRITE, &handle.value) != ESP_OK)
        return IdentityRecordCreate::Failure;
    handle.opened = true;
    std::size_t length = 0;
    const auto exists = nvs_get_blob(handle.value, Key, nullptr, &length);
    if (exists == ESP_OK)
        return IdentityRecordCreate::AlreadyPresent;
    if (exists != ESP_ERR_NVS_NOT_FOUND)
        return IdentityRecordCreate::Failure;
    if (nvs_set_blob(handle.value, Key, record.bytes.data(), record.bytes.size()) != ESP_OK ||
        nvs_commit(handle.value) != ESP_OK)
        return IdentityRecordCreate::Uncertain;
    return IdentityRecordCreate::Confirmed;
}
