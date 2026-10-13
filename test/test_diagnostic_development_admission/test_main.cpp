#include "CompanionIntegration/DiagnosticDevelopmentAdmission.h"
#include "CompanionIntegration/DiagnosticDevelopmentControl.h"
#include <cstdio>
#include <cstdlib>

using namespace larp;
using namespace larp::diagnostic;

static unsigned checks = 0;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
    std::fprintf(stderr, "FAIL line %u: %s\n", __LINE__, #expression); std::exit(1); \
} } while (false)

static Uuid uuid(std::uint8_t last) {
    Uuid result;
    result.bytes[6] = 0x40;
    result.bytes[8] = 0x80;
    result.bytes[15] = last;
    return result;
}
static companion::Identity device() {
    companion::Identity result;
    result.bytes[0] = 1;
    return result;
}
static DevelopmentLink link() {
    DevelopmentLink result;
    result.original.device = device();
    result.original.connection.token = 41;
    result.original.connection.handle = 1;
    result.original.address = {{1, 2, 3, 4, 5, 6}};
    result.original.registration = 7;
    result.original.lifecycle = 2;
    result.original.interface = 1;
    result.encrypted = true;
    result.bonded = true;
    return result;
}
int main() {
    DevelopmentAdmission owner(device(), 123);
    auto current = link();
    auto run = uuid(1), grant = uuid(2);
    auto broken = current;
    broken.encrypted = false;
    CHECK(owner.begin(broken, run, grant, 1000, true, 10) ==
          DevelopmentAdmissionResult::Unavailable);
    broken = current;
    broken.bonded = false;
    CHECK(owner.begin(broken, run, grant, 1000, true, 10) ==
          DevelopmentAdmissionResult::Unavailable);
    CHECK(owner.begin(current, run, grant, 1000, false, 10) ==
          DevelopmentAdmissionResult::Invalid);
    CHECK(owner.begin(current, run, grant, 1000, true, 10) ==
          DevelopmentAdmissionResult::Accepted);
    CHECK(owner.active());
    CHECK(owner.begin(current, uuid(3), uuid(4), 1000, true, 11) ==
          DevelopmentAdmissionResult::Busy);
    CHECK(owner.step(current, run, grant, 1, 100, 20) ==
          DevelopmentAdmissionResult::Accepted);
    CHECK(owner.resultCurrent(current, run, grant, 1, 20));
    CHECK(!owner.resultCurrent(current, run, grant, 2, 20));
    CHECK(owner.step(current, run, grant, 1, 100, 21) ==
          DevelopmentAdmissionResult::Replay);
    DevelopmentAdmission staleLifecycle(device(), 123);
    CHECK(staleLifecycle.begin(current, uuid(11), uuid(12), 1000, true, 10) ==
          DevelopmentAdmissionResult::Accepted);
    broken = current;
    ++broken.original.lifecycle;
    CHECK(staleLifecycle.step(broken, uuid(11), uuid(12), 1, 100, 22) ==
          DevelopmentAdmissionResult::StaleLink);
    CHECK(!staleLifecycle.active());
    DevelopmentAdmission staleToken(device(), 123);
    CHECK(staleToken.begin(current, uuid(13), uuid(14), 1000, true, 10) ==
          DevelopmentAdmissionResult::Accepted);
    broken = current;
    ++broken.original.connection.token;
    CHECK(staleToken.step(broken, uuid(13), uuid(14), 1, 100, 22) ==
          DevelopmentAdmissionResult::StaleLink);
    CHECK(!staleToken.active());
    DevelopmentAdmission lostEncryption(device(), 123);
    CHECK(lostEncryption.begin(current, uuid(15), uuid(16), 1000, true, 10) ==
          DevelopmentAdmissionResult::Accepted);
    broken = current;
    broken.encrypted = false;
    CHECK(lostEncryption.step(broken, uuid(15), uuid(16), 1, 100, 22) ==
          DevelopmentAdmissionResult::StaleLink);
    CHECK(!lostEncryption.active());
    CHECK(owner.step(current, run, uuid(4), 2, 100, 22) ==
          DevelopmentAdmissionResult::Invalid);
    CHECK(owner.step(current, run, grant, 2, 1000, 23) ==
          DevelopmentAdmissionResult::Invalid);
    CHECK(owner.step(current, run, grant, 2, 100, 23) ==
          DevelopmentAdmissionResult::Accepted);
    CHECK(owner.cancel(broken, run, grant, 23) == DevelopmentAdmissionResult::StaleLink);
    CHECK(!owner.active());
    CHECK(owner.begin(current, uuid(21), uuid(22), 1000, true, 23) ==
          DevelopmentAdmissionResult::Accepted);
    run = uuid(21); grant = uuid(22);
    CHECK(owner.active());
    CHECK(owner.cancel(current, run, grant, 23) == DevelopmentAdmissionResult::Accepted);
    CHECK(!owner.active());
    CHECK(owner.begin(current, run, uuid(5), 1000, true, 24) ==
          DevelopmentAdmissionResult::Replay);
    CHECK(owner.begin(current, uuid(3), grant, 1000, true, 24) ==
          DevelopmentAdmissionResult::Replay);
    CHECK(owner.begin(current, uuid(3), uuid(4), 1000, true, 24) ==
          DevelopmentAdmissionResult::Accepted);
    broken = current;
    ++broken.original.connection.token;
    owner.disconnect(broken.original);
    CHECK(owner.active());
    owner.disconnect(current.original);
    CHECK(!owner.active());
    CHECK(owner.begin(current, uuid(5), uuid(6), 1000, true, 100) ==
          DevelopmentAdmissionResult::Accepted);
    CHECK(owner.step(current, uuid(5), uuid(6), 1, 100, 1100) ==
          DevelopmentAdmissionResult::StaleLink);
    CHECK(!owner.active());
    CHECK(owner.begin(current, uuid(7), uuid(8), 1000, true, 1099) ==
          DevelopmentAdmissionResult::Unavailable); // clock went backwards

    DevelopmentAdmission wireAdmission(device(), 123);
    DevelopmentControl control(wireAdmission, device(), 123);
    const auto makeFrame = [](ControlCommand command, std::uint32_t message,
                              const Uuid &runId, const Uuid &grantId,
                              std::uint32_t ttl, bool consent) {
        std::array<std::uint8_t, TransportLimits::RequestBytes> frame = {};
        frame[0] = 'D'; frame[1] = 'G'; frame[2] = 1;
        frame[3] = static_cast<std::uint8_t>(command);
        for (unsigned i = 0; i < 4; ++i) {
            frame[4 + i] = static_cast<std::uint8_t>(message >> (8 * i));
            frame[64 + i] = static_cast<std::uint8_t>(ttl >> (8 * i));
        }
        frame[8] = 1;
        frame[24] = 123;
        for (unsigned i = 0; i < 16; ++i) {
            frame[32 + i] = runId.bytes[i];
            frame[48 + i] = grantId.bytes[i];
        }
        frame[68] = consent ? 1 : 0;
        return frame;
    };
    std::array<std::uint8_t, TransportLimits::ReplyBytes> reply = {};
    const auto capability = makeFrame(ControlCommand::Capabilities, 1, {}, {}, 0, false);
    CHECK(control.process(capability.data(), capability.size(), current, {}, 10,
                          reply.data(), reply.size()));
    CHECK(reply[3] == 0x81 && reply[64] == 0 && reply[65] == 4 &&
          reply[66] == 1 && reply[67] == 0x03);
    const auto request = makeFrame(ControlCommand::Grant, 2, uuid(31), {}, 1000, true);
    broken = current; broken.bonded = false;
    CHECK(control.process(request.data(), request.size(), broken, uuid(32), 11,
                          reply.data(), reply.size()));
    CHECK(reply[64] == 1 && !wireAdmission.active());
    CHECK(control.process(request.data(), request.size(), current, uuid(32), 12,
                          reply.data(), reply.size()));
    CHECK(reply[3] == 0x82 && reply[64] == 0 && reply[65] == 4 &&
          reply[48 + 6] == 0x40 && reply[48 + 8] == 0x80 && reply[63] == 32);
    CHECK(wireAdmission.active());
    DevelopmentRawStepRequest raw;
    raw.device = device(); raw.boot = 123;
    raw.run = uuid(31); raw.grant = uuid(32);
    raw.message = 3; raw.step = 1; raw.timeoutMs = 100;
    CHECK(control.admitRawStep(raw, current, 12) == DevelopmentAdmissionResult::Accepted);
    CHECK(control.rawResultCurrent(raw, current, 12));
    CHECK(!control.rawResultCurrent(raw, broken, 12));
    CHECK(control.admitRawStep(raw, current, 12) == DevelopmentAdmissionResult::Replay);
    const auto cancel = makeFrame(ControlCommand::Cancel, 3, uuid(31), uuid(32), 0, false);
    CHECK(control.process(cancel.data(), cancel.size(), current, {}, 13,
                          reply.data(), reply.size()));
    CHECK(reply[3] == 0x83 && reply[64] == 0 && !wireAdmission.active());
    CHECK(!control.rawResultCurrent(raw, current, 13));
    CHECK(control.process(request.data(), request.size(), current, uuid(33), 14,
                          reply.data(), reply.size()));
    CHECK(reply[64] == 4 && !wireAdmission.active()); // consumed run cannot recur
    auto malformed = request;
    malformed[8] = 2;
    reply.fill(0x5a);
    CHECK(!control.process(malformed.data(), malformed.size(), current, uuid(34), 15,
                           reply.data(), reply.size()));
    CHECK(reply[0] == 0x5a); // invalid context cannot forge a correlated reply
    std::printf("development admission: %u checks passed\n", checks);
}
