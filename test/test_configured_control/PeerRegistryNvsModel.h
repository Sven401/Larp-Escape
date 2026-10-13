#pragma once
#include "../../lib/Esp32PeerRegistryStore/Esp32PeerRegistryStore.h"
#include "Companion/PeerRegistry.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <nvs.h>
#include <vector>
namespace host_peer_nvs {
std::map<nvs_handle_t, bool> handles;
std::vector<std::uint8_t> value, pending;
bool namespacePresent = false, present = false, persistSetError = false, persistCommitError = false;
int openError = 0, getError = 0, setError = 0, commitError = 0;
unsigned opens = 0, gets = 0, sets = 0, commits = 0, closes = 0, unknown = 0;
nvs_handle_t nextHandle = 10001;
void (*beforeGet)() = nullptr;
void (*beforeSet)() = nullptr;
void (*afterCommit)() = nullptr;
void reset() {
    if (!handles.empty())
        std::abort();
    value.clear();
    pending.clear();
    namespacePresent = present = persistSetError = persistCommitError = false;
    openError = getError = setError = commitError = 0;
    opens = gets = sets = commits = closes = unknown = 0;
    nextHandle = 10001;
    beforeGet = beforeSet = afterCommit = nullptr;
}
int open(nvs_open_mode_t mode, nvs_handle_t *out) {
    ++opens;
    if (openError)
        return openError;
    if (mode == NVS_READONLY && !namespacePresent)
        return ESP_ERR_NVS_NOT_FOUND;
    namespacePresent = true;
    const auto handle = nextHandle++;
    handles.emplace(handle, mode == NVS_READWRITE);
    *out = handle;
    return ESP_OK;
}
int get(nvs_handle_t handle, const char *key, void *out, std::size_t *count) {
    ++gets;
    if (std::strcmp(key, "registry-v1") || !handles.count(handle)) {
        ++unknown;
        return 1;
    }
    if (beforeGet) {
        auto hook = beforeGet;
        beforeGet = nullptr;
        hook();
    }
    if (getError)
        return getError;
    if (!present)
        return ESP_ERR_NVS_NOT_FOUND;
    if (!out) {
        *count = value.size();
        return ESP_OK;
    }
    if (*count < value.size()) {
        *count = value.size();
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    std::memcpy(out, value.data(), value.size());
    *count = value.size();
    return ESP_OK;
}
int set(nvs_handle_t handle, const char *key, const void *source, std::size_t count) {
    ++sets;
    if (std::strcmp(key, "registry-v1") || !handles.count(handle) || !handles.at(handle) ||
        count != 404) {
        ++unknown;
        return 1;
    }
    if (beforeSet) {
        auto hook = beforeSet;
        beforeSet = nullptr;
        hook();
    }
    const auto *bytes = static_cast<const std::uint8_t *>(source);
    pending.assign(bytes, bytes + count);
    if (setError && persistSetError) {
        value = pending;
        present = true;
    }
    return setError;
}
int commit(nvs_handle_t handle) {
    ++commits;
    if (!handles.count(handle) || !handles.at(handle)) {
        ++unknown;
        return 1;
    }
    if (!commitError || persistCommitError) {
        value = pending;
        present = true;
    }
    if (afterCommit) {
        auto hook = afterCommit;
        afterCommit = nullptr;
        hook();
    }
    return commitError;
}
void close(nvs_handle_t handle) {
    ++closes;
    handles.erase(handle);
}
} // namespace host_peer_nvs
