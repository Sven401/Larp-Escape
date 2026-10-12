#include "CompanionIntegration/DiagnosticDevelopmentLedStep.h"
#include <cstring>

namespace larp {
namespace diagnostic {
namespace {
const std::uint64_t MaximumDeviceMs = 9007199254740991ULL;
std::uint64_t read(const std::uint8_t *bytes, unsigned count) noexcept {
    std::uint64_t value = 0;
    for (unsigned index = 0; index < count; ++index)
        value |= static_cast<std::uint64_t>(bytes[index]) << (8 * index);
    return value;
}
void write(std::uint8_t *bytes, std::uint64_t value, unsigned count) noexcept {
    for (unsigned index = 0; index < count; ++index)
        bytes[index] = static_cast<std::uint8_t>(value >> (8 * index));
}
bool valid(const DevelopmentLedStepRequest &r) noexcept {
    return r.message && r.step && r.timeoutMs >= 1000 && r.timeoutMs <= 10000 &&
           r.device.valid() && r.boot && r.run.valid() && r.grant.valid() &&
           r.index < 23 && r.red <= 32 && r.green <= 32 && r.blue <= 32 &&
           (r.red || r.green || r.blue) && r.brightness >= 1 &&
           r.brightness <= 32 && r.durationMs >= 100 && r.durationMs <= 250;
}
bool valid(const DevelopmentLedStepReply &r) noexcept {
    if (!valid(r.request) || r.startedDeviceMs > MaximumDeviceMs ||
        r.endedDeviceMs > MaximumDeviceMs || r.renderCalls > 2)
        return false;
    if (r.outcome == DevelopmentLedStepOutcome::Rejected)
        return !r.startedDeviceMs && !r.endedDeviceMs && !r.renderCalls &&
               !r.blackSubmitted && !r.timerFired &&
               r.rejection != DevelopmentLedStepRejection::None &&
               static_cast<unsigned>(r.rejection) <= 8;
    if (r.rejection != DevelopmentLedStepRejection::None ||
        r.startedDeviceMs > r.endedDeviceMs ||
        r.endedDeviceMs - r.startedDeviceMs >= r.request.timeoutMs)
        return false;
    if (r.outcome == DevelopmentLedStepOutcome::BlackSubmitted)
        return r.renderCalls == 2 && r.blackSubmitted && r.timerFired &&
               r.endedDeviceMs - r.startedDeviceMs >= r.request.durationMs;
    return r.outcome == DevelopmentLedStepOutcome::SoftwareFault &&
           (r.renderCalls < 2 || !r.blackSubmitted || !r.timerFired);
}
} // namespace

bool DevelopmentLedStepWire::encodeRequest(const DevelopmentLedStepRequest &r,
                                            std::uint8_t *bytes,
                                            std::size_t capacity) noexcept {
    if (!bytes || capacity < RequestBytes || !valid(r)) return false;
    std::uint8_t frame[RequestBytes] = {};
    frame[0] = 'D'; frame[1] = 'G'; frame[2] = 1; frame[3] = 7;
    write(frame + 4, r.message, 4);
    std::memcpy(frame + 8, r.device.bytes.data(), 16);
    write(frame + 24, r.boot, 8);
    std::memcpy(frame + 32, r.run.bytes.data(), 16);
    std::memcpy(frame + 48, r.grant.bytes.data(), 16);
    write(frame + 64, r.step, 4);
    write(frame + 68, r.timeoutMs, 4);
    frame[72] = 2; frame[73] = r.index; frame[74] = r.red;
    frame[75] = r.green; frame[76] = r.blue; frame[77] = r.brightness;
    write(frame + 78, r.durationMs, 2);
    std::memcpy(bytes, frame, sizeof(frame));
    return true;
}

bool DevelopmentLedStepWire::decodeRequest(const std::uint8_t *bytes,
                                            std::size_t count,
                                            DevelopmentLedStepRequest &destination) noexcept {
    if (!bytes || count != RequestBytes || bytes[0] != 'D' || bytes[1] != 'G' ||
        bytes[2] != 1 || bytes[3] != 7 || bytes[72] != 2)
        return false;
    DevelopmentLedStepRequest r;
    r.message = static_cast<std::uint32_t>(read(bytes + 4, 4));
    std::memcpy(r.device.bytes.data(), bytes + 8, 16);
    r.boot = read(bytes + 24, 8);
    std::memcpy(r.run.bytes.data(), bytes + 32, 16);
    std::memcpy(r.grant.bytes.data(), bytes + 48, 16);
    r.step = static_cast<std::uint32_t>(read(bytes + 64, 4));
    r.timeoutMs = static_cast<std::uint32_t>(read(bytes + 68, 4));
    r.index = bytes[73]; r.red = bytes[74]; r.green = bytes[75];
    r.blue = bytes[76]; r.brightness = bytes[77];
    r.durationMs = static_cast<std::uint16_t>(read(bytes + 78, 2));
    if (!valid(r)) return false;
    destination = r;
    return true;
}

bool DevelopmentLedStepWire::encodeReply(const DevelopmentLedStepReply &r,
                                          std::uint8_t *bytes,
                                          std::size_t capacity) noexcept {
    if (!bytes || capacity < ReplyBytes || !valid(r)) return false;
    std::uint8_t frame[ReplyBytes] = {};
    if (!encodeRequest(r.request, frame, sizeof(frame))) return false;
    write(frame + 80, r.startedDeviceMs, 8);
    write(frame + 88, r.endedDeviceMs, 8);
    frame[96] = static_cast<std::uint8_t>(r.outcome);
    frame[97] = static_cast<std::uint8_t>(r.rejection);
    frame[98] = r.renderCalls;
    frame[99] = r.blackSubmitted ? 1 : 0;
    frame[100] = r.timerFired ? 1 : 0;
    // 101 remains zero: no physical visibility or all-off confirmation.
    std::memcpy(bytes, frame, sizeof(frame));
    return true;
}

bool DevelopmentLedStepWire::decodeReply(const std::uint8_t *bytes,
                                          std::size_t count,
                                          DevelopmentLedStepReply &destination) noexcept {
    if (!bytes || count != ReplyBytes || bytes[96] > 2 || bytes[97] > 8 ||
        bytes[98] > 2 || bytes[99] > 1 || bytes[100] > 1 ||
        bytes[101] || bytes[102] || bytes[103]) return false;
    DevelopmentLedStepReply r;
    if (!decodeRequest(bytes, RequestBytes, r.request)) return false;
    r.startedDeviceMs = read(bytes + 80, 8);
    r.endedDeviceMs = read(bytes + 88, 8);
    r.outcome = static_cast<DevelopmentLedStepOutcome>(bytes[96]);
    r.rejection = static_cast<DevelopmentLedStepRejection>(bytes[97]);
    r.renderCalls = bytes[98]; r.blackSubmitted = bytes[99] != 0;
    r.timerFired = bytes[100] != 0;
    if (!valid(r)) return false;
    destination = r;
    return true;
}

} // namespace diagnostic
} // namespace larp
