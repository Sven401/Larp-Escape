#include "CompanionIntegration/AppMutualDevTranscriptV1.h"
#include <unity.h>
#include <cstdio>

using namespace larp::app_mutual_dev;
void setUp() {}
void tearDown() {}
static unsigned checks = 0;
#define CHECK(x) do { ++checks; TEST_ASSERT_TRUE_MESSAGE((x), #x); } while (false)

template <std::size_t N> static void fill(std::array<std::uint8_t, N> &target, std::uint8_t seed) {
    for (std::size_t i = 0; i < N; ++i) target[i] = static_cast<std::uint8_t>(seed + i);
}
static Transcript fixture() {
    Transcript value;
    fill(value.chip, 1); fill(value.deviceId, 11);
    value.bootId = 0x0102030405060708ULL;
    value.sourceRegistration = 0x11223344;
    value.sdkGeneration = 0x55667788;
    value.gattInterface = 9;
    fill(value.peerAddress, 31);
    value.connectionToken = 0x91a2b3c4;
    value.connectionHandle = 0x1234;
    fill(value.phoneInstallId, 41);
    fill(value.boxKeyId, 61); fill(value.phoneKeyId, 81);
    fill(value.boxPublic, 101); value.boxPublic[0] = 4;
    fill(value.phonePublic, 151); value.phonePublic[0] = 4;
    fill(value.boxNonce, 201); fill(value.phoneNonce, 221);
    value.attempt = 0x01020304;
    value.boxEnrollmentGeneration = 3;
    value.phoneEnrollmentGeneration = 5;
    value.boxRevocationGeneration = 7;
    value.phoneRevocationGeneration = 9;
    return value;
}
static void test_codec_and_closed_fake() {
    CHECK(FrameSize == 289);
    Transcript original = fixture(), parsed;
    Frame frame = {}, roundtrip = {};
    CHECK(encode(original, frame));
    CHECK(frame[0] == 0x41 && frame[1] == 0x4d && frame[2] == 0x44 && frame[3] == 0x31);
    CHECK(frame[4] == 1 && frame[5] == 0xff && frame[6] == 0x42 && frame[7] == 0x50);
    CHECK(frame[8] == 1 && frame[14] == 11);
    CHECK(frame[30] == 0x01 && frame[37] == 0x08);
    CHECK(frame[38] == 0x11 && frame[41] == 0x44);
    CHECK(frame[42] == 0x55 && frame[45] == 0x88);
    CHECK(frame[46] == 9 && frame[47] == 31);
    CHECK(frame[57] == 0x12 && frame[58] == 0x34);
    CHECK(frame[75] == 61 && frame[91] == 4);
    CHECK(frame[156] == 81 && frame[172] == 4);
    CHECK(frame[269] == 0x01 && frame[272] == 0x04);
    CHECK(frame[285] == 0 && frame[288] == 9);
    CHECK(decode(frame.data(), frame.size(), parsed));
    CHECK(encode(parsed, roundtrip) && roundtrip == frame);
    CHECK(!decode(nullptr, frame.size(), parsed));
    CHECK(!decode(frame.data(), frame.size() - 1, parsed));
    CHECK(!decode(frame.data(), frame.size() + 1, parsed));
    for (std::size_t i = 0; i < FrameSize; ++i) {
        ClosedFake fake;
        CHECK(fake.begin(original));
        Frame changed = frame; changed[i] ^= 1;
        CHECK(!fake.check(changed.data(), changed.size()));
        CHECK(fake.state() == State::Closed);
        CHECK(!fake.check(frame.data(), frame.size()));
    }
    {
        ClosedFake fake;
        CHECK(fake.begin(original));
        CHECK(fake.check(frame.data(), frame.size()));
        CHECK(fake.state() == State::TranscriptCheckedClosed);
        CHECK(!fake.check(frame.data(), frame.size())); // replay on same attempt
        CHECK(!fake.begin(original));
    }
    {
        ClosedFake fake;
        Transcript nextAttempt = original; nextAttempt.attempt++;
        CHECK(fake.begin(nextAttempt));
        CHECK(!fake.check(frame.data(), frame.size())); // previous attempt
        CHECK(fake.state() == State::Closed);
    }
    {
        ClosedFake fake;
        Transcript nextBirth = original; nextBirth.connectionToken++;
        CHECK(fake.begin(nextBirth));
        CHECK(!fake.check(frame.data(), frame.size()));
    }
    {
        ClosedFake fake;
        Transcript changedRole = original;
        Frame malformed = frame; malformed[6] = 0x50;
        CHECK(fake.begin(changedRole));
        CHECK(!fake.check(malformed.data(), malformed.size()));
    }
    {
        ClosedFake fake;
        CHECK(fake.begin(original)); fake.close();
        CHECK(fake.state() == State::Closed);
        CHECK(!fake.check(frame.data(), frame.size()));
    }
    Transcript invalid = original;
    invalid.boxNonce = {};
    Frame untouched = frame;
    CHECK(!encode(invalid, untouched));
    CHECK(untouched == frame);
    invalid = original; invalid.boxPublic[0] = 0;
    CHECK(!encode(invalid, untouched));
    std::printf("%u APP_MUTUAL development transcript checks passed\n", checks);
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_codec_and_closed_fake);
    return UNITY_END();
}
