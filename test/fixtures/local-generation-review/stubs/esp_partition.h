#pragma once
#include <cstddef>
constexpr int ESP_PARTITION_TYPE_DATA = 1, ESP_PARTITION_SUBTYPE_DATA_NVS = 2;
struct esp_partition_t {
    int type = 1, subtype = 2;
    char label[17] = "larpkeys";
    std::size_t address = 0x310000, size = 0x6000, erase_size = 4096;
    bool encrypted = false, readonly = false;
};
extern "C" const esp_partition_t *esp_partition_find_first(int, int, const char *);
