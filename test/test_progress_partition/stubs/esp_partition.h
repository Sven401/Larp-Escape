#pragma once
#include <cstddef>
#include <cstdint>
typedef int esp_err_t;
constexpr esp_err_t ESP_OK = 0;
enum esp_partition_type_t { ESP_PARTITION_TYPE_DATA = 1 };
enum esp_partition_subtype_t { ESP_PARTITION_SUBTYPE_DATA_UNDEFINED = 6 };
struct esp_partition_t {
    esp_partition_type_t type;
    esp_partition_subtype_t subtype;
    std::uint32_t address, size, erase_size;
    char label[17];
    bool encrypted, readonly;
};
const esp_partition_t *esp_partition_find_first(esp_partition_type_t, esp_partition_subtype_t,
                                                const char *);
esp_err_t esp_partition_read(const esp_partition_t *, std::size_t, void *, std::size_t);
esp_err_t esp_partition_write(const esp_partition_t *, std::size_t, const void *, std::size_t);
// Intentionally no erase API: using it makes the adapter test fail to compile.
