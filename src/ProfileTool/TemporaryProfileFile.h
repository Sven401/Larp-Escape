#pragma once
#include "Companion/ProfileFileStaging.h"
#include <cstdio>
// Host-only actual temporary file. No journal path, persistent save or radio.
class HostTemporaryFilePort : public larp::companion::IProfileTemporaryFilePort {
  public:
    bool openTemporary() noexcept override;
    bool writeTemporary(const char *bytes, std::size_t count) noexcept override;
    bool sealTemporary() noexcept override;
    larp::configuration::ReadStatus readTemporary(std::uint8_t &byte) noexcept override;
    bool discardTemporary() noexcept override;

  private:
    std::FILE *file_ = nullptr;
    bool sealed_ = false;
};
class TemporaryProfileFile : public larp::companion::ProfileFileStaging {
  public:
    TemporaryProfileFile() noexcept : ProfileFileStaging(port_) {}
    ~TemporaryProfileFile() override {
        discard();
    }

  private:
    HostTemporaryFilePort port_;
};
