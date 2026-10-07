#pragma once
#include "Companion/ProfileStaging.h"
#include <cstdio>
// Host-only actual temporary file. No journal path, persistent save or radio.
class TemporaryProfileFile : public larp::companion::IProfileStagingStore {
  public:
    ~TemporaryProfileFile() override {
        discard();
    }
    TemporaryProfileFile() = default;
    TemporaryProfileFile(const TemporaryProfileFile &) = delete;
    TemporaryProfileFile &operator=(const TemporaryProfileFile &) = delete;
    bool begin(std::uint32_t bytes) noexcept override;
    bool append(const char *bytes, std::size_t count) noexcept override;
    bool seal() noexcept override;
    bool discard() noexcept override;
    larp::configuration::ReadStatus readByte(std::uint8_t &byte) override;

  private:
    std::FILE *file_ = nullptr;
    std::uint32_t limit_ = 0, written_ = 0, read_ = 0;
    bool sealed_ = false, ended_ = false;
};
