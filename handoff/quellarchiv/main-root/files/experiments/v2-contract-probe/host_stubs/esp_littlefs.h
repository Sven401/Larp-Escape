#pragma once
#include <cstdint>
#include <esp_err.h>
// Deterministic mount model only. Real pinned SDK header is compiled on ESP.
struct esp_vfs_littlefs_conf_t {
    const char *base_path = nullptr;
    const char *partition_label = nullptr;
    const void *partition = nullptr;
    std::uint8_t format_if_mount_failed = 0, read_only = 0, dont_mount = 0, grow_on_mount = 0;
};
int esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t *configuration);
