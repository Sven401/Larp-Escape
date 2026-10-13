#pragma once
#include "Companion/ProfileStaging.h"
#include "DeviceRuntime/ProfileApplication.h"
namespace larp {
namespace integration {
// Exclusive sole-application-actor bridge, never a callback/ISR or SMP lock.
// App and backend outlive this object; backend is not otherwise accessed.
// The App fence spans all source IO, including sealed import and cleanup. A
// destructor performs no IO and leaves an App quarantine if still claimed.
class ApplicationProfileStaging : public companion::IProfileStagingStore {
  public:
    ApplicationProfileStaging(devices::ProfileApplication &application,
                              companion::IProfileStagingStore &backend) noexcept
        : application_(application), backend_(backend) {}
    ~ApplicationProfileStaging() override;
    ApplicationProfileStaging(const ApplicationProfileStaging &) = delete;
    ApplicationProfileStaging &operator=(const ApplicationProfileStaging &) = delete;
    bool begin(std::uint32_t bytes) noexcept override;
    bool append(const char *bytes, std::size_t count) noexcept override;
    bool seal() noexcept override;
    bool discard() noexcept override;
    configuration::ReadStatus readByte(std::uint8_t &byte) override;

  private:
    enum class Phase : std::uint8_t { Empty, Receiving, Sealed, Failed };
    devices::ProfileApplication &application_;
    companion::IProfileStagingStore &backend_;
    std::uint32_t limit_ = 0, written_ = 0;
    Phase phase_ = Phase::Empty;
    bool busy_ = false;
};
} // namespace integration
} // namespace larp
