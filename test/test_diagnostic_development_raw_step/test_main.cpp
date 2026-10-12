#include "CompanionIntegration/DiagnosticDevelopmentRawStep.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace larp::diagnostic;

DevelopmentRawStepRequest request() {
    DevelopmentRawStepRequest result;
    result.message = 7;
    result.step = 1;
    result.timeoutMs = 1000;
    result.device.bytes[0] = 0x5a;
    result.boot = 99;
    result.run.bytes[6] = 0x40;
    result.run.bytes[8] = 0x80;
    result.run.bytes[15] = 1;
    result.grant.bytes[6] = 0x40;
    result.grant.bytes[8] = 0x80;
    result.grant.bytes[15] = 2;
    return result;
}

int main() {
    unsigned cases = 0;
    std::array<std::uint8_t, DevelopmentRawStepWire::RequestBytes> bytes = {};
    assert(DevelopmentRawStepWire::encodeRequest(request(), bytes.data(), bytes.size()));
    DevelopmentRawStepRequest decoded;
    assert(DevelopmentRawStepWire::decodeRequest(bytes.data(), bytes.size(), decoded));
    assert(decoded.message == 7 && decoded.step == 1 && decoded.timeoutMs == 1000);
    assert(decoded.device == request().device && decoded.run == request().run);
    assert(bytes[72] == 1 && bytes[73] == 0x20 && bytes[74] == 0x13 && bytes[75] == 6);
    const char *golden =
        "44470106070000005A0000000000000000000000000000006300000000000000"
        "0000000000004000800000000000000100000000000040008000000000000002"
        "01000000E80300000120130600000000";
    char encoded[DevelopmentRawStepWire::RequestBytes * 2 + 1] = {};
    for (std::size_t index = 0; index < bytes.size(); ++index)
        std::snprintf(encoded + index * 2, 3, "%02X", bytes[index]);
    assert(std::strcmp(encoded, golden) == 0);
    ++cases;

    auto malformed = bytes;
    malformed[73] = 0x21;
    assert(!DevelopmentRawStepWire::decodeRequest(malformed.data(), malformed.size(), decoded));
    malformed = bytes; malformed[76] = 1;
    assert(!DevelopmentRawStepWire::decodeRequest(malformed.data(), malformed.size(), decoded));
    malformed = bytes; malformed[64] = 0;
    assert(!DevelopmentRawStepWire::decodeRequest(malformed.data(), malformed.size(), decoded));
    assert(!DevelopmentRawStepWire::decodeRequest(bytes.data(), bytes.size() - 1, decoded));
    ++cases;

    DevelopmentRawStepReply observed;
    observed.request = request();
    observed.startedDeviceMs = 100;
    observed.endedDeviceMs = 105;
    observed.outcome = DevelopmentRawStepOutcome::RegisterObserved;
    observed.observation = {0x20, 0x13, 6, RawRegisterStatus::Valid,
                            0x40, 0, true, false};
    std::array<std::uint8_t, DevelopmentRawStepWire::ReplyBytes> reply = {};
    assert(DevelopmentRawStepWire::encodeReply(observed, reply.data(), reply.size()));
    const char *replyGolden =
        "44470106070000005A0000000000000000000000000000006300000000000000"
        "0000000000004000800000000000000100000000000040008000000000000002"
        "01000000E80300000120130600000000"
        "640000000000000069000000000000000000400100000000";
    char encodedReply[DevelopmentRawStepWire::ReplyBytes * 2 + 1] = {};
    for (std::size_t index = 0; index < reply.size(); ++index)
        std::snprintf(encodedReply + index * 2, 3, "%02X", reply[index]);
    assert(std::strcmp(encodedReply, replyGolden) == 0);
    DevelopmentRawStepReply received;
    assert(DevelopmentRawStepWire::decodeReply(reply.data(), reply.size(), received));
    assert(received.outcome == DevelopmentRawStepOutcome::RegisterObserved);
    assert(received.observation.readable() && received.observation.rawHigh);
    assert(!received.observation.physicalChannelQualified);
    ++cases;

    reply[99] = 0;
    assert(!DevelopmentRawStepWire::decodeReply(reply.data(), reply.size(), received));
    reply[99] = 1; reply[101] = 1;
    assert(!DevelopmentRawStepWire::decodeReply(reply.data(), reply.size(), received));
    reply[101] = 0; reply[88] = 0x4c; reply[89] = 0x04; // 1100 = timeout boundary.
    assert(!DevelopmentRawStepWire::decodeReply(reply.data(), reply.size(), received));
    ++cases;

    observed.outcome = DevelopmentRawStepOutcome::I2cFailed;
    observed.observation = {0x20, 0x13, 6,
                            RawRegisterStatus::RegisterSelectFailed, 0, 2, false, false};
    assert(DevelopmentRawStepWire::encodeReply(observed, reply.data(), reply.size()));
    assert(DevelopmentRawStepWire::decodeReply(reply.data(), reply.size(), received));
    assert(received.observation.busCode == 2 && !received.observation.readable());
    ++cases;

    observed.outcome = DevelopmentRawStepOutcome::Rejected;
    observed.rejection = DevelopmentRawStepRejection::Permission;
    observed.startedDeviceMs = observed.endedDeviceMs = 0;
    observed.observation = {0x20, 0x13, 6, RawRegisterStatus::ReadFailed,
                            0, 0, false, false};
    assert(DevelopmentRawStepWire::encodeReply(observed, reply.data(), reply.size()));
    assert(DevelopmentRawStepWire::decodeReply(reply.data(), reply.size(), received));
    assert(received.rejection == DevelopmentRawStepRejection::Permission);
    ++cases;

    std::printf("%u development raw-step wire cases passed\n", cases);
}
