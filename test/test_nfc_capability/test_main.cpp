#include "CompanionIntegration/NfcCapabilityV1.h"
#include <unity.h>
#include <array>
#include <cstdio>

using namespace larp::nfc_capability;
void setUp() {}
void tearDown() {}
static unsigned checks = 0;
#define CHECK(x) do { ++checks; TEST_ASSERT_TRUE_MESSAGE((x), #x); } while (false)

static Nonce nonce(std::uint8_t first) {
    Nonce out = {};
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = static_cast<std::uint8_t>(first + i);
    return out;
}
static std::array<std::uint8_t, 21> response(const Nonce &value) {
    std::array<std::uint8_t, 21> out = {{0x4e, 0x43, 0x01}};
    for (std::size_t i = 0; i < value.size(); ++i) out[i + 3] = value[i];
    out[19] = 0x90; out[20] = 0;
    return out;
}
static ReaderContext owner() {
    ReaderContext out;
    out.ownerTicket = 17; out.readerSession = 8; out.bleGeneration = 5;
    out.exclusiveReader = true; out.gameParked = true;
    return out;
}
static Probe awaitingEcho() {
    Probe probe;
    ReaderContext ctx = owner();
    const auto select = response(nonce(1));
    CHECK(probe.begin(ctx, nonce(41), 100, 30000));
    CHECK(probe.runId() == Nonce{});
    CHECK(probe.onSelectResponse(ctx, 101, select.data(), select.size()));
    CHECK(probe.runId() == nonce(1));
    CHECK(probe.result() == Result::AwaitEcho);
    return probe;
}

