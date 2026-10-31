#pragma once
#include "DeviceRuntime/ProgressJournal.h"
#include <esp_partition.h>

// Native ESP-IDF 6.1 adapter. Sole storage-worker owner; never call from a
// callback/ISR/game tick. No mount, partition provisioning, erase or fallback.
class Esp32ProgressPartition final : public larp::devices::IProgressStore {
  public:
    bool begin();
    std::size_t capacity() const override;
    bool read(std::size_t offset, std::uint8_t *bytes, std::size_t count) override;
    bool program(std::size_t offset, const std::uint8_t *bytes, std::size_t count) override;
    bool sync() override;

  private:
    const esp_partition_t *partition_ = nullptr;
    bool failed_ = false;
    bool attempted_ = false;
    bool inBounds(std::size_t offset, std::size_t count) const;
};
