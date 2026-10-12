#include "CompanionIntegration/DiagnosticDevelopmentControl.h"
#include <cstring>

#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
namespace larp {
namespace diagnostic {
namespace {
void write(std::uint8_t *target, std::uint64_t value, unsigned count) noexcept {
    for (unsigned index = 0; index < count; ++index)
        target[index] = static_cast<std::uint8_t>(value >> (8 * index));
}
std::uint8_t outcome(DevelopmentAdmissionResult result) noexcept {
    switch (result) {
    case DevelopmentAdmissionResult::Accepted: return 0;
    case DevelopmentAdmissionResult::Unavailable:
    case DevelopmentAdmissionResult::StaleLink: return 1;
    case DevelopmentAdmissionResult::Invalid: return 2;
    case DevelopmentAdmissionResult::Busy: return 3;
    case DevelopmentAdmissionResult::Replay: return 4;
    case DevelopmentAdmissionResult::Expired: return 5;
    case DevelopmentAdmissionResult::Capacity: return 7;
    }
    return 6;
}
}
bool DevelopmentControl::process(
    const std::uint8_t *bytes, std::size_t count, const DevelopmentLink &link,
    const Uuid &deviceGrant, std::uint64_t now, std::uint8_t *reply,
    std::size_t capacity) noexcept {
    ControlRequest request;
    if (!reply || capacity < TransportLimits::ReplyBytes ||
        !decodeControlRequest(bytes, count, request) ||
        !(request.device == device_) || request.boot != boot_ ||
        now > 9007199254740991ULL)
        return false;
    DevelopmentAdmissionResult result = DevelopmentAdmissionResult::Accepted;
    if (!link.original.valid() || !(link.original.device == device_) ||
        !link.encrypted || !link.bonded)
        result = DevelopmentAdmissionResult::Unavailable;
    else if (request.command == ControlCommand::Grant)
        result = admission_.begin(link, request.run, deviceGrant, request.ttlMs,
                                  request.consent, now);
    else if (request.command == ControlCommand::Cancel)
        result = admission_.cancel(link, request.run, request.grant, now);

    std::uint8_t value[TransportLimits::ReplyBytes] = {};
    value[0] = 'D'; value[1] = 'G'; value[2] = 1;
    value[3] = static_cast<std::uint8_t>(request.command) | 0x80;
    write(value + 4, request.message, 4);
    std::memcpy(value + 8, device_.bytes.data(), device_.bytes.size());
    write(value + 24, boot_, 8);
    std::memcpy(value + 32, request.run.bytes.data(), request.run.bytes.size());
    const Uuid grant = request.command == ControlCommand::Grant
                           ? (result == DevelopmentAdmissionResult::Accepted ? deviceGrant : Uuid{})
                           : request.grant;
    std::memcpy(value + 48, grant.bytes.data(), grant.bytes.size());
    value[64] = outcome(result);
    value[65] = 4; // Distinct development owner; never the synthetic/product owner.
    value[66] = 1; // Logical diagnostic schema.
    if (request.command == ControlCommand::Grant &&
        result == DevelopmentAdmissionResult::Accepted) {
        write(value + 68, now, 8);
        write(value + 76, now + request.ttlMs, 8);
    }
    write(value + 84, 60000, 4);
    std::memcpy(reply, value, sizeof(value));
    return true;
}
} // namespace diagnostic
} // namespace larp
#endif
