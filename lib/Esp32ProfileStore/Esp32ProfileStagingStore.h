#pragma once
#include "Companion/ProfileFileStaging.h"
#include "Esp32ProfileStore.h"
// Same sole stopped application owner as the mount/journal. No worker/callback,
// automatic mount/format or client path. One claimed named temporary file.
class Esp32TemporaryFilePort : public larp::companion::IProfileTemporaryFilePort {
  public:
    explicit Esp32TemporaryFilePort(Esp32ProfileStore &owner) noexcept : owner_(owner) {}
    ~Esp32TemporaryFilePort() override;
    bool openTemporary() noexcept override;
    bool writeTemporary(const char *bytes, std::size_t count) noexcept override;
    bool sealTemporary() noexcept override;
    larp::configuration::ReadStatus readTemporary(std::uint8_t &byte) noexcept override;
    bool discardTemporary() noexcept override;

  private:
    bool claim(bool cleanup = false) noexcept;
    Esp32ProfileStore &owner_;
    std::FILE *file_ = nullptr;
    bool writing_ = false;
};
// Child must be destroyed before the process-lifetime mount owner. Cleanup only
// profile-source.tmp; neither journal record is opened, removed or relabelled.
class Esp32ProfileStagingStore : public larp::companion::ProfileFileStaging {
  public:
    explicit Esp32ProfileStagingStore(Esp32ProfileStore &owner) noexcept
        : ProfileFileStaging(port_), port_(owner) {}
    ~Esp32ProfileStagingStore() override {
        discard();
    }

  private:
    Esp32TemporaryFilePort port_;
};
