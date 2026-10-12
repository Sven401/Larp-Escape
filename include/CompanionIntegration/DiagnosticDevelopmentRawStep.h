#pragma once

#include "CompanionIntegration/DiagnosticControl.h"
#include "CompanionIntegration/DiagnosticRawExpanderProbe.h"

#include <cstddef>
#include <cstdint>

namespace larp {
namespace diagnostic {

// Development-image-only fixed-width BLE contract. The request selects one
// exploratory register read, not a physically qualified Wake channel.
struct DevelopmentRawStepRequest {
    std::uint32_t message = 0, step = 0, timeoutMs = 0;
    companion::Identity device;
    std::uint64_t boot = 0;
    Uuid run, grant;
};

enum class DevelopmentRawStepOutcome : std::uint8_t {
    RegisterObserved = 0,
    I2cFailed = 1,
    Rejected = 2,
};

enum class DevelopmentRawStepRejection : std::uint8_t {
    None = 0, Permission = 1, Expired = 2, Replay = 3,
    Busy = 4, Invalid = 5, Capacity = 6, LinkChanged = 7,
};

struct DevelopmentRawStepReply {
    DevelopmentRawStepRequest request;
    std::uint64_t startedDeviceMs = 0, endedDeviceMs = 0;
    DevelopmentRawStepOutcome outcome = DevelopmentRawStepOutcome::Rejected;
    DevelopmentRawStepRejection rejection = DevelopmentRawStepRejection::None;
    RawExpanderObservation observation;
};

struct DevelopmentRawStepWire {
    static const std::size_t RequestBytes = 80;
    static const std::size_t ReplyBytes = 104;
    static bool encodeRequest(const DevelopmentRawStepRequest &, std::uint8_t *,
                              std::size_t) noexcept;
    static bool decodeRequest(const std::uint8_t *, std::size_t,
                              DevelopmentRawStepRequest &) noexcept;
    static bool encodeReply(const DevelopmentRawStepReply &, std::uint8_t *,
                            std::size_t) noexcept;
    static bool decodeReply(const std::uint8_t *, std::size_t,
                            DevelopmentRawStepReply &) noexcept;
};

} // namespace diagnostic
} // namespace larp
