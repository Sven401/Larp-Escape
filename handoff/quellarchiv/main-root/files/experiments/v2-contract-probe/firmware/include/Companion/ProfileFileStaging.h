#pragma once
#include "Companion/ProfileStaging.h"
namespace larp {
namespace companion {
// Trusted synchronous sole-owner file port. No caller-controlled path or mount.
// A failed operation may leave a partial file: discard must close/remove it.
class IProfileTemporaryFilePort {
  public:
    virtual ~IProfileTemporaryFilePort() = default;
    virtual bool openTemporary() noexcept = 0;
    virtual bool writeTemporary(const char *bytes, std::size_t count) noexcept = 0;
    virtual bool sealTemporary() noexcept = 0;
    virtual configuration::ReadStatus readTemporary(std::uint8_t &byte) noexcept = 0;
    virtual bool discardTemporary() noexcept = 0;
};
// Shared exact-length adapter for host and ESP file ports. No heap/Profile;
// each append <=120 bytes, immutable source after seal, no end from live FIFO.
// Failures fence reuse until confirmed discard. Reentrancy fence is NOT SMP.
class ProfileFileStaging : public IProfileStagingStore {
  public:
    explicit ProfileFileStaging(IProfileTemporaryFilePort &file) noexcept : file_(file) {}
    ProfileFileStaging(const ProfileFileStaging &) = delete;
    ProfileFileStaging &operator=(const ProfileFileStaging &) = delete;
    bool begin(std::uint32_t bytes) noexcept override;
    bool append(const char *bytes, std::size_t count) noexcept override;
    bool seal() noexcept override;
    bool discard() noexcept override;
    configuration::ReadStatus readByte(std::uint8_t &byte) override;

  private:
    IProfileTemporaryFilePort &file_;
    std::uint32_t limit_ = 0, written_ = 0, read_ = 0;
    bool started_ = false, sealed_ = false, ended_ = false, failed_ = false, busy_ = false;
};
} // namespace companion
} // namespace larp
