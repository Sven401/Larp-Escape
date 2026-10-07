#pragma once
#include "../../lib/Esp32DeviceIdentityStore/Esp32DeviceIdentityStore.h"
#include "../../lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.h"
#include <bootloader_random.h>
#include <cstdlib>
#include <cstring>
#include <esp_random.h>
#include <freertos/semphr.h>
#include <map>
#include <nvs.h>
#include <nvs_flash.h>
#include <vector>
namespace host_mutex {
bool failCreate = false, forceBusy = false;
unsigned takes = 0, gives = 0, creates = 0;
} // namespace host_mutex
namespace host_identity_nvs {
std::vector<std::uint8_t> value, pending;
std::map<nvs_handle_t, bool> handles;
bool namespacePresent = false, present = false, persistSetError = false, persistCommitError = false;
int initError = 0, openError = 0, getError = 0, setError = 0, commitError = 0;
unsigned inits = 0, opens = 0, getCalls = 0, sets = 0, commits = 0, closes = 0, unknown = 0;
nvs_handle_t nextHandle = 1;
void (*beforeSet)() = nullptr;
void (*beforeGet)() = nullptr;
larp::companion::DeviceIdentityRecord *caller = nullptr;
Esp32DeviceIdentityStore *nestedStore = nullptr;
larp::companion::IdentityRecordRead nestedResult = larp::companion::IdentityRecordRead::Present;
unsigned enables = 0, fills = 0, disables = 0, entropyOrder = 0;
bool entropyActive = false, zeroEntropy = false, badEntropyOrder = false;
void reset() {
    if (!handles.empty())
        std::abort();
    value.clear();
    pending.clear();
    namespacePresent = present = persistSetError = persistCommitError = false;
    initError = openError = getError = setError = commitError = 0;
    inits = opens = getCalls = sets = commits = closes = unknown = 0;
    nextHandle = 1;
    beforeSet = beforeGet = nullptr;
    caller = nullptr;
    nestedStore = nullptr;
    enables = fills = disables = entropyOrder = 0;
    entropyActive = zeroEntropy = badEntropyOrder = false;
    host_mutex::forceBusy = host_mutex::failCreate = false;
    host_mutex::takes = host_mutex::gives = host_mutex::creates = 0;
}
} // namespace host_identity_nvs
int nvs_flash_init() {
    ++host_identity_nvs::inits;
    return host_identity_nvs::initError;
}
int nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *destination) {
    using namespace host_identity_nvs;
    ++opens;
    if (std::strcmp(name, "larp-companion") != 0) {
        ++unknown;
        return 1;
    }
    if (openError)
        return openError;
    if (mode == NVS_READONLY && !namespacePresent)
        return ESP_ERR_NVS_NOT_FOUND;
    namespacePresent = true;
    const auto handle = nextHandle++;
    handles.emplace(handle, mode == NVS_READWRITE);
    *destination = handle;
    return ESP_OK;
}
int nvs_get_blob(nvs_handle_t handle, const char *key, void *destination, std::size_t *length) {
    using namespace host_identity_nvs;
    ++getCalls;
    if (std::strcmp(key, "device-id-v1") != 0 || !handles.count(handle)) {
        ++unknown;
        return 1;
    }
    if (beforeGet) {
        auto selected = beforeGet;
        beforeGet = nullptr;
        selected();
    }
    if (getError)
        return getError;
    if (!present)
        return ESP_ERR_NVS_NOT_FOUND;
    if (!destination) {
        *length = value.size();
        return ESP_OK;
    }
    if (*length < value.size()) {
        *length = value.size();
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    std::memcpy(destination, value.data(), value.size());
    *length = value.size();
    return ESP_OK;
}
int nvs_set_blob(nvs_handle_t handle, const char *key, const void *source, std::size_t count) {
    using namespace host_identity_nvs;
    ++sets;
    if (std::strcmp(key, "device-id-v1") != 0 || !handles.count(handle) || !handles.at(handle) ||
        count != 28) {
        ++unknown;
        return 1;
    }
    if (beforeSet) {
        auto selected = beforeSet;
        beforeSet = nullptr;
        selected();
    }
    const auto bytes = static_cast<const std::uint8_t *>(source);
    pending.assign(bytes, bytes + count);
    if (setError && persistSetError) {
        value = pending;
        present = true;
    }
    return setError;
}
int nvs_commit(nvs_handle_t handle) {
    using namespace host_identity_nvs;
    ++commits;
    if (!handles.count(handle) || !handles.at(handle)) {
        ++unknown;
        return 1;
    }
    if (!commitError || persistCommitError) {
        value = pending;
        present = true;
    }
    return commitError;
}
void nvs_close(nvs_handle_t handle) {
    ++host_identity_nvs::closes;
    host_identity_nvs::handles.erase(handle);
}
void bootloader_random_enable() {
    using namespace host_identity_nvs;
    ++enables;
    badEntropyOrder = badEntropyOrder || entropyActive || entropyOrder != 0;
    entropyActive = true;
    entropyOrder = 1;
}
void esp_fill_random(void *destination, std::size_t count) {
    using namespace host_identity_nvs;
    ++fills;
    badEntropyOrder = badEntropyOrder || !entropyActive || entropyOrder != 1 || count != 16;
    entropyOrder = 2;
    auto bytes = static_cast<std::uint8_t *>(destination);
    for (unsigned i = 0; i < count; ++i)
        bytes[i] = zeroEntropy ? 0 : static_cast<std::uint8_t>(i + 1);
}
void bootloader_random_disable() {
    using namespace host_identity_nvs;
    ++disables;
    badEntropyOrder = badEntropyOrder || !entropyActive || entropyOrder != 2;
    entropyActive = false;
    entropyOrder = 0;
}
// Compile the actual SDK adapter bodies against the copied NVS/mutex model.
#include "../../lib/Esp32DeviceIdentityStore/Esp32DeviceIdentityStore.cpp"
#include "../../lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.cpp"
