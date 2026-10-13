#include "CompanionIntegration/LedControlV1.h"

namespace larp {
namespace ledcontrol {

bool LedControlV1::valid(const Selection &s) noexcept {
    const auto effect = static_cast<std::uint8_t>(s.effect);
    return s.first < LedCount && s.count > 0 &&
           static_cast<unsigned>(s.first) + s.count <= LedCount &&
           (s.red || s.green || s.blue) && s.red <= 32 &&
           s.green <= 32 && s.blue <= 32 &&
           s.brightness >= 1 && s.brightness <= 32 && effect <= 2 &&
           s.durationMs >= MinimumDurationMs &&
           s.durationMs <= MaximumDurationMs;
}

Snapshot LedControlV1::start(std::uint32_t requestId, std::uint32_t runId,
                             const Selection &selection,
                             std::uint32_t nowMs) noexcept {
    Snapshot answer = state_;
    answer.requestId = requestId;
    if (!requestId || requestId <= lastRequestId_ || !runId ||
        runId <= lastRunId_) {
        answer.outcome = Outcome::RejectedStale;
        return answer;
    }
    lastRequestId_ = requestId;
    if (!valid(selection)) {
        answer.outcome = Outcome::RejectedInvalid;
        return answer;
    }
    if (state_.phase == Phase::Running || state_.phase == Phase::Fault) {
        answer.outcome = Outcome::RejectedBusy;
        return answer;
    }
    lastRunId_ = runId;
    state_ = {};
    state_.phase = Phase::Running;
    state_.outcome = Outcome::Accepted;
    state_.requestId = requestId;
    state_.runId = runId;
    state_.startedAtMs = nowMs;
    state_.deadlineMs = nowMs + selection.durationMs;
    offRequested_ = false;
    return state_;
}

Snapshot LedControlV1::stop(std::uint32_t requestId, std::uint32_t runId,
                            std::uint32_t nowMs) noexcept {
    Snapshot answer = state_;
    answer.requestId = requestId;
    if (!requestId || requestId <= lastRequestId_ || !runId ||
        runId != state_.runId) {
        answer.outcome = Outcome::RejectedStale;
        return answer;
    }
    lastRequestId_ = requestId;
    if (state_.phase != Phase::Running) {
        answer.outcome = Outcome::RejectedBusy;
        return answer;
    }
    offRequested_ = true;
    state_.requestId = requestId;
    state_.outcome = reached(nowMs, state_.deadlineMs)
        ? Outcome::Expired : Outcome::Stopped;
    return state_;
}

bool LedControlV1::offDue(std::uint32_t nowMs) const noexcept {
    return (state_.phase == Phase::Running &&
            (offRequested_ || reached(nowMs, state_.deadlineMs))) ||
           (state_.phase == Phase::Fault && offRequested_);
}

Snapshot LedControlV1::blackSubmitted(std::uint32_t runId,
                                      std::uint32_t nowMs) noexcept {
    if ((state_.phase != Phase::Running && state_.phase != Phase::Fault) || !runId ||
        runId != state_.runId || !offDue(nowMs))
        return state_;
    if (state_.phase == Phase::Running) {
        if (!offRequested_) state_.outcome = Outcome::Expired;
        state_.phase = Phase::OffSubmitted;
    }
    state_.blackSubmitted = true;
    offRequested_ = false;
    return state_;
}

Snapshot LedControlV1::fault(std::uint32_t runId) noexcept {
    if (runId && runId == state_.runId) {
        state_.phase = Phase::Fault;
        state_.outcome = Outcome::DriverFault;
        offRequested_ = true;
    }
    return state_;
}

} // namespace ledcontrol
} // namespace larp
