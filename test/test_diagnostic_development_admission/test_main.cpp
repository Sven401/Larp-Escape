#include "CompanionIntegration/DiagnosticDevelopmentAdmission.h"
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
    result.original.connection.handle = 3;
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
    owner.cancel(broken, run, grant);
    CHECK(owner.active());
    owner.cancel(current, run, grant);
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
    std::printf("development admission: %u checks passed\n", checks);
}
