#pragma once

#include <cstdint>

// A discovery-only GAP sequence. Each SDK post has one terminal callback;
// failure is permanent for this boot. No state represents peer authority.
class ClosedBleAdvertiseGate {
  public:
    enum class Phase : std::uint8_t {
        Closed, DataPending, StartPending, Visible, Connected, Failed,
        Quiescing, RadioUnconfirmed, FencedClosed
    };

    Phase phase() const noexcept { return phase_; }
    bool configure(bool serviceReady, bool registryInstalledClosed,
                   bool authorityAbsent) noexcept {
        if (phase_ != Phase::Closed)
            return false;
        if (!serviceReady || !registryInstalledClosed || !authorityAbsent) {
            phase_ = Phase::Failed;
            return false;
        }
        phase_ = Phase::DataPending;
        return true;
    }
    bool dataComplete(bool success) noexcept {
        if (phase_ != Phase::DataPending || !success) {
            phase_ = Phase::Failed;
            return false;
        }
        phase_ = Phase::StartPending;
        return true;
    }
    bool startComplete(bool success) noexcept {
        if (phase_ != Phase::StartPending || !success) {
            phase_ = Phase::Failed;
            return false;
        }
        phase_ = Phase::Visible;
        return true;
    }
    bool connected() noexcept {
        if (phase_ != Phase::Visible)
            return false;
        phase_ = Phase::Connected;
        return true;
    }
    bool disconnected(bool serviceReady, bool registryInstalledClosed,
                      bool authorityAbsent) noexcept {
        if (phase_ != Phase::Connected || !serviceReady ||
            !registryInstalledClosed || !authorityAbsent) {
            phase_ = Phase::Failed;
            return false;
        }
        phase_ = Phase::StartPending;
        return true;
    }
    bool missedOriginalBirth(bool serviceReady, bool registryInstalledClosed,
                             bool authorityAbsent) noexcept {
        if (phase_ != Phase::Visible) {
            phase_ = Phase::Failed;
            return false;
        }
        phase_ = Phase::Connected;
        return disconnected(serviceReady, registryInstalledClosed, authorityAbsent);
    }
    void fail() noexcept { phase_ = Phase::Failed; }

  private:
    Phase phase_ = Phase::Closed;
};

// Stop posted before a pending start ACK cannot prove the late start stayed
// stopped. A positive late start demands a second, acknowledged stop.
class ClosedBleRadioFence {
  public:
    explicit ClosedBleRadioFence(bool startTerminal, bool startSucceeded) noexcept
        : startTerminal_(startTerminal), startSucceeded_(startSucceeded),
          closed_(startTerminal && !startSucceeded) {}
    void startComplete(bool success) noexcept {
        startTerminal_ = true;
        startSucceeded_ = success;
        if (!success)
            closed_ = true;
    }
    bool needsStopAfterStart() const noexcept {
        return startTerminal_ && startSucceeded_ && !earlyStopPending_ &&
               !stopPendingAfterStart_ && !closed_;
    }
    void earlyStopPosted(bool accepted) noexcept { earlyStopPending_ = accepted; }
    void stopPosted(bool accepted) noexcept { stopPendingAfterStart_ = accepted; }
    void stopComplete(bool success) noexcept {
        if (earlyStopPending_) {
            earlyStopPending_ = false;
            return;
        }
        if (stopPendingAfterStart_) {
            closed_ = success;
            if (!success)
                stopPendingAfterStart_ = false;
        }
    }
    bool closed() const noexcept { return closed_; }

  private:
    bool startTerminal_ = false, startSucceeded_ = false;
    bool earlyStopPending_ = false, stopPendingAfterStart_ = false, closed_ = false;
};
