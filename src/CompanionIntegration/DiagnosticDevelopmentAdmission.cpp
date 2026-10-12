#include "CompanionIntegration/DiagnosticDevelopmentAdmission.h"

#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
namespace larp {
namespace diagnostic {
namespace {
const std::uint64_t MaximumDeviceMs = 9007199254740991ULL;
bool usable(const DevelopmentLink &link, const companion::Identity &device) noexcept {
    return link.encrypted && link.bonded && link.original.device == device &&
           link.original.connection.token && link.original.registration &&
           link.original.lifecycle && link.original.interface != 0xff;
}
} // namespace

bool DevelopmentAdmission::clock(std::uint64_t now) noexcept {
    if (clockFault_ || now > MaximumDeviceMs || now < lastNow_) {
        active_ = false;
        clockFault_ = true;
        return false;
    }
    lastNow_ = now;
    if (active_ && now >= expires_)
        active_ = false;
    return true;
}

bool DevelopmentAdmission::current(const DevelopmentLink &link) const noexcept {
    return active_ && usable(link, device_) && link.original == owner_;
}

DevelopmentAdmissionResult DevelopmentAdmission::begin(
    const DevelopmentLink &link, const Uuid &run, const Uuid &deviceGrant,
    std::uint32_t ttlMs, bool explicitConsent, std::uint64_t now) noexcept {
    if (!clock(now))
        return DevelopmentAdmissionResult::Unavailable;
    if (active_)
        return DevelopmentAdmissionResult::Busy;
    if (!device_.valid() || !boot_ || !run.valid() || !deviceGrant.valid() ||
        !explicitConsent || !ttlMs || ttlMs > 60000 ||
        now > MaximumDeviceMs - ttlMs || run == deviceGrant)
        return DevelopmentAdmissionResult::Invalid;
    if (!usable(link, device_))
        return DevelopmentAdmissionResult::Unavailable;
    for (std::size_t i = 0; i < used_; ++i)
        if (usedRuns_[i] == run || usedGrants_[i] == deviceGrant)
            return DevelopmentAdmissionResult::Replay;
    if (used_ == MaximumRuns)
        return DevelopmentAdmissionResult::Capacity;
    usedRuns_[used_] = run;
    usedGrants_[used_++] = deviceGrant;
    owner_ = link.original;
    run_ = run;
    grant_ = deviceGrant;
    expires_ = now + ttlMs;
    lastStep_ = 0;
    active_ = true;
    return DevelopmentAdmissionResult::Accepted;
}

DevelopmentAdmissionResult DevelopmentAdmission::step(
    const DevelopmentLink &link, const Uuid &run, const Uuid &grant,
    std::uint32_t stepId, std::uint32_t timeoutMs, std::uint64_t now) noexcept {
    if (!clock(now))
        return DevelopmentAdmissionResult::Unavailable;
    if (!current(link)) {
        active_ = false;
        return DevelopmentAdmissionResult::StaleLink;
    }
    if (!(run == run_) || !(grant == grant_) || !stepId ||
        !timeoutMs || timeoutMs > 60000 || now > MaximumDeviceMs - timeoutMs ||
        now + timeoutMs > expires_)
        return DevelopmentAdmissionResult::Invalid;
    if (stepId <= lastStep_)
        return DevelopmentAdmissionResult::Replay;
    lastStep_ = stepId;
    return DevelopmentAdmissionResult::Accepted;
}

bool DevelopmentAdmission::resultCurrent(
    const DevelopmentLink &link, const Uuid &run, const Uuid &grant,
    std::uint32_t stepId, std::uint64_t now) noexcept {
    return clock(now) && current(link) && run == run_ && grant == grant_ &&
           stepId == lastStep_;
}

void DevelopmentAdmission::disconnect(
    const companion::PeerTransportBinding &link) noexcept {
    if (active_ && link == owner_)
        active_ = false;
}

DevelopmentAdmissionResult DevelopmentAdmission::cancel(
    const DevelopmentLink &link, const Uuid &run, const Uuid &grant,
    std::uint64_t now) noexcept {
    if (!clock(now))
        return DevelopmentAdmissionResult::Unavailable;
    if (!current(link)) {
        active_ = false;
        return DevelopmentAdmissionResult::StaleLink;
    }
    if (!(run == run_) || !(grant == grant_))
        return DevelopmentAdmissionResult::Invalid;
    active_ = false;
    return DevelopmentAdmissionResult::Accepted;
}

} // namespace diagnostic
} // namespace larp
#endif
