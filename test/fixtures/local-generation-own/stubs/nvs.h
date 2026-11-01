#pragma once
#include <cstddef>
using nvs_handle_t = unsigned;
constexpr int ESP_OK = 0, ESP_ERR_NVS_NOT_FOUND = 1, NVS_READONLY = 0, NVS_READWRITE = 1;
extern "C" int nvs_open_from_partition(const char *, const char *, int, nvs_handle_t *);
extern "C" int nvs_get_blob(nvs_handle_t, const char *, void *, std::size_t *);
extern "C" int nvs_set_blob(nvs_handle_t, const char *, const void *, std::size_t);
extern "C" int nvs_commit(nvs_handle_t);
extern "C" void nvs_close(nvs_handle_t);
