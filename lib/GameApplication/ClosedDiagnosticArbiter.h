#pragma once

#include "LarpEscapeApp.h"
#include <cstdint>

namespace larp { namespace application {

inline bool softwareOffWithinDeadline(std::int64_t startedUs, std::int64_t completedUs) {
    return startedUs >= 0 && completedUs >= startedUs &&
           completedUs - startedUs <= 500000;
}

// These ports deliberately expose no physical-off acknowledgement or grant.
class ILedParkPort {
  public:
    virtual ~ILedParkPort() = default;
    virtual bool requestPark(std::uint32_t ticket) = 0;
    virtual bool offFrameSubmittedFor(std::uint32_t ticket) const = 0;
    virtual bool forceBlackFromDeadline(std::uint32_t ticket) = 0;
};

class ILedParkDeadline {
  public:
    virtual ~ILedParkDeadline() = default;
    // Independently observes the deadline; this cannot switch LED power off.
    virtual bool arm(std::uint32_t ticket, ILedParkPort &port) = 0;
    virtual bool firedFor(std::uint32_t ticket) const = 0;
    virtual bool missedFor(std::uint32_t ticket) const = 0;
    virtual bool independentBlackSubmittedFor(std::uint32_t ticket) const = 0;
};

enum class ClosedHandoff : std::uint8_t {
    Game, Fenced, Fault
};

class ClosedDiagnosticArbiter {
  public:
    ClosedDiagnosticArbiter(LarpEscapeApp &app, ILedParkPort &lights, ILedParkDeadline &timer)
        : app_(app), lights_(lights), timer_(timer) {}
    void update() { app_.update(); }
    // Sole application-loop owner only. A request after game start requires
    // two settled Standby observations. No transition out of Fenced/Fault.
    bool requestClosedHandoff(std::uint32_t ticket, bool preflight = false);
    ClosedHandoff state() const {
        return state_ == ClosedHandoff::Fenced && timer_.missedFor(ticket_)
                   ? ClosedHandoff::Fault : state_;
    }
    bool blackFrameSubmitted() const {
        return state_ != ClosedHandoff::Game && lights_.offFrameSubmittedFor(ticket_);
    }
    bool timerExpired() const {
        return state_ != ClosedHandoff::Game && timer_.firedFor(ticket_);
    }
    bool deadlineMissed() const {
        return state_ != ClosedHandoff::Game && timer_.missedFor(ticket_);
    }
    bool independentBlackSubmitted() const {
        return state_ != ClosedHandoff::Game && timer_.independentBlackSubmittedFor(ticket_);
    }
    // No reported observation may be promoted to a physical all-off result.
    bool diagnosticGrantAvailable() const { return false; }

  private:
    LarpEscapeApp &app_;
    ILedParkPort &lights_;
    ILedParkDeadline &timer_;
    ClosedHandoff state_ = ClosedHandoff::Game;
    std::uint32_t ticket_ = 0;
};
}} // namespace larp::application
