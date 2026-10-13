#include "Esp32PeerRegistryStore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>
#include <nvs_flash.h>
namespace {
const char *const PeerNamespace = "larp-peers-v1";
const char *const PeerKey = "registry-v1";
StaticSemaphore_t peerNamespaceMutexStorage;
SemaphoreHandle_t peerNamespaceMutex = nullptr;
portMUX_TYPE peerFenceMux = portMUX_INITIALIZER_UNLOCKED;
bool peerNamespaceBlocked = false;
bool peerBlocked() {
    portENTER_CRITICAL(&peerFenceMux);
    const bool value = peerNamespaceBlocked;
    portEXIT_CRITICAL(&peerFenceMux);
    return value;
}
class PeerNamespaceCall {
  public:
    PeerNamespaceCall()
        : locked_(peerNamespaceMutex && xSemaphoreTake(peerNamespaceMutex, 0) == pdTRUE) {}
    ~PeerNamespaceCall() {
        if (locked_)
            xSemaphoreGive(peerNamespaceMutex);
    }
    bool locked() const {
        return locked_;
    }

  private:
    bool locked_;
};
class PeerNvsHandle {
  public:
    ~PeerNvsHandle() {
        if (opened)
            nvs_close(value);
    }
    nvs_handle_t value = 0;
    bool opened = false;
};
larp::companion::PeerRegistryRead peerReadHandle(nvs_handle_t handle,
                                                 larp::companion::PeerRegistryRecord &out) {
    using namespace larp::companion;
    std::size_t count = 0;
    auto result = nvs_get_blob(handle, PeerKey, nullptr, &count);
    if (peerBlocked())
        return PeerRegistryRead::Failure;
    if (result == ESP_ERR_NVS_NOT_FOUND)
        return PeerRegistryRead::Missing;
    PeerRegistryRecord owned;
    if (result != ESP_OK || count != owned.bytes.size())
        return PeerRegistryRead::Failure;
    result = nvs_get_blob(handle, PeerKey, owned.bytes.data(), &count);
    if (result != ESP_OK || count != owned.bytes.size() || peerBlocked())
        return PeerRegistryRead::Failure;
    out = owned;
    return PeerRegistryRead::Present;
}
} // namespace
using namespace larp::companion;
void Esp32PeerRegistryStore::fence() noexcept {
    portENTER_CRITICAL(&peerFenceMux);
    peerNamespaceBlocked = true;
    portEXIT_CRITICAL(&peerFenceMux);
}
bool Esp32PeerRegistryStore::begin() noexcept {
    if (peerBlocked())
        return false;
    if (initialized_)
        return true;
    // Sole trusted startup owner before workers; no reset of a shared fence.
    if (!peerNamespaceMutex)
        peerNamespaceMutex = xSemaphoreCreateMutexStatic(&peerNamespaceMutexStorage);
    PeerNamespaceCall call;
    if (!call.locked())
        return false;
    initialized_ = nvs_flash_init() == ESP_OK && !peerBlocked();
    return initialized_;
}
PeerRegistryRead Esp32PeerRegistryStore::read(PeerRegistryRecord &destination) noexcept {
    if (!initialized_ || peerBlocked())
        return PeerRegistryRead::Failure;
    PeerNamespaceCall call;
    if (!call.locked() || peerBlocked())
        return PeerRegistryRead::Failure;
    PeerNvsHandle handle;
    const auto result = nvs_open(PeerNamespace, NVS_READONLY, &handle.value);
    if (result == ESP_OK)
        handle.opened = true;
    if (peerBlocked())
        return PeerRegistryRead::Failure;
    if (result == ESP_ERR_NVS_NOT_FOUND)
        return PeerRegistryRead::Missing;
    if (result != ESP_OK)
        return PeerRegistryRead::Failure;
    return peerReadHandle(handle.value, destination);
}
PeerRegistryWrite Esp32PeerRegistryStore::replace(const PeerRegistryRecord *callerExpected,
                                                  const PeerRegistryRecord &caller) noexcept {
    const bool absent = callerExpected == nullptr;
    const auto expected = absent ? PeerRegistryRecord{} : *callerExpected;
    const auto next = caller;
    if (!initialized_ || peerBlocked())
        return PeerRegistryWrite::Failure;
    // One decode workspace: next and expected are validated sequentially.
    // Both original records stay frozen for the complete CAS operation.
    PeerRegistrySnapshot decoded;
    if (!decodePeerRegistry(next, decoded))
        return PeerRegistryWrite::Failure;
    const auto nextGeneration = decoded.generation;
    const auto device = decoded.device;
    if (absent ? nextGeneration != 1
               : (!decodePeerRegistry(expected, decoded) || decoded.generation == UINT32_MAX ||
                  nextGeneration != decoded.generation + 1 || !(device == decoded.device)))
        return PeerRegistryWrite::Failure;
    PeerNamespaceCall call;
    if (!call.locked() || peerBlocked())
        return PeerRegistryWrite::Failure;
    PeerNvsHandle handle;
    if (nvs_open(PeerNamespace, NVS_READWRITE, &handle.value) != ESP_OK)
        return PeerRegistryWrite::Failure;
    handle.opened = true;
    PeerRegistryRecord observed;
    const auto read = peerReadHandle(handle.value, observed);
    if (read == PeerRegistryRead::Failure)
        return PeerRegistryWrite::Failure;
    if (absent ? read != PeerRegistryRead::Missing
               : (read != PeerRegistryRead::Present || !(observed == expected)))
        return PeerRegistryWrite::Conflict;
    if (peerBlocked())
        return PeerRegistryWrite::Failure;
    if (nvs_set_blob(handle.value, PeerKey, next.bytes.data(), next.bytes.size()) != ESP_OK ||
        nvs_commit(handle.value) != ESP_OK) {
        fence();
        return PeerRegistryWrite::Uncertain;
    }
    // The pre-write observation is no longer needed; reuse its fixed buffer.
    if (peerReadHandle(handle.value, observed) != PeerRegistryRead::Present ||
        !(observed == next) || peerBlocked()) {
        fence();
        return PeerRegistryWrite::Uncertain;
    }
    return PeerRegistryWrite::Confirmed;
}
