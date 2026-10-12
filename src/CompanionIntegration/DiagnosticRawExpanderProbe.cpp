#include "CompanionIntegration/DiagnosticRawExpanderProbe.h"

namespace larp {
namespace diagnostic {

RawExpanderObservation RawExpanderProbe::sample() noexcept {
    RawExpanderObservation result;
    result.address = 0x20;
    result.reg = 0x13;
    result.bit = 6;
    const RawRegisterRead read = reader_.read(result.address, result.reg);
    result.status = read.status;
    result.busCode = read.busCode;
    if (result.readable()) {
        result.rawByte = read.value;
        result.rawHigh = (read.value & (1U << result.bit)) != 0;
    }
    return result;
}

} // namespace diagnostic
} // namespace larp
