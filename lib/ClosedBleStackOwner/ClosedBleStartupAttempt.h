#pragma once

#include <cstdint>

// One process-lifetime attempt. The first SDK error and its surrounding state
// remain available even if a caller invokes begin() again after failure.
class ClosedBleStartupAttempt {
  public:
    enum class Mode : std::uint8_t { Ble = 1 };
    enum class State : std::uint8_t {
        NotStarted, ControllerInitFailed, ControllerEnableFailed,
        BluedroidInitFailed, BluedroidEnableFailed, ReadyClosed
    };
    struct Report {
        State state = State::NotStarted;
        std::int32_t sdkError = 0;
        std::uint8_t controllerBefore = 0, controllerAfter = 0;
        std::uint8_t bluedroidBefore = 0, bluedroidAfter = 0;
        std::uint32_t internalFreeBefore = 0, internalLargestBefore = 0;
        std::uint32_t internalFreeAfter = 0, internalLargestAfter = 0;
    };

    template <typename Port> State begin(Port &port) noexcept {
        if (report_.state != State::NotStarted)
            return report_.state;
        report_.controllerBefore = port.controllerStatus();
        report_.bluedroidBefore = port.bluedroidStatus();
        report_.internalFreeBefore = port.internalFree();
        report_.internalLargestBefore = port.internalLargest();

        constexpr auto mode = Mode::Ble;
        std::int32_t result = port.controllerInit(mode);
        if (result != 0)
            return finish(port, State::ControllerInitFailed, result);
        result = port.controllerEnable(mode);
        if (result != 0)
            return finish(port, State::ControllerEnableFailed, result);
        result = port.bluedroidInit();
        if (result != 0)
            return finish(port, State::BluedroidInitFailed, result);
        result = port.bluedroidEnable();
        if (result != 0)
            return finish(port, State::BluedroidEnableFailed, result);
        return finish(port, State::ReadyClosed, 0);
    }
    const Report &report() const noexcept { return report_; }

  private:
    template <typename Port>
    State finish(Port &port, State state, std::int32_t error) noexcept {
        report_.controllerAfter = port.controllerStatus();
        report_.bluedroidAfter = port.bluedroidStatus();
        report_.internalFreeAfter = port.internalFree();
        report_.internalLargestAfter = port.internalLargest();
        report_.sdkError = error;
        report_.state = state;
        return state;
    }
    Report report_{};
};
