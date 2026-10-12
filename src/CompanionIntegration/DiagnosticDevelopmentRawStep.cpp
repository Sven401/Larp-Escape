#include "CompanionIntegration/DiagnosticDevelopmentRawStep.h"

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
bool valid(const DevelopmentRawStepRequest &request) noexcept {
    return request.message && request.step && request.timeoutMs >= 1000 &&
           request.timeoutMs <= 10000 && request.device.valid() && request.boot &&
           request.run.valid() && request.grant.valid();
}
bool valid(const DevelopmentRawStepReply &reply) noexcept {
    if (!valid(reply.request) || reply.startedDeviceMs > MaximumDeviceMs ||
        reply.endedDeviceMs > MaximumDeviceMs ||
        reply.observation.physicalChannelQualified ||
        reply.observation.address != 0x20 || reply.observation.reg != 0x13 ||
        reply.observation.bit != 6)
        return false;
    if (reply.outcome == DevelopmentRawStepOutcome::Rejected)
        return !reply.startedDeviceMs && !reply.endedDeviceMs &&
               reply.rejection != DevelopmentRawStepRejection::None &&
               static_cast<unsigned>(reply.rejection) <= 7 &&
               reply.observation.status == RawRegisterStatus::ReadFailed &&
               !reply.observation.rawByte && !reply.observation.rawHigh &&
               !reply.observation.busCode;
    if (reply.startedDeviceMs > reply.endedDeviceMs ||
        reply.endedDeviceMs - reply.startedDeviceMs >= reply.request.timeoutMs)
        return false;
    if (reply.rejection != DevelopmentRawStepRejection::None)
        return false;
    if (reply.outcome == DevelopmentRawStepOutcome::RegisterObserved)
        return reply.observation.readable() && !reply.observation.busCode &&
               reply.observation.rawHigh ==
                   ((reply.observation.rawByte & 0x40) != 0);
    return reply.outcome == DevelopmentRawStepOutcome::I2cFailed &&
           !reply.observation.readable() && !reply.observation.rawByte &&
           !reply.observation.rawHigh;
}
} // namespace

bool DevelopmentRawStepWire::encodeRequest(
    const DevelopmentRawStepRequest &request, std::uint8_t *bytes,
    std::size_t capacity) noexcept {
    if (!bytes || capacity < RequestBytes || !valid(request))
        return false;
    std::uint8_t frame[RequestBytes] = {};
    frame[0] = 'D'; frame[1] = 'G'; frame[2] = 1; frame[3] = 6;
    write(frame + 4, request.message, 4);
    std::memcpy(frame + 8, request.device.bytes.data(), 16);
    write(frame + 24, request.boot, 8);
    std::memcpy(frame + 32, request.run.bytes.data(), 16);
    std::memcpy(frame + 48, request.grant.bytes.data(), 16);
    write(frame + 64, request.step, 4);
    write(frame + 68, request.timeoutMs, 4);
    frame[72] = 1; // Raw register bit, deliberately not WakeCapture.
    frame[73] = 0x20; frame[74] = 0x13; frame[75] = 6;
    std::memcpy(bytes, frame, sizeof(frame));
    return true;
}

bool DevelopmentRawStepWire::decodeRequest(
    const std::uint8_t *bytes, std::size_t count,
    DevelopmentRawStepRequest &destination) noexcept {
    if (!bytes || count != RequestBytes || bytes[0] != 'D' || bytes[1] != 'G' ||
        bytes[2] != 1 || bytes[3] != 6 || bytes[72] != 1 ||
        bytes[73] != 0x20 || bytes[74] != 0x13 || bytes[75] != 6 ||
        bytes[76] || bytes[77] || bytes[78] || bytes[79])
        return false;
    DevelopmentRawStepRequest request;
    request.message = static_cast<std::uint32_t>(read(bytes + 4, 4));
    std::memcpy(request.device.bytes.data(), bytes + 8, 16);
    request.boot = read(bytes + 24, 8);
    std::memcpy(request.run.bytes.data(), bytes + 32, 16);
    std::memcpy(request.grant.bytes.data(), bytes + 48, 16);
    request.step = static_cast<std::uint32_t>(read(bytes + 64, 4));
    request.timeoutMs = static_cast<std::uint32_t>(read(bytes + 68, 4));
    if (!valid(request)) return false;
    destination = request;
    return true;
}

bool DevelopmentRawStepWire::encodeReply(
    const DevelopmentRawStepReply &reply, std::uint8_t *bytes,
    std::size_t capacity) noexcept {
    if (!bytes || capacity < ReplyBytes || !valid(reply))
        return false;
    std::uint8_t frame[ReplyBytes] = {};
    if (!encodeRequest(reply.request, frame, sizeof(frame)))
        return false;
    write(frame + 80, reply.startedDeviceMs, 8);
    write(frame + 88, reply.endedDeviceMs, 8);
    frame[96] = static_cast<std::uint8_t>(reply.outcome);
    frame[97] = static_cast<std::uint8_t>(reply.observation.status);
    frame[98] = reply.observation.rawByte;
    frame[99] = reply.observation.rawHigh ? 1 : 0;
    frame[100] = reply.observation.busCode;
    // Byte 101 remains zero: physical channel not qualified.
    frame[102] = static_cast<std::uint8_t>(reply.rejection);
    std::memcpy(bytes, frame, sizeof(frame));
    return true;
}

bool DevelopmentRawStepWire::decodeReply(
    const std::uint8_t *bytes, std::size_t count,
    DevelopmentRawStepReply &destination) noexcept {
    if (!bytes || count != ReplyBytes || bytes[96] > 2 || bytes[97] > 3 ||
        bytes[99] > 1 || bytes[101] || bytes[102] > 7 || bytes[103])
        return false;
    DevelopmentRawStepReply reply;
    if (!decodeRequest(bytes, RequestBytes, reply.request))
        return false;
    reply.startedDeviceMs = read(bytes + 80, 8);
    reply.endedDeviceMs = read(bytes + 88, 8);
    reply.outcome = static_cast<DevelopmentRawStepOutcome>(bytes[96]);
    reply.rejection = static_cast<DevelopmentRawStepRejection>(bytes[102]);
    reply.observation.address = 0x20;
    reply.observation.reg = 0x13;
    reply.observation.bit = 6;
    reply.observation.status = static_cast<RawRegisterStatus>(bytes[97]);
    reply.observation.rawByte = bytes[98];
    reply.observation.rawHigh = bytes[99] != 0;
    reply.observation.busCode = bytes[100];
    if (!valid(reply)) return false;
    destination = reply;
    return true;
}

} // namespace diagnostic
} // namespace larp
