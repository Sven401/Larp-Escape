#pragma once

#include "CompanionIntegration/DiagnosticControl.h"
#include <cstddef>
#include <cstdint>

namespace larp {
namespace diagnostic {

// Development-image-only single LED step. This shape is not the productive
// diagnostic StepTransport and does not assert a qualified physical LED.
struct DevelopmentLedStepRequest {
    std::uint32_t message = 0, step = 0, timeoutMs = 1000;
    companion::Identity device;
    std::uint64_t boot = 0;
    Uuid run, grant;
    std::uint8_t index = 0, red = 16, green = 0, blue = 0, brightness = 16;
    std::uint16_t durationMs = 250;
};

enum class DevelopmentLedStepOutcome : std::uint8_t {
    BlackSubmitted = 0, SoftwareFault = 1, Rejected = 2,
};
enum class DevelopmentLedStepRejection : std::uint8_t {
    None = 0, HardwareUnqualified = 1, Permission = 2, Expired = 3,
    Replay = 4, Busy = 5, Invalid = 6, Capacity = 7, LinkChanged = 8,
};
struct DevelopmentLedStepReply {
    DevelopmentLedStepRequest request;
    std::uint64_t startedDeviceMs = 0, endedDeviceMs = 0;
    DevelopmentLedStepOutcome outcome = DevelopmentLedStepOutcome::Rejected;
    DevelopmentLedStepRejection rejection = DevelopmentLedStepRejection::None;
    std::uint8_t renderCalls = 0;
    bool blackSubmitted = false, timerFired = false;
};

struct DevelopmentLedStepWire {
    static const std::size_t RequestBytes = 80;
    static const std::size_t ReplyBytes = 104;
    static bool encodeRequest(const DevelopmentLedStepRequest &, std::uint8_t *,
                              std::size_t) noexcept;
    static bool decodeRequest(const std::uint8_t *, std::size_t,
                              DevelopmentLedStepRequest &) noexcept;
    static bool encodeReply(const DevelopmentLedStepReply &, std::uint8_t *,
                            std::size_t) noexcept;
    static bool decodeReply(const std::uint8_t *, std::size_t,
                            DevelopmentLedStepReply &) noexcept;
};

} // namespace diagnostic
} // namespace larp