static void test_nfc_capability() {
    const SelectFrame expectedSelect = {{0x00, 0xa4, 0x04, 0x00, 0x08, 0xf0, 0x4c, 0x41,
                                          0x52, 0x50, 0x4e, 0x46, 0x43, 0x00}};
    CHECK(selectFrame() == expectedSelect);
    ChallengeFrame expectedChallenge = {{0x80, 0x10, 0, 0, 0x10}};
    for (std::size_t i = 0; i < 16; ++i) expectedChallenge[i + 5] = static_cast<std::uint8_t>(41 + i);
    CHECK(challengeFrame(nonce(41)) == expectedChallenge);
    auto runReply = response(nonce(1));
    auto echoReply = response(nonce(41));
    Nonce parsedRun = nonce(90);
    CHECK(validSelectResponse(runReply.data(), runReply.size(), parsedRun));
    CHECK(parsedRun == nonce(1));
    CHECK(validEchoResponse(echoReply.data(), echoReply.size(), nonce(41)));
    CHECK(!validSelectResponse(nullptr, 21, parsedRun));
    CHECK(!validSelectResponse(runReply.data(), 20, parsedRun));
    CHECK(!validSelectResponse(runReply.data(), 22, parsedRun));
    CHECK(parsedRun == nonce(1));
    auto zeroRunReply = response(Nonce{});
    CHECK(!validSelectResponse(zeroRunReply.data(), zeroRunReply.size(), parsedRun));
    CHECK(parsedRun == nonce(1));
    for (std::size_t index : {std::size_t(0), std::size_t(1), std::size_t(2), std::size_t(19), std::size_t(20)}) {
        auto bad = runReply; bad[index] ^= 1;
        CHECK(!validSelectResponse(bad.data(), bad.size(), parsedRun));
        CHECK(parsedRun == nonce(1));
    }
    {
        Probe probe = awaitingEcho();
        CHECK(probe.onEchoResponse(owner(), 102, echoReply.data(), echoReply.size()));
        CHECK(probe.probeEchoOk());
        CHECK(!probe.onEchoResponse(owner(), 103, echoReply.data(), echoReply.size()));
        CHECK(!probe.begin(owner(), nonce(41), 104, 30000));
    }
    {
        Probe probe;
        ReaderContext busy = owner(); busy.exclusiveReader = false;
        CHECK(!probe.begin(busy, nonce(41), 100, 30000));
        CHECK(probe.result() == Result::ReaderBusy);
        CHECK(!probe.begin(owner(), nonce(41), 101, 30000));
    }
    {
        Probe probe;
        ReaderContext busy = owner(); busy.gameParked = false;
        CHECK(!probe.begin(busy, nonce(41), 100, 30000));
        CHECK(probe.result() == Result::ReaderBusy);
    }
    for (int variant = 0; variant < 6; ++variant) {
        Probe probe;
        ReaderContext ctx = owner();
        Nonce challenge = nonce(41);
        std::uint32_t duration = 30000;
        if (variant == 0) ctx.ownerTicket = 0;
        if (variant == 1) ctx.readerSession = 0;
        if (variant == 2) ctx.bleGeneration = 0;
        if (variant == 3) challenge = {};
        if (variant == 4) duration = 0;
        if (variant == 5) duration = 30001;
        CHECK(!probe.begin(ctx, challenge, 100, duration));
        CHECK(probe.result() == Result::InvalidStart);
    }
    {
        Probe probe;
        CHECK(probe.begin(owner(), nonce(41), 100, 30000));
        CHECK(!probe.onEchoResponse(owner(), 101, echoReply.data(), echoReply.size()));
        CHECK(probe.result() == Result::MalformedEcho);
    }
    {
        Probe probe;
        CHECK(probe.begin(owner(), nonce(41), 100, 30000));
        CHECK(!probe.onSelectResponse(owner(), 101, zeroRunReply.data(), zeroRunReply.size()));
        CHECK(probe.result() == Result::MalformedSelect);
        CHECK(probe.runId() == Nonce{});
    }
    {
        Probe probe;
        CHECK(probe.begin(owner(), nonce(41), 100, 30000));
        auto badVersion = runReply; badVersion[2] = 2;
        CHECK(!probe.onSelectResponse(owner(), 101, badVersion.data(), badVersion.size()));
        CHECK(probe.result() == Result::MalformedSelect);
        CHECK(probe.runId() == Nonce{});
    }
    {
        Probe probe;
        CHECK(probe.begin(owner(), nonce(41), 100, 30000));
        CHECK(!probe.onSelectResponse(owner(), 101, echoReply.data(), echoReply.size()));
        CHECK(probe.result() == Result::MalformedSelect);
    }
    {
        Probe probe = awaitingEcho();
        CHECK(!probe.onSelectResponse(owner(), 102, runReply.data(), runReply.size()));
        CHECK(probe.result() == Result::MalformedSelect);
    }
    {
        Probe probe = awaitingEcho();
        CHECK(!probe.onEchoResponse(owner(), 102, runReply.data(), runReply.size()));
        CHECK(probe.result() == Result::MalformedEcho);
    }
    {
        Probe probe = awaitingEcho();
        auto bad = echoReply; bad[19] = 0x6a; bad[20] = 0x82;
        CHECK(!probe.onEchoResponse(owner(), 102, bad.data(), bad.size()));
        CHECK(probe.result() == Result::MalformedEcho);
    }
    {
        Probe probe = awaitingEcho();
        ReaderContext changed = owner(); changed.ownerTicket++;
        CHECK(!probe.onEchoResponse(changed, 102, echoReply.data(), echoReply.size()));
        CHECK(probe.result() == Result::OwnerChanged);
    }
    {
        Probe probe = awaitingEcho();
        ReaderContext changed = owner(); changed.readerSession++;
        probe.tick(changed, 102);
        CHECK(probe.result() == Result::SessionChanged);
    }
    {
        Probe probe = awaitingEcho();
        ReaderContext changed = owner(); changed.bleGeneration++;
        CHECK(!probe.onEchoResponse(changed, 102, echoReply.data(), echoReply.size()));
        CHECK(probe.result() == Result::BleGenerationChanged);
    }
    {
        Probe probe = awaitingEcho();
        probe.tick(owner(), 30100);
        CHECK(probe.result() == Result::Timeout);
        CHECK(!probe.onEchoResponse(owner(), 30101, echoReply.data(), echoReply.size()));
    }
    {
        Probe probe = awaitingEcho();
        probe.deactivate();
        CHECK(probe.result() == Result::Deactivated);
        CHECK(!probe.onEchoResponse(owner(), 102, echoReply.data(), echoReply.size()));
    }
    std::printf("%u NFC capability checks passed\n", checks);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_nfc_capability);
    return UNITY_END();
}
