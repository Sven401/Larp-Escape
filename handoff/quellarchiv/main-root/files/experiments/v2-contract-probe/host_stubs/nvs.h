#pragma once
#include <cstddef>
#include <cstdint>
#include <esp_err.h>
using nvs_handle_t = std::uint32_t;
enum nvs_open_mode_t { NVS_READONLY, NVS_READWRITE };
// Values relevant to this deterministic model; actual pinned constants compile
// on SDK. ESP_OK is provided by the existing mount stub in the same suite.
constexpr int ESP_ERR_NVS_NOT_FOUND = 0x1102, ESP_ERR_NVS_TYPE_MISMATCH = 0x1103,
              ESP_ERR_NVS_INVALID_LENGTH = 0x110c;
int nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *);
int nvs_get_blob(nvs_handle_t, const char *, void *, std::size_t *);
int nvs_set_blob(nvs_handle_t, const char *, const void *, std::size_t);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
