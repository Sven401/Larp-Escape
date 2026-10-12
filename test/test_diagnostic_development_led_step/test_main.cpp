#include "CompanionIntegration/DiagnosticDevelopmentLedStep.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace larp::diagnostic;

DevelopmentLedStepRequest request() {
    DevelopmentLedStepRequest r;
    r.message = 7; r.step = 2; r.timeoutMs = 1000;
    r.device.bytes[0] = 0x5a; r.boot = 99;
    r.run.bytes[6] = 0x40; r.run.bytes[8] = 0x80; r.run.bytes[15] = 1;
    r.grant.bytes[6] = 0x40; r.grant.bytes[8] = 0x80; r.grant.bytes[15] = 2;
    r.index = 0; r.red = 16; r.green = 0; r.blue = 0;
    r.brightness = 16; r.durationMs = 250;
    return r;
}

int main() {
    unsigned cases = 0;
    std::array<std::uint8_t, DevelopmentLedStepWire::RequestBytes> bytes = {};
    assert(DevelopmentLedStepWire::encodeRequest(request(), bytes.data(), bytes.size()));
    assert(bytes[3] == 7 && bytes[72] == 2 && bytes[73] == 0 &&
           bytes[74] == 16 && bytes[77] == 16 && bytes[78] == 250);
    const char *golden =
        "44470107070000005A0000000000000000000000000000006300000000000000"
        "0000000000004000800000000000000100000000000040008000000000000002"
        "02000000E8030000020010000010FA00";
    char encoded[DevelopmentLedStepWire::RequestBytes * 2 + 1] = {};
    for (std::size_t index = 0; index < bytes.size(); ++index)
        std::snprintf(encoded + index * 2, 3, "%02X", bytes[index]);
    assert(std::strcmp(encoded, golden) == 0);
    DevelopmentLedStepRequest decoded;
    assert(DevelopmentLedStepWire::decodeRequest(bytes.data(), bytes.size(), decoded));
    assert(decoded.message == 7 && decoded.step == 2 && decoded.durationMs == 250);
    ++cases;

    auto malformed = bytes;
    malformed[73] = 23;
    assert(!DevelopmentLedStepWire::decodeRequest(malformed.data(), malformed.size(), decoded));
    malformed = bytes; malformed[74] = 33;
    assert(!DevelopmentLedStepWire::decodeRequest(malformed.data(), malformed.size(), decoded));
    malformed = bytes; malformed[78] = 251;
    assert(!DevelopmentLedStepWire::decodeRequest(malformed.data(), malformed.size(), decoded));
    assert(!DevelopmentLedStepWire::decodeRequest(bytes.data(), 79, decoded));
    ++cases;

    DevelopmentLedStepReply answer;
    answer.request = request(); answer.startedDeviceMs = 100;
    answer.endedDeviceMs = 520;
    answer.outcome = DevelopmentLedStepOutcome::BlackSubmitted;
    answer.renderCalls = 2; answer.blackSubmitted = true; answer.timerFired = true;
    std::array<std::uint8_t, DevelopmentLedStepWire::ReplyBytes> reply = {};
    assert(DevelopmentLedStepWire::encodeReply(answer, reply.data(), reply.size()));
    const char *replySuffix =
        "640000000000000008020000000000000000020101000000";
    char encodedReply[DevelopmentLedStepWire::ReplyBytes * 2 + 1] = {};
    for (std::size_t index = 0; index < reply.size(); ++index)
        std::snprintf(encodedReply + index * 2, 3, "%02X", reply[index]);
    assert(std::strcmp(encodedReply + DevelopmentLedStepWire::RequestBytes * 2,
                       replySuffix) == 0);
    DevelopmentLedStepReply received;
    assert(DevelopmentLedStepWire::decodeReply(reply.data(), reply.size(), received));
    assert(received.renderCalls == 2 && received.blackSubmitted &&
           received.timerFired && received.endedDeviceMs == 520);
    ++cases;

    reply[101] = 1; // Physical confirmation cannot be asserted on this wire.
    assert(!DevelopmentLedStepWire::decodeReply(reply.data(), reply.size(), received));
    reply[101] = 0; reply[100] = 0;
    assert(!DevelopmentLedStepWire::decodeReply(reply.data(), reply.size(), received));
    reply[100] = 1; reply[88] = 0x2c; reply[89] = 0x01; // 200 ms is too short.
    assert(!DevelopmentLedStepWire::decodeReply(reply.data(), reply.size(), received));
    ++cases;

    answer = DevelopmentLedStepReply{};
    answer.request = request();
    answer.outcome = DevelopmentLedStepOutcome::Rejected;
    answer.rejection = DevelopmentLedStepRejection::HardwareUnqualified;
    assert(DevelopmentLedStepWire::encodeReply(answer, reply.data(), reply.size()));
    assert(DevelopmentLedStepWire::decodeReply(reply.data(), reply.size(), received));
    assert(received.rejection == DevelopmentLedStepRejection::HardwareUnqualified);
    ++cases;

    std::printf("%u development LED-step wire cases passed\n", cases);
}
