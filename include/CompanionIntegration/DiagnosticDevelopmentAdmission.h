#pragma once

#include "CompanionIntegration/DiagnosticControl.h"
#include "Companion/PeerLinkSecurity.h"

#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
namespace larp {
namespace diagnostic {

// Evidence copied by the original BLE owner. No field may come from a client
// frame or Android's local bond display. This type never grants a product role.
struct DevelopmentLink {
    companion::PeerTransportBinding original;
    bool encrypted = false;
    bool bonded = false;
};

enum class DevelopmentAdmissionResult : std::uint8_t {
    Accepted, Invalid, Unavailable, StaleLink, Expired, Replay, Busy, Capacity
};

// Test-image-only session fence. The SDK adapter must serialize calls with
// original-link lifecycle and component execution; this class is not a lock.
// Consumes a step before returning Accepted so a lost reply cannot replay it.
class DevelopmentAdmission {
  public:
    DevelopmentAdmission(const companion::Identity &device, std::uint64_t boot) noexcept
        : device_(device), boot_(boot) {}
    DevelopmentAdmissionResult begin(const DevelopmentLink &, const Uuid &run,
                                     const Uuid &deviceGrant, std::uint32_t ttlMs,
                                     bool explicitConsent, std::uint64_t now) noexcept;
    DevelopmentAdmissionResult step(const DevelopmentLink &, const Uuid &run,
                                    const Uuid &grant, std::uint32_t stepId,
                                    std::uint32_t timeoutMs, std::uint64_t now) noexcept;
    void disconnect(const companion::PeerTransportBinding &) noexcept;
    DevelopmentAdmissionResult cancel(const DevelopmentLink &, const Uuid &run,
                                      const Uuid &grant, std::uint64_t now) noexcept;
    bool active() const noexcept { return active_; }
    std::uint64_t boot() const noexcept { return boot_; }

  private:
    static const std::size_t MaximumRuns = 128;
    companion::Identity device_;
    std::uint64_t boot_ = 0, expires_ = 0, lastNow_ = 0;
    companion::PeerTransportBinding owner_;
    Uuid run_, grant_;
    std::array<Uuid, MaximumRuns> usedRuns_ = {};
    std::array<Uuid, MaximumRuns> usedGrants_ = {};
    std::size_t used_ = 0;
    std::uint32_t lastStep_ = 0;
    bool active_ = false, clockFault_ = false;
    bool current(const DevelopmentLink &) const noexcept;
    bool clock(std::uint64_t) noexcept;
};

} // namespace diagnostic
} // namespace larp
#endif
