#pragma once

#include <atomic>
#include <cstdint>

// One-way handoff for the sole light worker. Submission of a black frame is
// deliberately weaker than a confirmed physical off state.
class LightParkFence {
  public:
    bool request(std::uint32_t ticket) {
        if (ticket == 0U) {
            return false;
        }
        State expected = State::Running;
        if (!state.compare_exchange_strong(expected, State::Claiming,
                                           std::memory_order_acq_rel)) {
            return false;
        }
        ticket_.store(ticket, std::memory_order_relaxed);
        state.store(State::Requested, std::memory_order_release);
        return true;
    }

    bool blocksGameCommands() const {
        return state.load(std::memory_order_acquire) != State::Running;
    }

    bool workerShouldSubmitOff() const {
        return state.load(std::memory_order_acquire) == State::Requested;
    }

    void workerSubmittedOffFrame() {
        State expected = State::Requested;
        state.compare_exchange_strong(expected, State::OffFrameSubmitted,
                                      std::memory_order_acq_rel);
    }

    bool offFrameSubmittedFor(std::uint32_t ticket) const {
        return ticket != 0U && state.load(std::memory_order_acquire) == State::OffFrameSubmitted &&
               ticket_.load(std::memory_order_relaxed) == ticket;
    }
    bool requestedFor(std::uint32_t ticket) const {
        const State current = state.load(std::memory_order_acquire);
        return ticket != 0U &&
               (current == State::Requested || current == State::OffFrameSubmitted) &&
               ticket_.load(std::memory_order_acquire) == ticket;
    }
    void workerFault() { state.store(State::Fault, std::memory_order_release); }

  private:
    enum class State : std::uint8_t { Running, Claiming, Requested, OffFrameSubmitted, Fault };
    std::atomic<State> state{State::Running};
    std::atomic<std::uint32_t> ticket_{0U};
};
