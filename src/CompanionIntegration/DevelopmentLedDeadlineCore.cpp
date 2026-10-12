#include "CompanionIntegration/DevelopmentLedDeadlineCore.h"

namespace larp {
namespace diagnostic {

bool DevelopmentLedDeadlineCore::arm(std::uint32_t ticket,
                                     std::uint64_t startedUs) noexcept {
    if (!ready() || !ticket || ticket <= completed_ || !startedUs)
        return false;
    active_ = ticket;
    startedUs_ = startedUs;
    timerFired_ = false;
    blackRequested_ = false;
    blackSubmitted_ = false;
    return true;
}

bool DevelopmentLedDeadlineCore::requestStop(std::uint32_t ticket) noexcept {
    if (faulted_ || !ticket || ticket != active_ || blackSubmitted_)
        return false;
    blackRequested_ = true;
    return true;
}

bool DevelopmentLedDeadlineCore::timerFired(std::uint32_t ticket) noexcept {
    if (faulted_ || !ticket || ticket != active_ || timerFired_)
        return false;
    timerFired_ = true;
    if (blackSubmitted_)
        finish();
    else
        blackRequested_ = true;
    return true;
}

bool DevelopmentLedDeadlineCore::blackSubmitted(std::uint32_t ticket,
                                                 std::uint64_t finishedUs) noexcept {
    if (faulted_ || !ticket || ticket != active_ || !blackRequested_ ||
        blackSubmitted_ || finishedUs < startedUs_ ||
        finishedUs - startedUs_ > MaximumOffUs) {
        if (ticket && ticket == active_) fault(ticket);
        return false;
    }
    blackSubmitted_ = true;
    submitted_ = ticket;
    if (timerFired_) finish();
    return true;
}

void DevelopmentLedDeadlineCore::fault(std::uint32_t ticket) noexcept {
    faulted_ = true;
    if (ticket) missed_ = ticket;
}

void DevelopmentLedDeadlineCore::finish() noexcept {
    completed_ = active_;
    active_ = 0;
    startedUs_ = 0;
    blackRequested_ = false;
}

} // namespace diagnostic
} // namespace larp
