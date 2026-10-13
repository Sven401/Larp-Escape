#include "../../lib/FreeRtosProfileFrameQueue/FreeRtosProfileFrameQueue.h"
#include "../../lib/FreeRtosProfileResponseQueue/FreeRtosProfileResponseQueue.h"
#include "../../src/ProfileTool/TemporaryProfileFile.h"
#include "Companion/CommandReceipt.h"
#include "Companion/CommandStream.h"
#include "Companion/IdentityAccess.h"
#include "Companion/PeerProofFrame.h"
#include "Companion/PeerProofReceiver.h"
#include "Companion/PeerRegistryRuntime.h"
#include "Companion/ProfileCapabilities.h"
#include "Companion/ProfileFrames.h"
#include "Companion/ProfileResponseSender.h"
#include "Companion/ProfileStaging.h"
#include "Companion/ReplyFrameSender.h"
#include "Companion/StatusFrameSender.h"
#include "CompanionIntegration/ApplicationProfileStaging.h"
#include "CompanionIntegration/GameControl.h"
#include "CompanionIntegration/ProfileApply.h"
#include "CompanionIntegration/ProfileImport.h"
#include "CompanionIntegration/ProfileTransferControl.h"
#include "FakeOwners.h"
#include "FreeRtosCommandFrameQueue.h"
#include "FreeRtosGameControlChannel.h"
#include "MemoryJournalStore.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unity.h>
#include <vector>
#define LARP_GATT_SOURCE_HOST_MODEL 1
#include "GattsServiceSdkModel.h"
#include "SdkGattSourceModel.h"
#include "SdkPeerSecurityModel.h"
// Compile the actual adapter against the deterministic RTOS model.
#include "../../lib/FreeRtosCommandFrameQueue/FreeRtosCommandFrameQueue.cpp"
#include "../../lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.cpp"
#include "../../lib/FreeRtosPeerAuthRuntime/FreeRtosPeerAuthRuntime.cpp"
#include "../../lib/FreeRtosPeerChallengeIssueRuntime/FreeRtosPeerChallengeIssueRuntime.cpp"
#include "../../lib/FreeRtosPeerChallengeReadRuntime/FreeRtosPeerChallengeReadRuntime.cpp"
#include "../../lib/FreeRtosPeerProofGrantRuntime/FreeRtosPeerProofGrantRuntime.cpp"
#include "../../lib/FreeRtosPeerProofQueue/FreeRtosPeerProofQueue.cpp"
#include "../../lib/FreeRtosPeerRegistryRuntime/FreeRtosPeerRegistryRuntime.cpp"
#include "../../lib/FreeRtosProfileFrameQueue/FreeRtosProfileFrameQueue.cpp"
#include "../../lib/FreeRtosProfileResponseQueue/FreeRtosProfileResponseQueue.cpp"
#include "../../lib/GattsWriteAdmission/Esp32GattsPeerAuth.cpp"
#include "../../lib/GattsWriteAdmission/Esp32GattsService.cpp"
#include "../../lib/GattsWriteAdmission/Esp32GattsSource.cpp"
#include "../../lib/GattsWriteAdmission/FreeRtosGattsConnection.cpp"
#include "../../lib/GattsWriteAdmission/FreeRtosGattsResponseQueue.cpp"
#include "../../lib/GattsWriteAdmission/GattsWriteAdmission.cpp"
#include "../../lib/GattsWriteAdmission/SdkGattsService.cpp"
#if defined(LARP_REAL_PEER_CRYPTO)
#include "../../lib/Esp32PeerProofCrypto/Esp32PeerProofCrypto.cpp"
#endif
#include "../../src/ProfileTool/TemporaryProfileFile.cpp"
// Actual ESP file adapters under an isolated deterministic VFS model.
#include "ProfileFileVfs.h"
// Actual identity persistence/entropy adapters under deterministic SDK models.
#include "../../lib/Esp32PeerRegistryStore/Esp32PeerRegistryStore.cpp"
#include "DeviceIdentityNvsModel.h"
#include "PeerRegistryNvsModel.h"
namespace host_rtos {
void (*beforeEnter)() = nullptr;
unsigned depth = 0;
bool nonzeroWait = false;
} // namespace host_rtos
namespace {
using namespace larp::companion;
using host_journal::MemoryStore;
TrustedAuthority authority(std::uint32_t epoch, bool gm = true, std::uint32_t principal = 7) {
    TrustedAuthority value;
    value.epoch = epoch;
    value.principal = principal;
    value.gameMaster = gm;
    return value;
}
// Test-only observation hooks model updates immediately before/after the read.
// They never access private controllers and do not claim an SMP execution.
class ObservedChannel : public IGameControlChannel {
  public:
    FreeRtosGameControlChannel actual;
    mutable unsigned reads = 0;
    mutable unsigned revokeBefore = 0, revokeAfter = 0;
    bool readAuthority(TrustedAuthority &value) const noexcept override {
        ++reads;
        auto &source = const_cast<FreeRtosGameControlChannel &>(actual);
        if (reads == revokeBefore)
            source.updateAuthority(authority(2, false));
        const bool available = source.readAuthority(value);
        if (reads == revokeAfter)
            source.updateAuthority(authority(2, false));
        return available;
    }
    bool receive(TrustedGameRequest &value) noexcept override {
        return actual.receive(value);
    }
    bool send(const RoutedGameReply &value) noexcept override {
        return actual.send(value);
    }
    bool publish(const RoutedConfiguredStatus &value) noexcept override {
        return actual.publish(value);
    }
};
struct Fixture {
    Owners owners;
    MemoryStore store;
    ProfileApplication application{owners, owners, store};
    ObservedChannel channel;
    larp::integration::GameControl control{application, channel};
    BootIdentity boot;
    std::uint32_t now = 1;
    void ready() {
        boot.high = 0x12345678;
        boot.low = 0xabcdef01;
        TEST_ASSERT_TRUE(channel.actual.begin());
        TEST_ASSERT_TRUE(application.begin());
        for (unsigned step = 0; step < 256 && application.busy(); ++step) {
            owners.pump(now);
            application.service(now);
            now += 5;
        }
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ApplicationPhase::Ready),
                          static_cast<unsigned>(application.snapshot().phase));
        TEST_ASSERT_TRUE(application.snapshot().storedMatchesActive);
        TEST_ASSERT_TRUE(application.activation().hardwareReady);
        TEST_ASSERT_TRUE(control.begin(boot));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                          static_cast<unsigned>(channel.actual.updateAuthority(authority(1))));
        control.service(now);
        TEST_ASSERT_EQUAL_UINT32(1, control.snapshot().lease);
    }
    GameCommand command(GameCommandKind kind = GameCommandKind::Start) const {
        GameCommand value;
        value.boot = boot;
        value.lease = control.snapshot().lease;
        value.request = control.snapshot().nextRequest;
        value.expectedConfigurationRevision = application.activation().configurationRevision;
        value.expectedStateRevision = application.execution().stateRevision;
        value.kind = kind;
        return value;
    }
    GameSubmission submit(const GameCommand &value, std::uint32_t epoch = 1,
                          std::uint32_t principal = 7) {
        char message[maxGameCommandJsonBytes];
        std::size_t written = 0;
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(ConfiguredEncodeResult::Success),
            static_cast<unsigned>(encodeGameCommand(value, message, sizeof(message), written)));
        return channel.actual.submitJson(message, written, principal, epoch);
    }
    RoutedGameReply execute(const GameCommand &value) {
        TEST_ASSERT_TRUE(submit(value));
        control.service(++now);
        RoutedGameReply reply;
        TEST_ASSERT_TRUE(channel.actual.takeReply(reply));
        return reply;
    }
};
std::unique_ptr<Fixture> f;
void equalCommand(const GameCommand &a, const GameCommand &b) {
    TEST_ASSERT_TRUE(a.boot == b.boot);
    TEST_ASSERT_EQUAL_UINT32(a.lease, b.lease);
    TEST_ASSERT_EQUAL_UINT32(a.request, b.request);
    TEST_ASSERT_EQUAL_UINT32(a.expectedConfigurationRevision, b.expectedConfigurationRevision);
    TEST_ASSERT_EQUAL_UINT32(a.expectedStateRevision, b.expectedStateRevision);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(a.kind), static_cast<unsigned>(b.kind));
    TEST_ASSERT_EQUAL_UINT16(a.target, b.target);
}
std::string json(const GameCommand &value) {
    char bytes[maxGameCommandJsonBytes];
    std::size_t written;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeGameCommand(value, bytes, sizeof(bytes), written)));
    return std::string(bytes, written);
}
std::string replace(std::string text, const std::string &from, const std::string &to) {
    const auto position = text.find(from);
    TEST_ASSERT_TRUE(position != std::string::npos);
    text.replace(position, from.size(), to);
    return text;
}
void test_codec_roundtrip_boundaries_and_capacity() {
    auto input = f->command(GameCommandKind::TakeTransition);
    input.boot.high = input.boot.low = input.lease = input.request =
        input.expectedConfigurationRevision = input.expectedStateRevision = UINT32_MAX;
    input.target = UINT16_MAX;
    const auto text = json(input);
    GameCommand output;
    TEST_ASSERT_TRUE(decodeGameCommand(text.data(), text.size(), output));
    equalCommand(input, output);
    char buffer[maxGameCommandJsonBytes + 1] = {};
    std::size_t written;
    buffer[text.size() + 1] = 'Z';
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeGameCommand(input, buffer, text.size() + 1, written)));
    TEST_ASSERT_EQUAL_UINT32(text.size(), written);
    TEST_ASSERT_EQUAL_CHAR('Z', buffer[text.size() + 1]);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::BufferTooSmall),
        static_cast<unsigned>(encodeGameCommand(input, buffer, text.size(), written)));
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL_CHAR(0, buffer[0]);
}
void test_codec_reordered_escaped_keys_and_values() {
    const char bytes[] =
        R"({"target":0,"kind":"st\u0061rt","expectedStateRevision":0,"expectedConfigurationRevision":1,"request":1,"lease":1,"boot":"12345678ABCDEF01","schema\u0056ersion":2})";
    GameCommand output;
    TEST_ASSERT_TRUE(decodeGameCommand(bytes, sizeof(bytes) - 1, output));
    TEST_ASSERT_TRUE(output.boot == f->boot);
    TEST_ASSERT_EQUAL_UINT32(1, output.lease);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameCommandKind::Start),
                      static_cast<unsigned>(output.kind));
}
void test_codec_rejections_preserve_destination() {
    const auto good = json(f->command());
    const std::string invalid[] = {
        replace(good, "\"lease\":1", "\"lease\":0"),
        replace(good, "\"lease\":1", "\"lease\":-0"),
        replace(good, "\"lease\":1", "\"lease\":01"),
        replace(good, "\"lease\":1", "\"lease\":1.0"),
        replace(good, "\"lease\":1", "\"lease\":1e0"),
        replace(good, "\"lease\":1", "\"lease\":4294967296"),
        replace(good, "\"lease\":1", "\"lease\":+1"),
        replace(good, "\"lease\":1", "\"lease\":true"),
        replace(good, "\"lease\":1", "\"lease\":{}"),
        replace(good, "\"lease\":1", "\"lease\":[]"),
        replace(good, "\"schemaVersion\":2", "\"schemaVersion\":3"),
        replace(good, "\"request\":1", "\"request\":0"),
        replace(good, "\"kind\":\"start\"", "\"kind\":\"reset\""),
        replace(good, "\"target\":0", "\"target\":1"),
        replace(good, "\"target\":0", "\"target\":65536"),
        replace(good, "\"boot\":\"12345678ABCDEF01\"", "\"boot\":\"0000000000000000\""),
        replace(good, "\"boot\":\"12345678ABCDEF01\"", "\"boot\":\"12345678abcdef01\""),
        replace(good, "\"target\":0", "\"principal\":7"),
        replace(good, "\"target\":0", "\"role\":\"gm\""),
        replace(good, "\"target\":0", "\"lease\":1"),
        replace(good, "\"target\":0", "\"l\\u0065ase\":1"),
        replace(good, ",\"target\":0", ""),
        good.substr(0, good.size() - 1) + ",}",
        good + "x",
        good + std::string(1, '\0'),
        std::string("\xef\xbb\xbf") + good,
        good.substr(0, good.size() - 1),
        replace(good, "\"kind\":\"start\"", "\"kind\":\"st\\u0000art\"")};
    const auto preserved = f->command(GameCommandKind::Pause);
    for (const auto &bytes : invalid) {
        GameCommand destination = preserved;
        TEST_ASSERT_FALSE_MESSAGE(bool(decodeGameCommand(bytes.data(), bytes.size(), destination)),
                                  bytes.c_str());
        equalCommand(preserved, destination);
        const auto result = f->channel.actual.submitJson(bytes.data(), bytes.size(), 7, 1);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::DecodeRejected),
                          static_cast<unsigned>(result.outcome));
    }
    GameCommand output = preserved;
    std::string oversized(maxGameCommandDocumentBytes + 1, ' ');
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(CommandDecodeError::MessageTooLarge),
        static_cast<unsigned>(decodeGameCommand(oversized.data(), oversized.size(), output).error));
    equalCommand(preserved, output);
    TEST_ASSERT_FALSE(decodeGameCommand(nullptr, 1, output));
    equalCommand(preserved, output);
    TrustedGameRequest request;
    TEST_ASSERT_FALSE(f->channel.actual.receive(request));
}
void test_uninitialized_and_missing_metadata_skip_decode() {
    FreeRtosGameControlChannel channel;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::Uninitialized),
                      static_cast<unsigned>(channel.submitJson(nullptr, 0, 7, 1).outcome));
    TEST_ASSERT_TRUE(channel.begin());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::MissingPrincipal),
                      static_cast<unsigned>(channel.submitJson(nullptr, 0, 0, 1).outcome));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::MissingEpoch),
                      static_cast<unsigned>(channel.submitJson(nullptr, 0, 7, 0).outcome));
    const auto bytes = json(f->command());
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(GameSubmissionOutcome::Unauthorized),
        static_cast<unsigned>(channel.submitJson(bytes.data(), bytes.size(), 7, 1).outcome));
}
void test_authority_fault_fences_duplicate_and_new_epoch_recovers() {
    AuthorityState state;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.update(authority(10))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Duplicate),
                      static_cast<unsigned>(state.update(authority(10))));
    TEST_ASSERT_TRUE(state.admits(7, 10));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Conflict),
                      static_cast<unsigned>(state.update(authority(10, false))));
    TEST_ASSERT_FALSE(state.admits(7, 10));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.update(authority(10))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.update(authority(11))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Stale),
                      static_cast<unsigned>(state.update(authority(9))));
    TEST_ASSERT_FALSE(state.admits(7, 11));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.update(authority(11))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Invalid),
                      static_cast<unsigned>(state.update(authority(20, true, 0))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.update(authority(20))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.update(authority(21))));
    state.suspend();
    TrustedAuthority output = authority(99);
    TEST_ASSERT_FALSE(state.read(output));
    TEST_ASSERT_EQUAL_UINT32(99, output.epoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.update(authority(21))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.update(authority(22, false, 0))));
    TEST_ASSERT_FALSE(state.admits(7, 22));
}
void test_authority_maximum_never_wraps_or_reopens() {
    AuthorityState state;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(state.update(authority(UINT32_MAX))));
    state.suspend();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(state.update(authority(UINT32_MAX))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Invalid),
                      static_cast<unsigned>(state.update(authority(0))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Stale),
                      static_cast<unsigned>(state.update(authority(1))));
    TEST_ASSERT_FALSE(state.admits(7, UINT32_MAX));
}
void test_complete_json_starts_stored_game_and_publishes_copied_state() {
    const auto reply = f->execute(f->command());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
    TEST_ASSERT_TRUE(reply.reply.consumed);
    TEST_ASSERT_FALSE(reply.reply.duplicate);
    TEST_ASSERT_EQUAL_UINT32(7, reply.principal);
    TEST_ASSERT_EQUAL_UINT32(1, reply.connectionEpoch);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    ConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readStatus(status));
    TEST_ASSERT_TRUE(status.running);
    TEST_ASSERT_TRUE(status.storedMatchesActive);
    TEST_ASSERT_EQUAL_UINT32(f->application.execution().stateRevision, status.stateRevision);
    TEST_ASSERT_EQUAL_UINT32(2, status.nextRequest);
    char wire[maxConfiguredStatusJsonBytes];
    std::size_t written;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeConfiguredStatus(status, wire, sizeof(wire), written)));
    TEST_ASSERT_NOT_NULL(std::strstr(wire, "\"running\":true"));
}
void test_idempotent_authority_and_command_retry_keep_original_receipt() {
    const auto command = f->command();
    const auto first = f->execute(command);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Duplicate),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(1))));
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().lease);
    const auto retry = f->execute(command);
    TEST_ASSERT_TRUE(retry.reply.duplicate);
    TEST_ASSERT_EQUAL_UINT32(first.reply.stateRevision, retry.reply.stateRevision);
    TEST_ASSERT_EQUAL_UINT32(first.reply.stateRevision, f->application.execution().stateRevision);
    auto conflict = command;
    conflict.kind = GameCommandKind::Stop;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::RequestConflict),
                      static_cast<unsigned>(f->execute(conflict).reply.outcome));
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_old_queue_and_recycled_principal_rejected_after_reconnect() {
    const auto old = f->command();
    TEST_ASSERT_TRUE(f->submit(old));
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(AuthorityUpdate::Published),
        static_cast<unsigned>(f->channel.actual.updateAuthority(authority(2, false, 0))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(3))));
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Unauthorized),
                      static_cast<unsigned>(reply.reply.outcome));
    TEST_ASSERT_FALSE(reply.reply.consumed);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(1, reply.connectionEpoch);
    TEST_ASSERT_EQUAL_UINT32(3, f->control.snapshot().authorityEpoch);
    TEST_ASSERT_EQUAL_UINT32(2, f->control.snapshot().lease);
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::Unauthorized),
                      static_cast<unsigned>(f->submit(old).outcome));
    auto current = f->command();
    TEST_ASSERT_TRUE(f->submit(current, 3));
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
    TEST_ASSERT_EQUAL_UINT32(3, reply.connectionEpoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.reply.outcome));
}
void test_full_queue_cannot_lose_revocation() {
    const auto old = f->command();
    for (unsigned i = 0; i < ControlLimits::Queue; ++i)
        TEST_ASSERT_TRUE(f->submit(old));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::QueueFull),
                      static_cast<unsigned>(f->submit(old).outcome));
    f->channel.actual.updateAuthority(authority(2, false));
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(0, f->control.snapshot().lease);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    RoutedGameReply reply;
    unsigned count = 0;
    while (f->channel.actual.takeReply(reply)) {
        ++count;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Unauthorized),
                          static_cast<unsigned>(reply.reply.outcome));
        TEST_ASSERT_FALSE(reply.reply.consumed);
    }
    TEST_ASSERT_EQUAL_UINT32(ControlLimits::Queue, count);
}
void revokeAtAdmission() {
    f->channel.actual.updateAuthority(authority(2, false));
}
void test_revocation_between_decode_and_admission_rejects() {
    host_rtos::beforeEnter = revokeAtAdmission;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::Unauthorized),
                      static_cast<unsigned>(f->submit(f->command()).outcome));
    TrustedGameRequest value;
    TEST_ASSERT_FALSE(f->channel.actual.receive(value));
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(0, f->control.snapshot().lease);
}
void test_revocation_before_final_observation_prevents_operation() {
    TEST_ASSERT_TRUE(f->submit(f->command()));
    f->channel.revokeBefore = f->channel.reads + 2;
    f->control.service(++f->now);
    RoutedGameReply reply;
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Unauthorized),
                      static_cast<unsigned>(reply.reply.outcome));
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_revocation_after_claim_allows_one_operation_then_closes() {
    const auto value = f->command();
    TEST_ASSERT_TRUE(f->submit(value));
    TEST_ASSERT_TRUE(f->submit(value));
    f->channel.revokeAfter = f->channel.reads + 2;
    f->control.service(++f->now);
    RoutedGameReply first, second;
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(first));
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(second));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(first.reply.outcome));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Unauthorized),
                      static_cast<unsigned>(second.reply.outcome));
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(0, f->control.snapshot().lease);
}
void test_suspend_preserves_game_and_needs_new_epoch() {
    f->execute(f->command());
    f->channel.actual.suspendAuthority();
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(0, f->control.snapshot().lease);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Suspended),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(1))));
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(0, f->control.snapshot().lease);
    f->channel.actual.updateAuthority(authority(2));
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(2, f->control.snapshot().lease);
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().nextRequest);
}
void test_stale_revision_consumed_wrong_boot_and_hole_not_consumed() {
    auto value = f->command();
    ++value.boot.low;
    auto reply = f->execute(value).reply;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::WrongBoot),
                      static_cast<unsigned>(reply.outcome));
    TEST_ASSERT_FALSE(reply.consumed);
    value = f->command();
    ++value.request;
    reply = f->execute(value).reply;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::OutOfOrder),
                      static_cast<unsigned>(reply.outcome));
    TEST_ASSERT_FALSE(reply.consumed);
    value = f->command();
    ++value.expectedConfigurationRevision;
    reply = f->execute(value).reply;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::StaleConfiguration),
                      static_cast<unsigned>(reply.outcome));
    TEST_ASSERT_TRUE(reply.consumed);
    value = f->command();
    ++value.expectedStateRevision;
    reply = f->execute(value).reply;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::StaleState),
                      static_cast<unsigned>(reply.outcome));
    TEST_ASSERT_TRUE(reply.consumed);
    TEST_ASSERT_EQUAL_UINT32(3, f->control.snapshot().nextRequest);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_evicted_receipt_never_reexecutes_old_request() {
    auto first = f->command();
    ++first.expectedStateRevision;
    f->execute(first);
    for (unsigned i = 0; i < ControlLimits::Receipts; ++i) {
        auto value = f->command();
        ++value.expectedStateRevision;
        f->execute(value);
    }
    const auto result = f->execute(first).reply;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::ReceiptExpired),
                      static_cast<unsigned>(result.outcome));
    TEST_ASSERT_FALSE(result.consumed);
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_dropped_reply_retry_executes_once() {
    for (unsigned i = 0; i < ControlLimits::Queue; ++i) {
        auto value = f->command();
        ++value.expectedStateRevision;
        TEST_ASSERT_TRUE(f->submit(value));
        f->control.service(++f->now);
    }
    auto start = f->command();
    TEST_ASSERT_TRUE(f->submit(start));
    f->control.service(++f->now);
    TEST_ASSERT_EQUAL_UINT32(1, f->control.snapshot().droppedReplies);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    const auto revision = f->application.execution().stateRevision;
    RoutedGameReply ignored;
    while (f->channel.actual.takeReply(ignored)) {
    }
    const auto retry = f->execute(start).reply;
    TEST_ASSERT_TRUE(retry.duplicate);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(retry.outcome));
    TEST_ASSERT_EQUAL_UINT32(revision, f->application.execution().stateRevision);
}
void test_copied_message_survives_source_reuse() {
    auto bytes = json(f->command());
    TEST_ASSERT_TRUE(f->channel.actual.submitJson(bytes.data(), bytes.size(), 7, 1));
    bytes.assign(bytes.size(), 'x');
    f->control.service(++f->now);
    RoutedGameReply result;
    TEST_ASSERT_TRUE(f->channel.actual.takeReply(result));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(result.reply.outcome));
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_wrong_principal_and_epoch_cannot_admit() {
    const auto value = f->command();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::Unauthorized),
                      static_cast<unsigned>(f->submit(value, 1, 8).outcome));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameSubmissionOutcome::Unauthorized),
                      static_cast<unsigned>(f->submit(value, 2, 7).outcome));
    TrustedGameRequest typed;
    typed.command = value;
    typed.principal = 8;
    typed.connectionEpoch = 1;
    TEST_ASSERT_FALSE(f->channel.actual.submit(typed));
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_pending_configuration_blocks_start_and_consumes_request() {
    Fixture pending;
    pending.boot = f->boot;
    TEST_ASSERT_TRUE(pending.channel.actual.begin());
    TEST_ASSERT_TRUE(pending.application.begin());
    TEST_ASSERT_TRUE(pending.control.begin(pending.boot));
    pending.channel.actual.updateAuthority(authority(1));
    pending.control.service(1);
    auto value = pending.command();
    const auto result = pending.execute(value).reply;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::NotReady),
                      static_cast<unsigned>(result.outcome));
    TEST_ASSERT_TRUE(result.consumed);
    TEST_ASSERT_FALSE(pending.application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(2, pending.control.snapshot().nextRequest);
}

void fixture(const char *name, const char *bytes, std::size_t count) {
    const std::string path = std::string("docs/companion/fixtures/v2/") + name + ".json";
    const char *exportFlag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (exportFlag && std::strcmp(exportFlag, "1") == 0) {
        auto *output = std::fopen(path.c_str(), "wb");
        TEST_ASSERT_NOT_NULL(output);
        TEST_ASSERT_EQUAL_UINT32(count, std::fwrite(bytes, 1, count, output));
        TEST_ASSERT_EQUAL_INT('\n', std::fputc('\n', output));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(output));
    }
    auto *input = std::fopen(path.c_str(), "rb");
    TEST_ASSERT_NOT_NULL(input);
    char expected[maxConfiguredStatusJsonBytes + 1];
    const auto read = std::fread(expected, 1, sizeof(expected), input);
    TEST_ASSERT_EQUAL_INT(0, std::fclose(input));
    // The minified JSON content remains byte-exact; permit only a final LF or
    // CRLF so a Windows Git checkout does not change the contract comparison.
    TEST_ASSERT_TRUE(read == count + 1 || read == count + 2);
    if (read == count + 2)
        TEST_ASSERT_EQUAL_CHAR('\r', expected[count]);
    TEST_ASSERT_EQUAL_CHAR('\n', expected[read - 1]);
    TEST_ASSERT_EQUAL_MEMORY(bytes, expected, count);
}
void statusFixture(const char *name) {
    RoutedConfiguredStatus routed;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(routed));
    const auto &status = routed.status;
    char bytes[maxConfiguredStatusJsonBytes];
    std::size_t count;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeConfiguredStatus(status, bytes, sizeof(bytes), count)));
    fixture(name, bytes, count);
}
void replyFixture(const char *name, const GameReply &reply) {
    char bytes[maxGameReplyJsonBytes];
    std::size_t count;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                      static_cast<unsigned>(encodeGameReply(reply, bytes, sizeof(bytes), count)));
    fixture(name, bytes, count);
}
void test_app_fixtures_match_actual_firmware_vertical_flow() {
    statusFixture("status-ready");
    const auto start = f->command();
    const auto request = json(start);
    fixture("command-start", request.data(), request.size());
    const auto executed = f->execute(start).reply;
    replyFixture("reply-executed", executed);
    statusFixture("status-running");
    const auto duplicate = f->execute(start).reply;
    TEST_ASSERT_TRUE(duplicate.duplicate);
    replyFixture("reply-duplicate", duplicate);
    auto conflict = start;
    conflict.kind = GameCommandKind::Stop;
    replyFixture("reply-conflict", f->execute(conflict).reply);
    auto stale = f->command(GameCommandKind::Pause);
    ++stale.expectedStateRevision;
    const auto staleRequest = json(stale);
    fixture("command-stale-state", staleRequest.data(), staleRequest.size());
    replyFixture("reply-stale-state", f->execute(stale).reply);
}

class ProfileBytes : public IProfileWriter, public IProfileReader {
  public:
    std::string bytes;
    std::size_t position = 0;
    bool write(const char *value, std::size_t count) override {
        bytes.append(value, count);
        return true;
    }
    ReadStatus readByte(std::uint8_t &value) override {
        if (position == bytes.size())
            return ReadStatus::End;
        value = static_cast<std::uint8_t>(bytes[position++]);
        return ReadStatus::Byte;
    }
};
void test_ready_inventory_exists_before_first_game_start() {
    std::unique_ptr<Profile> profile(new Profile);
    TEST_ASSERT_TRUE(makeStandardProfile(*profile));
    const auto value = f->application.game();
    TEST_ASSERT_EQUAL_UINT32(profile->components.size(), value.deviceCount);
    for (std::size_t i = 0; i < profile->components.size(); ++i) {
        TEST_ASSERT_EQUAL_UINT16(profile->components.get(i)->id.value, value.devices[i].id.value);
        TEST_ASSERT_EQUAL(profile->components.get(i)->enabled, value.devices[i].enabled);
        TEST_ASSERT_TRUE(value.devices[i].available);
    }
    TEST_ASSERT_FALSE(value.execution.running);
    TEST_ASSERT_EQUAL_UINT32(0, value.execution.stateRevision);
}
void test_profile_change_hides_old_role_and_rebinds_inventory_without_resetting_revision() {
    std::unique_ptr<Profile> profile(new Profile);
    TEST_ASSERT_TRUE(makeStandardProfile(*profile));
    f->owners.card = profile->roles.get(0)->uid;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(f->execute(f->command()).reply.outcome));
    StateId readingState;
    for (std::size_t i = 0; i < profile->states.size() && !readingState.value; ++i) {
        const auto &state = *profile->states.get(i);
        for (std::size_t j = 0; j < state.tick.size(); ++j)
            for (std::size_t k = 0; k < profile->actions.size(); ++k)
                if (profile->actions.get(k)->id == *state.tick.get(j) &&
                    profile->actions.get(k)->kind == ActionKind::ReadRole)
                    readingState = state.id;
    }
    TEST_ASSERT_TRUE(readingState.value != 0);
    auto change = f->command(GameCommandKind::ChangeState);
    change.target = readingState.value;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(f->execute(change).reply.outcome));
    for (unsigned i = 0; i < 400 && !f->application.game().rolePresent; ++i) {
        f->now += 5;
        f->owners.pump(f->now);
        f->application.service(f->now);
    }
    TEST_ASSERT_TRUE(f->application.game().rolePresent);
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(GameReplyOutcome::Executed),
        static_cast<unsigned>(f->execute(f->command(GameCommandKind::Stop)).reply.outcome));
    TEST_ASSERT_TRUE(f->application.game().rolePresent); // Retained within same configuration.
    const auto stoppedRevision = f->application.execution().stateRevision;
    const auto oldConfiguration = f->application.activation().configurationRevision;
    for (std::size_t i = 0; i < profile->components.size(); ++i) {
        auto &component = *profile->components.get(i);
        component.id.value += 1000;
        for (std::size_t j = 0; j < component.rows.size(); ++j)
            component.rows.get(j)->expander.value += 1000;
        for (std::size_t j = 0; j < component.columns.size(); ++j)
            component.columns.get(j)->expander.value += 1000;
        for (std::size_t j = 0; j < component.wakeInputs.size(); ++j)
            component.wakeInputs.get(j)->expander.value += 1000;
    }
    for (std::size_t i = 0; i < profile->conditions.size(); ++i)
        if (profile->conditions.get(i)->component.value)
            profile->conditions.get(i)->component.value += 1000;
    for (std::size_t i = 0; i < profile->effects.size(); ++i)
        profile->effects.get(i)->component.value += 1000;
    for (std::size_t i = 0; i < profile->actions.size(); ++i) {
        auto &action = *profile->actions.get(i);
        if (action.component.value)
            action.component.value += 1000;
        if (action.source.value)
            action.source.value += 1000;
    }
    for (std::size_t i = 0; i < profile->roles.size(); ++i)
        profile->roles.get(i)->id.value += 1000;
    TEST_ASSERT_TRUE(validateProfile(*profile));
    ProfileBytes bytes;
    TEST_ASSERT_TRUE(writeProfile(*profile, bytes));
    TEST_ASSERT_TRUE(f->application.importDraft(bytes, f->application.storage().draftRevision));
    TEST_ASSERT_TRUE(f->application.activateDraft(f->application.storage().draftRevision,
                                                  oldConfiguration, f->now));
    for (unsigned i = 0; i < 256 && f->application.busy(); ++i) {
        f->now += 5;
        f->owners.pump(f->now);
        f->application.service(f->now);
    }
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ApplicationPhase::Ready),
                      static_cast<unsigned>(f->application.snapshot().phase));
    TEST_ASSERT_TRUE(f->application.activation().configurationRevision > oldConfiguration);
    const auto fresh = f->application.game();
    TEST_ASSERT_FALSE(fresh.rolePresent);
    TEST_ASSERT_EQUAL_UINT16(0, fresh.role.value);
    TEST_ASSERT_FALSE(fresh.execution.running);
    TEST_ASSERT_EQUAL_UINT32(stoppedRevision, fresh.execution.stateRevision);
    TEST_ASSERT_EQUAL_UINT32(f->application.storage().activeProfileRevision,
                             fresh.execution.profileRevision);
    TEST_ASSERT_EQUAL_UINT32(profile->components.size(), fresh.deviceCount);
    for (std::size_t i = 0; i < fresh.deviceCount; ++i) {
        TEST_ASSERT_EQUAL_UINT16(profile->components.get(i)->id.value, fresh.devices[i].id.value);
        TEST_ASSERT_TRUE(fresh.devices[i].available);
    }
    f->now += ControlLimits::PublishMilliseconds;
    f->control.service(f->now);
    ConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readStatus(status));
    TEST_ASSERT_FALSE(status.rolePresent);
    TEST_ASSERT_EQUAL_UINT16(1001, status.devices[0].id);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(f->execute(f->command()).reply.outcome));
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_stale_owner_generation_is_unavailable_without_changing_game_revision() {
    f->execute(f->command());
    f->application.service(f->now + 5);
    f->owners.staleObservations();
    const auto value = f->application.game();
    TEST_ASSERT_TRUE(value.deviceCount > 0);
    for (std::size_t i = 0; i < value.deviceCount; ++i)
        TEST_ASSERT_FALSE(value.devices[i].available);
    TEST_ASSERT_EQUAL_UINT32(f->application.execution().stateRevision,
                             value.execution.stateRevision);
}

void test_pending_hardware_switch_does_not_publish_old_availability() {
    f->execute(f->command());
    f->now += 5;
    f->application.service(f->now);
    f->execute(f->command(GameCommandKind::Stop));
    TEST_ASSERT_TRUE(
        f->application.createDraft(FactoryProfile::Hint, f->application.storage().draftRevision));
    TEST_ASSERT_TRUE(f->application.activateDraft(f->application.storage().draftRevision,
                                                  f->application.activation().configurationRevision,
                                                  f->now));
    TEST_ASSERT_FALSE(f->application.activation().hardwareReady);
    const auto value = f->application.game();
    TEST_ASSERT_TRUE(value.deviceCount > 0);
    for (std::size_t i = 0; i < value.deviceCount; ++i)
        TEST_ASSERT_FALSE(value.devices[i].available);
}

void test_reply_selection_drops_old_epoch_and_selects_current_verified_recipient() {
    TEST_ASSERT_TRUE(f->submit(f->command()));
    f->control.service(++f->now); // Executed reply for epoch1 remains queued.
    f->channel.actual.updateAuthority(authority(2, false));
    f->channel.actual.updateAuthority(authority(3));
    f->control.service(++f->now);
    const auto stop = f->command(GameCommandKind::Stop);
    TEST_ASSERT_TRUE(f->submit(stop, 3));
    f->control.service(++f->now);
    RoutedGameReply selected;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(selected));
    TEST_ASSERT_EQUAL_UINT32(7, selected.principal);
    TEST_ASSERT_EQUAL_UINT32(3, selected.connectionEpoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(selected.reply.outcome));
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_FALSE(f->channel.actual.takeReply(selected));
}
void test_reply_selection_filters_wrong_principal_and_preserves_target_on_false() {
    RoutedGameReply foreign;
    foreign.principal = 8;
    foreign.connectionEpoch = 1;
    TEST_ASSERT_TRUE(f->channel.actual.send(foreign));
    RoutedGameReply wrongEpoch;
    wrongEpoch.principal = 7;
    wrongEpoch.connectionEpoch = 9;
    TEST_ASSERT_TRUE(f->channel.actual.send(wrongEpoch));
    RoutedGameReply selected;
    selected.principal = 99;
    selected.connectionEpoch = 42;
    selected.reply.request = 77;
    TEST_ASSERT_FALSE(f->channel.actual.takeCurrentReply(selected));
    TEST_ASSERT_EQUAL_UINT32(99, selected.principal);
    TEST_ASSERT_EQUAL_UINT32(42, selected.connectionEpoch);
    TEST_ASSERT_EQUAL_UINT32(77, selected.reply.request);
    TEST_ASSERT_FALSE(f->channel.actual.takeReply(foreign));
    FreeRtosGameControlChannel unopened;
    TEST_ASSERT_FALSE(unopened.takeCurrentReply(selected));
    TEST_ASSERT_EQUAL_UINT32(77, selected.reply.request);
}
void test_revocation_before_reply_selection_discards_full_queue() {
    RoutedGameReply reply;
    reply.principal = 7;
    reply.connectionEpoch = 1;
    for (unsigned i = 0; i < ControlLimits::Queue; ++i)
        TEST_ASSERT_TRUE(f->channel.actual.send(reply));
    host_rtos::beforeEnter = revokeAtAdmission;
    RoutedGameReply selected;
    selected.reply.request = 88;
    TEST_ASSERT_FALSE(f->channel.actual.takeCurrentReply(selected));
    TEST_ASSERT_EQUAL_UINT32(88, selected.reply.request);
    TEST_ASSERT_FALSE(f->channel.actual.takeReply(reply));
}
void test_revocation_after_reply_selection_cannot_change_copied_epoch() {
    TEST_ASSERT_TRUE(f->submit(f->command()));
    f->control.service(++f->now);
    RoutedGameReply selected;
    TEST_ASSERT_TRUE(f->channel.actual.takeCurrentReply(selected));
    f->channel.actual.updateAuthority(authority(2, false));
    f->channel.actual.updateAuthority(authority(3));
    TEST_ASSERT_EQUAL_UINT32(1, selected.connectionEpoch); // Never relabel a claimed old reply.
    TrustedAuthority current;
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(current));
    TEST_ASSERT_EQUAL_UINT32(3, current.epoch);
    TEST_ASSERT_TRUE(selected.connectionEpoch != current.epoch);
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_profile_capabilities_encoder_bounds_and_compiled_limits() {
    char bytes[maxProfileCapabilitiesJsonBytes + 2];
    std::memset(bytes, '#', sizeof(bytes));
    std::size_t written = 999;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::BufferTooSmall),
                      static_cast<unsigned>(encodeProfileCapabilities(nullptr, 0, written)));
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::BufferTooSmall),
                      static_cast<unsigned>(encodeProfileCapabilities(bytes + 1, 1, written)));
    TEST_ASSERT_EQUAL_CHAR(0, bytes[1]);
    TEST_ASSERT_EQUAL_CHAR('#', bytes[0]);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                      static_cast<unsigned>(encodeProfileCapabilities(
                          bytes + 1, maxProfileCapabilitiesJsonBytes, written)));
    const auto required = written;
    TEST_ASSERT_TRUE(required < maxProfileCapabilitiesJsonBytes);
    TEST_ASSERT_NOT_NULL(std::strstr(bytes + 1, "\"components\":16"));
    TEST_ASSERT_NOT_NULL(std::strstr(bytes + 1, "\"profileDocumentBytes\":131072"));
    TEST_ASSERT_NOT_NULL(std::strstr(bytes + 1, "\"commandDocumentBytes\":1024"));
    TEST_ASSERT_NOT_NULL(std::strstr(bytes + 1, "\"commandFramePayloadBytes\":120"));
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::BufferTooSmall),
        static_cast<unsigned>(encodeProfileCapabilities(bytes + 1, required, written)));
    TEST_ASSERT_EQUAL_UINT32(0, written);
    TEST_ASSERT_EQUAL_CHAR(0, bytes[1]);
    std::memset(bytes, '#', sizeof(bytes));
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeProfileCapabilities(bytes + 1, required + 1, written)));
    TEST_ASSERT_EQUAL_UINT32(required, written);
    TEST_ASSERT_EQUAL_CHAR(0, bytes[required + 1]);
    TEST_ASSERT_EQUAL_CHAR('#', bytes[required + 2]);
    TEST_ASSERT_EQUAL_CHAR('#', bytes[0]);
}
void test_catalog_led_limit_is_for_enabled_components() {
    using namespace larp::configuration;
    std::unique_ptr<Profile> profile(new Profile);
    TEST_ASSERT_TRUE(profile->name.assign("LED boundary"));
    State state;
    state.id = StateId(1);
    TEST_ASSERT_TRUE(state.name.assign("Ready"));
    TEST_ASSERT_TRUE(profile->states.push(state));
    profile->initialState = profile->errorState = state.id;
    Component led;
    led.id = ComponentId(1);
    TEST_ASSERT_TRUE(led.name.assign("LED"));
    led.kind = ComponentKind::LedStrip;
    led.pins[0] = 4;
    led.ledCount = Limits::Leds;
    TEST_ASSERT_TRUE(profile->components.push(led));
    TEST_ASSERT_TRUE(validateProfile(*profile));
    auto &component = *profile->components.get(0);
    component.ledCount = 0;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(Error::InvalidParameter),
                      static_cast<unsigned>(validateProfile(*profile).error));
    component.ledCount = Limits::Leds + 1;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(Error::InvalidParameter),
                      static_cast<unsigned>(validateProfile(*profile).error));
    component.enabled = false;
    component.ledCount = UINT16_MAX;
    TEST_ASSERT_TRUE(validateProfile(*profile));
    // An enabled unrelated kind also carries the uint16 field without using it.
    component.enabled = true;
    component.kind = ComponentKind::EspNow;
    component.pins[0] = UnusedPin;
    component.peer[0] = 2;
    TEST_ASSERT_TRUE(validateProfile(*profile));
}
void test_profile_capability_fixture_matches_firmware_encoder() {
    char bytes[maxProfileCapabilitiesJsonBytes];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeProfileCapabilities(bytes, sizeof(bytes), written)));
    const char *path = "docs/companion/fixtures/profile-capabilities-v1.json";
    const auto *flag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (flag && std::strcmp(flag, "1") == 0) {
        auto *file = std::fopen(path, "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL_UINT32(written, std::fwrite(bytes, 1, written, file));
        TEST_ASSERT_EQUAL_INT('\n', std::fputc('\n', file));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    }
    auto *file = std::fopen(path, "rt");
    TEST_ASSERT_NOT_NULL(file);
    char expected[maxProfileCapabilitiesJsonBytes + 1];
    const auto count = std::fread(expected, 1, sizeof(expected), file);
    TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    TEST_ASSERT_EQUAL_UINT32(written + 1, count);
    TEST_ASSERT_EQUAL_CHAR('\n', expected[written]);
    TEST_ASSERT_EQUAL_MEMORY(bytes, expected, written);
}
#include "CommandStreamCases.h"
#include "FrameQueueCases.h"
#include "JournalFaultCases.h"
#include "ReplyFrameCases.h"
// Uses the stream/queue/bearer helpers defined above; preserve this order.
#include "CommandReceiptCases.h"
#include "StatusRoutingCases.h"
// Depends on the real bearer and routing helpers above.
#include "StatusFrameCases.h"
// Identity cases depend on real status and bearer helpers.
#include "IdentityAccessCases.h"
// SDK admission cases depend on the actual identity and queue helpers.
#include "GattsWriteCases.h"
#include "ProfileImportCases.h"
// Apply cases require the actual source/import fixture helpers above.
#include "ProfileApplyCases.h"
// Frame cases require import and apply fixture helpers above.
#include "ProfileFrameCases.h"
// Staging cases use the actual frame/import/apply fixtures and host file adapter.
#include "ProfileStagingCases.h"
// Queue cases use completed source and actual staging fixture helpers above.
#include "ProfileFrameQueueCases.h"
// Control cases need the actual queue/source/application and bearer fixtures.
#include "ProfileControlCases.h"
// Response queue cases need actual control/file/application and sender helpers.
#include "ProfileResponseQueueCases.h"
// Profile SDK cases need completed owners/input/output/ATT fixture helpers.
#include "GattsProfileWriteCases.h"
// File cases use original Raw/Owner/response helpers and actual ESP VFS adapters.
#include "ProfileFileCases.h"
// Persistent public labels reuse policy and actual NVS adapter helpers.
#include "DeviceIdentityStoreCases.h"
// Application file ownership uses actual ESP/source/control helpers above.
#include "ApplicationProfileStagingCases.h"
#include "GattsLifecycleCases.h"
#include "GattsResponseQueueCases.h"
#include "GattsServiceCases.h"
#include "GattsSourceCases.h"
#include "IdentityRetirementCases.h"
#include "PeerAuthCases.h"
#include "PeerLinkSecurityCases.h"
#include "PeerProofCases.h"
#include "PeerProofGrantCases.h"
#include "PeerRegistryActorCases.h"
#include "PeerRegistryCases.h"
} // namespace
void setUp() {
    host_rtos::depth = 0;
    host_rtos::beforeEnter = nullptr;
    host_rtos::nonzeroWait = false;
    f.reset(new Fixture);
    f->ready();
}
void tearDown() {
    TEST_ASSERT_EQUAL_UINT32(0, host_rtos::depth);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
    host_rtos::beforeEnter = nullptr;
    f.reset();
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_codec_roundtrip_boundaries_and_capacity);
    RUN_TEST(test_codec_reordered_escaped_keys_and_values);
    RUN_TEST(test_codec_rejections_preserve_destination);
    RUN_TEST(test_uninitialized_and_missing_metadata_skip_decode);
    RUN_TEST(test_authority_fault_fences_duplicate_and_new_epoch_recovers);
    RUN_TEST(test_authority_maximum_never_wraps_or_reopens);
    RUN_TEST(test_complete_json_starts_stored_game_and_publishes_copied_state);
    RUN_TEST(test_idempotent_authority_and_command_retry_keep_original_receipt);
    RUN_TEST(test_old_queue_and_recycled_principal_rejected_after_reconnect);
    RUN_TEST(test_full_queue_cannot_lose_revocation);
    RUN_TEST(test_revocation_between_decode_and_admission_rejects);
    RUN_TEST(test_revocation_before_final_observation_prevents_operation);
    RUN_TEST(test_revocation_after_claim_allows_one_operation_then_closes);
    RUN_TEST(test_suspend_preserves_game_and_needs_new_epoch);
    RUN_TEST(test_stale_revision_consumed_wrong_boot_and_hole_not_consumed);
    RUN_TEST(test_evicted_receipt_never_reexecutes_old_request);
    RUN_TEST(test_dropped_reply_retry_executes_once);
    RUN_TEST(test_copied_message_survives_source_reuse);
    RUN_TEST(test_wrong_principal_and_epoch_cannot_admit);
    RUN_TEST(test_pending_configuration_blocks_start_and_consumes_request);
    RUN_TEST(test_app_fixtures_match_actual_firmware_vertical_flow);
    RUN_TEST(test_ready_inventory_exists_before_first_game_start);
    RUN_TEST(test_profile_change_hides_old_role_and_rebinds_inventory_without_resetting_revision);
    RUN_TEST(test_stale_owner_generation_is_unavailable_without_changing_game_revision);
    RUN_TEST(test_pending_hardware_switch_does_not_publish_old_availability);
    RUN_TEST(test_frame_codec_limits_endianness_and_target_preservation);
    RUN_TEST(test_fragmented_command_runs_through_real_queue_application_and_reply);
    RUN_TEST(test_partial_and_duplicate_fragments_do_not_execute_or_extend_deadline);
    RUN_TEST(test_fragment_gaps_overlap_and_other_message_preserve_pending);
    RUN_TEST(test_conflicting_duplicate_and_total_discard_pending);
    RUN_TEST(test_reconnect_cannot_join_old_fragments_or_erase_new_pending);
    RUN_TEST(test_revocation_discards_complete_queuefull_message);
    RUN_TEST(test_queuefull_retains_exact_document_and_explicit_retry_executes_once);
    RUN_TEST(test_fragment_expiry_across_clock_wrap_and_busy_frames);
    RUN_TEST(test_queuefull_expiry_never_submits_after_deadline);
    RUN_TEST(test_fragmented_malformed_json_cannot_consume_game_request);
    RUN_TEST(test_maximum_document_and_variable_chunk_sizes);
    RUN_TEST(test_revoke_after_fragment_observation_still_fails_admission);
    RUN_TEST(test_frame_fixture_matches_canonical_producer);
    RUN_TEST(test_existing_only_empty_and_corrupt_boot_never_create_records);
    RUN_TEST(test_partial_and_failed_truncate_preserve_confirmed_profile_for_reboot);
    RUN_TEST(test_uncertain_readable_record_cannot_start_until_confirmed_retry);
    RUN_TEST(test_damaged_newer_records_fall_back_without_auto_start);
    RUN_TEST(test_equal_generations_are_ambiguous_and_do_not_overwrite);
    RUN_TEST(test_future_record_version_preserves_bytes_and_blocks_save);
    RUN_TEST(test_backend_open_read_close_failures_block_initialization);
    RUN_TEST(test_import_close_failure_never_leaves_ready_draft);
    RUN_TEST(test_record_changed_after_inspection_is_rejected);
    RUN_TEST(test_generation_maximum_never_wraps_or_overwrites);
    RUN_TEST(test_semantically_invalid_record_falls_back_to_valid_profile);
    RUN_TEST(test_missing_previously_confirmed_records_are_not_fresh_empty_store);
    RUN_TEST(test_reply_selection_drops_old_epoch_and_selects_current_verified_recipient);
    RUN_TEST(test_reply_selection_filters_wrong_principal_and_preserves_target_on_false);
    RUN_TEST(test_revocation_before_reply_selection_discards_full_queue);
    RUN_TEST(test_revocation_after_reply_selection_cannot_change_copied_epoch);
    RUN_TEST(test_profile_capabilities_encoder_bounds_and_compiled_limits);
    RUN_TEST(test_catalog_led_limit_is_for_enabled_components);
    RUN_TEST(test_profile_capability_fixture_matches_firmware_encoder);
    RUN_TEST(test_frame_queue_validates_outer_write_and_preserves_empty_destination);
    RUN_TEST(test_frame_queue_owns_bytes_and_trusted_metadata);
    RUN_TEST(test_frame_queue_full_does_not_report_copied_or_consume_command);
    RUN_TEST(test_queued_frames_reach_actual_stored_game_and_current_reply);
    RUN_TEST(test_frame_queue_revoke_and_reconnect_reject_buffered_old_frames);
    RUN_TEST(test_frame_queue_wait_does_not_extend_absolute_deadline);
    RUN_TEST(test_empty_frame_queue_poll_revokes_without_retry_or_game_tick);
    RUN_TEST(test_frame_copy_after_authority_observation_is_rechecked_by_stream);
    RUN_TEST(test_reply_frame_codec_separate_kind_bounds_and_target_preservation);
    RUN_TEST(test_executed_reply_frames_reconstruct_actual_firmware_fixture);
    RUN_TEST(test_reply_sender_busy_retains_exact_frame_and_original_deadline);
    RUN_TEST(test_reply_sender_frame_capacity_and_pending_reply_preservation);
    RUN_TEST(test_reply_sender_reconnect_rejects_old_reply_and_binds_new_epoch);
    RUN_TEST(test_reply_bearer_rechecks_revoke_before_copy);
    RUN_TEST(test_reply_copy_before_later_revoke_keeps_original_epoch);
    RUN_TEST(test_reply_sender_deadline_wrap_and_invalid_reply);
    RUN_TEST(test_reply_message_counter_exhaustion_never_wraps_and_new_epoch_recovers);
    RUN_TEST(test_command_receipt_codec_codes_bounds_and_target_preservation);
    RUN_TEST(test_actual_fragment_receipts_precede_execution_and_match_fixture);
    RUN_TEST(test_receipt_busy_does_not_undo_queued_command_or_retry);
    RUN_TEST(test_command_queue_full_receipt_and_explicit_duplicate_retry);
    RUN_TEST(test_receipt_decode_rejection_does_not_consume_game_request);
    RUN_TEST(test_receipt_distinguishes_input_from_busy_pending_message);
    RUN_TEST(test_receipt_expired_queuewait_and_empty_poll);
    RUN_TEST(test_receipt_reconnect_and_bearer_revoke_keep_original_binding);
    RUN_TEST(test_current_status_filters_reconnect_until_new_producer_snapshot);
    RUN_TEST(test_delayed_old_status_cannot_be_relabelled_to_current_epoch);
    RUN_TEST(test_status_selection_revoke_observer_suspend_preserves_game);
    RUN_TEST(test_status_producer_observation_race_keeps_original_epoch);
    RUN_TEST(test_status_frame_codec_kind_capacity_and_target_preservation);
    RUN_TEST(test_ready_mailbox_status_frames_match_encoder_and_independent_fixture);
    RUN_TEST(test_running_mailbox_status_128_frames_match_actual_start_fixture);
    RUN_TEST(test_status_snapshot_stays_frozen_when_real_mailbox_is_overwritten);
    RUN_TEST(test_real_reply_priority_reserves_whole_step_including_completion);
    RUN_TEST(test_status_busy_keeps_exact_frame_then_first_copy_freshness_expires);
    RUN_TEST(test_status_partial_busy_and_deferral_never_renew_absolute_deadline);
    RUN_TEST(test_status_deadline_clock_wrap_capacity_invalid_and_cancel);
    RUN_TEST(test_status_source_age_sample_and_boot_cannot_reopen_old_document);
    RUN_TEST(test_status_reconnect_rejects_old_tags_and_new_producer_resets_counter);
    RUN_TEST(test_status_bearer_revoke_before_and_after_copy_observer_and_unavailable);
    RUN_TEST(test_status_message_exhaustion_no_wrap_and_cancel_consumes_identity);
    RUN_TEST(test_device_identity_codec_bounds_and_public_fixture_only);
    RUN_TEST(test_authority_owned_epoch_advance_above_fault_and_maximum_no_wrap);
    RUN_TEST(test_verified_connection_needs_explicit_gm_assignment_before_actual_start);
    RUN_TEST(test_assigned_peer_without_each_verification_requirement_never_admits);
    RUN_TEST(test_duplicate_auth_and_assignment_preserve_epoch_and_game_receipt);
    RUN_TEST(test_recycled_handle_stale_auth_and_disconnect_cannot_change_new_binding);
    RUN_TEST(test_gm_revocation_fences_copied_stop_but_preserves_running_game);
    RUN_TEST(test_conflicting_identity_closes_and_same_link_cannot_reopen);
    RUN_TEST(test_identity_registry_bounds_unique_principals_and_explicit_slot_reuse);
    RUN_TEST(test_identity_device_must_be_valid_and_remains_immutable_until_new_boot);
    RUN_TEST(test_identity_maximum_epoch_and_channel_suspend_cannot_be_reopened_by_duplicates);
    RUN_TEST(test_identity_route_rechecks_actual_channel_and_preserves_false_target);
    RUN_TEST(test_delayed_verification_cannot_reopen_revoked_channel_at_publish_point);
    RUN_TEST(test_delayed_local_assignment_cannot_reopen_revoked_verified_link);
    RUN_TEST(test_gatts_complete_frames_actual_application_and_independent_tool_fixture);
    RUN_TEST(test_gatts_wrong_sdk_context_never_copies_or_responds_on_other_peer);
    RUN_TEST(test_gatts_missing_original_binding_does_not_infer_current_authority);
    RUN_TEST(test_gatts_attribute_prepare_offset_length_and_header_errors_are_explicit);
    RUN_TEST(test_gatts_queue_full_reports_resources_without_positive_copy_or_retry);
    RUN_TEST(test_gatts_response_loss_preserves_copied_input_without_reexecution);
    RUN_TEST(test_gatts_write_without_response_still_copies_only_one_bounded_frame);
    RUN_TEST(test_gatts_revocation_before_copy_and_between_observations_rejects);
    RUN_TEST(test_gatts_copied_before_revoke_retains_original_epoch_and_cannot_execute);
    RUN_TEST(test_gatts_recycled_handle_old_envelope_never_gets_new_response_binding);
    RUN_TEST(test_gatts_queue_wait_uses_original_callback_time_not_dispatch_time);
    RUN_TEST(test_gatts_uninitialized_queue_cannot_ack_positive_copy);
    RUN_TEST(test_profile_source_codec_exact_envelope_and_failure_preserves_output);
    RUN_TEST(test_profile_source_actual_private_draft_receipt_without_activation_or_save);
    RUN_TEST(test_profile_source_unauthorized_inputs_never_consume_or_edit);
    RUN_TEST(test_profile_source_deadline_and_future_time_never_consume_or_extend);
    RUN_TEST(test_profile_source_stale_running_and_busy_do_not_touch_input);
    RUN_TEST(test_profile_source_length_digest_and_io_failure_leave_no_ready_draft);
    RUN_TEST(test_profile_source_malformed_and_invalid_profiles_never_issue_receipt);
    RUN_TEST(test_profile_source_revoke_during_byte_and_eof_cannot_become_complete);
    RUN_TEST(test_profile_source_original_envelope_and_route_survive_caller_mutation);
    RUN_TEST(test_profile_source_late_revoke_after_validated_eof_has_no_apply_receipt);
    RUN_TEST(test_profile_source_expiry_during_input_and_after_eof_never_issues_receipt);
    RUN_TEST(test_profile_source_maximum_document_and_clock_wrap_stay_bounded);
    RUN_TEST(test_profile_source_reentrant_source_cannot_edit_start_or_tick_application);
    RUN_TEST(test_profile_apply_actual_queue_confirmation_and_current_wire_status);
    RUN_TEST(test_profile_apply_default_receipt_has_no_authority_or_application_effect);
    RUN_TEST(test_profile_apply_wrong_application_rejected_even_with_same_revisions);
    RUN_TEST(test_profile_apply_original_epoch_cannot_be_reissued_after_revocation);
    RUN_TEST(test_profile_apply_local_draft_edit_invalidates_old_receipt);
    RUN_TEST(test_profile_apply_game_start_after_import_denies_apply_without_stopping_game);
    RUN_TEST(test_profile_apply_rechecks_original_authority_after_admission_selection);
    RUN_TEST(test_profile_apply_duplicate_is_busy_then_stale_without_repeating_save);
    RUN_TEST(test_profile_apply_later_revoke_preserves_already_selected_transaction);
    RUN_TEST(test_profile_apply_uncertain_storage_is_fault_not_confirmed_or_started);
    RUN_TEST(test_profile_apply_copies_original_receipt_before_external_observation);
    RUN_TEST(test_profile_apply_late_unaccepted_import_cannot_use_locally_parsed_draft);

    RUN_TEST(test_profile_frames_codec_32bit_offsets_exact_bounds_and_in_place_encoding);
    RUN_TEST(test_profile_frames_codec_rejects_bad_values_without_mutating_destination);
    RUN_TEST(test_profile_frames_actual_20byte_complete_import_apply_and_golden_values);
    RUN_TEST(test_profile_frames_actual_128byte_complete_import_apply_and_golden_values);
    RUN_TEST(test_profile_frames_missing_duplicate_reordered_foreign_and_extra_are_terminal);
    RUN_TEST(test_profile_frames_invalid_metadata_never_reads_and_end_remains_final);
    RUN_TEST(test_profile_frames_owned_value_and_metadata_survive_caller_mutation);
    RUN_TEST(test_profile_frames_crc_mismatch_has_no_receipt_activation_or_save);
    RUN_TEST(test_profile_frames_partial_or_extra_source_cannot_become_accepted_draft);
    RUN_TEST(test_profile_frames_revoke_or_expire_during_frame_copy_prevents_acceptance);
    RUN_TEST(test_profile_frames_original_route_and_metadata_copied_before_source_call);
    RUN_TEST(test_profile_frames_maximum_document_crosses_16bit_boundary_in_private_slot);

    RUN_TEST(test_profile_staging_actual_file_20byte_frames_import_apply_confirm);
    RUN_TEST(test_profile_staging_actual_file_128byte_frames_import_apply_confirm);
    RUN_TEST(test_profile_staging_finish_is_explicit_and_end_is_final);
    RUN_TEST(test_profile_staging_wrong_route_or_id_does_not_mutate_original_transfer);
    RUN_TEST(test_profile_staging_bad_missing_duplicate_extra_or_digest_never_seals);
    RUN_TEST(test_profile_staging_backend_open_write_seal_and_discard_failures_are_fenced);
    RUN_TEST(test_profile_staging_waiting_revoke_or_deadline_service_discards_without_game_stop);
    RUN_TEST(test_profile_staging_seal_does_not_restart_original_deadline);
    RUN_TEST(test_profile_staging_revoke_or_expire_inside_backend_prevents_complete);
    RUN_TEST(test_profile_staging_source_io_length_and_crc_are_checked_again_after_seal);
    RUN_TEST(test_profile_staging_reentrant_storage_cannot_replace_discard_or_finish_owner);
    RUN_TEST(test_profile_staging_copies_metadata_route_and_payload_before_backend_call);
    RUN_TEST(test_profile_staging_actual_file_maximum_is_complete_in_existing_private_slot);
    RUN_TEST(test_profile_staging_discard_invalid_authority_and_future_timestamp_do_not_open);
    RUN_TEST(test_profile_staging_clock_wrap_retains_original_full_transfer_budget);
    RUN_TEST(test_profile_staging_revoke_inside_source_read_preserves_byte_and_closes_source);

    RUN_TEST(test_profile_staging_freezes_frame_before_first_authority_observation);
    RUN_TEST(test_profile_staging_revoke_during_cleanup_does_not_open_new_source);

    RUN_TEST(test_profile_staging_running_game_blocks_import_and_revoke_does_not_stop_it);

    RUN_TEST(test_profile_queue_actual_copied_frames_to_temp_private_apply_and_status);
    RUN_TEST(test_profile_queue_full_exact_retry_copies_and_preserves_false_destination);
    RUN_TEST(test_profile_queue_callback_copy_survives_first_observation_and_caller_mutation);
    RUN_TEST(test_profile_queue_rejects_prepared_offset_wrong_kind_bounds_and_untrusted_route);
    RUN_TEST(test_profile_queue_pump_takes_one_frame_and_does_not_finish_empty_queue);
    RUN_TEST(test_profile_queue_original_metadata_time_counts_wait_and_deadline);
    RUN_TEST(test_profile_queue_expired_future_and_wrapped_metadata_keep_fixed_age);
    RUN_TEST(test_profile_queue_revoke_before_owner_start_never_opens_or_relabels_metadata);
    RUN_TEST(test_profile_queue_revoke_before_queued_data_closes_source_without_app_effect);
    RUN_TEST(test_profile_queue_reentrant_backend_cannot_take_next_pending_frame);
    RUN_TEST(test_profile_queue_bad_port_item_stays_bounded_without_backend_or_borrowed_result);

    RUN_TEST(test_profile_control_codec_exact_layout_limits_and_failed_destination);
    RUN_TEST(test_profile_control_actual_queue_source_finish_apply_saved_current_status);
    RUN_TEST(test_profile_control_duplicate_finish_apply_replays_without_import_or_save);
    RUN_TEST(test_profile_control_request_conflict_old_order_no_reexecution);
    RUN_TEST(test_profile_control_abort_clears_only_temporary_and_server_proof);
    RUN_TEST(test_profile_control_incomplete_finish_and_semantic_failure_issue_no_proof);
    RUN_TEST(test_profile_control_running_game_denies_finish_and_preserves_game);
    RUN_TEST(test_profile_control_stale_draft_denies_apply);
    RUN_TEST(test_profile_control_late_apply_has_fresh_rights_without_source_deadline_reset);
    RUN_TEST(test_profile_control_own_queue_age_future_wrap_and_response_backpressure);
    RUN_TEST(test_profile_control_pending_response_polls_source_deadline_and_blocks_take);
    RUN_TEST(test_profile_control_revoke_reconnect_never_relabels_old_proof_or_response);
    RUN_TEST(test_profile_control_final_rights_observation_blocks_apply_and_response);
    RUN_TEST(test_profile_control_later_revoke_finishes_selected_transaction_silently);
    RUN_TEST(test_profile_control_uncertain_journal_produces_failed_never_saved);
    RUN_TEST(test_profile_control_abort_after_selected_apply_waits_preserves_transaction);
    RUN_TEST(test_profile_response_sender_original_copy_busy_priority_expiry);
    RUN_TEST(test_profile_response_sender_bearer_recheck_and_old_generation);
    RUN_TEST(test_profile_response_scheduler_completing_game_reply_owns_entire_step);
    RUN_TEST(test_profile_control_queue_admits_exact12_rejects_malformed_and_full_retry);
    RUN_TEST(test_profile_control_apply_while_running_is_rejected_without_game_change);
    RUN_TEST(test_profile_control_reply_pending_revoke_and_same_source_deadline);
    RUN_TEST(test_profile_control_freezes_command_and_reentrant_call_has_no_effect);
    RUN_TEST(test_profile_control_new_metadata_invalidates_previous_private_proof);
    RUN_TEST(test_profile_response_sender_selected_bytes_survive_caller_mutation);
    RUN_TEST(test_profile_response_queue_actual_owner_copied_output_sender_saved_status);
    RUN_TEST(test_profile_response_queue_full_owner_retains_exact_output_and_input);
    RUN_TEST(test_profile_response_queue_invalid_uninitialized_future_and_wrong_route);
    RUN_TEST(test_profile_response_queue_freezes_all_producer_scalars_before_observation);
    RUN_TEST(test_profile_response_queue_consumer_discards_four_stale_without_relabel);
    RUN_TEST(test_profile_response_queue_expiry_queue_wait_wrap_preserves_original_time);
    RUN_TEST(test_profile_response_queue_rights_loss_after_copy_selection_is_not_delivery);
    RUN_TEST(test_profile_response_queue_rights_loss_after_take_selection_sender_rechecks);
    RUN_TEST(test_profile_response_owner_offer_reentrant_and_expired_retains_bounds);
    RUN_TEST(test_profile_response_owner_full_port_revoke_closes_without_game_or_store_effect);
    RUN_TEST(test_profile_response_selection_busy_does_not_take_and_load_preserves_time);
    RUN_TEST(test_profile_response_selection_reentrant_clock_authority_does_not_take_twice);
    RUN_TEST(test_gatts_profile_actual_raw_queue_control_output_sender_confirmed_journal);
    RUN_TEST(test_gatts_profile_full_retry_att_resource_error_is_not_success);
    RUN_TEST(test_gatts_profile_context_prepare_offset_type_and_shape_gates);
    RUN_TEST(test_gatts_profile_without_att_response_still_only_copies_queue);
    RUN_TEST(test_gatts_profile_response_failure_does_not_undo_copy_or_relabel_epoch);
    RUN_TEST(test_gatts_profile_revoke_between_outer_and_queue_admission_prevents_copy);
    RUN_TEST(test_gatts_profile_freezes_event_binding_and_bytes_before_first_observation);
    RUN_TEST(test_gatts_command_shared_gate_freezes_event_binding_and_bytes_too);
    RUN_TEST(test_gatts_profile_uninitialized_and_no_binding_do_not_ack_success);
    RUN_TEST(test_file_staging_exact_length_bounds_and_target_preservation);
    RUN_TEST(test_file_staging_partial_write_poison_and_failed_cleanup_fence);
    RUN_TEST(test_file_staging_failed_open_or_seal_requires_discard);
    RUN_TEST(test_file_staging_short_extra_fault_or_invalid_end_poison_read);
    RUN_TEST(test_file_staging_freezes_append_and_fences_all_nested_operations);
    RUN_TEST(test_file_staging_actual_host_maximum_source_and_reuse);
    RUN_TEST(test_esp_staging_requires_existing_mount_and_never_formats);
    RUN_TEST(test_esp_staging_claim_blocks_second_source_until_confirmed_cleanup);
    RUN_TEST(test_esp_staging_failed_open_partial_write_flush_close_or_reopen_no_source);
    RUN_TEST(test_esp_staging_read_only_exact_end_and_corruption_fences);
    RUN_TEST(test_esp_staging_old_boot_source_discard_preserves_both_journal_names);
    RUN_TEST(test_esp_staging_actual_raw_queue_private_apply_output_saved_model);
    RUN_TEST(test_esp_staging_source_revoke_discards_only_temporary_and_keeps_game_journal);
    RUN_TEST(test_esp_staging_actual_maximum_source_with_fixed_append_budget);
    RUN_TEST(test_esp_staging_close_failure_keeps_claim_even_after_file_removal);
    RUN_TEST(test_esp_staging_import_cleanup_failure_issues_no_apply_receipt);
    RUN_TEST(test_esp_staging_destroyed_fault_child_address_reuse_requires_cleanup);
    RUN_TEST(test_device_identity_record_exact_crc_bounds_and_target_preservation);
    RUN_TEST(test_device_identity_loaded_is_immutable_without_entropy_or_writes);
    RUN_TEST(test_device_identity_missing_create_verify_and_restart_keep_same_label);
    RUN_TEST(test_device_identity_entropy_failure_or_zero_never_writes);
    RUN_TEST(test_device_identity_read_failure_or_corrupt_never_regenerates);
    RUN_TEST(test_device_identity_uncertain_or_failed_write_never_publishes_candidate);
    RUN_TEST(test_device_identity_confirmed_readback_must_match_and_be_complete);
    RUN_TEST(test_device_identity_concurrent_existing_value_wins_without_overwrite);
    RUN_TEST(test_device_identity_reentrant_begin_and_observation_expose_no_pending_label);
    RUN_TEST(test_device_identity_nvs_init_full_or_mutex_busy_never_erases_or_opens);
    RUN_TEST(test_device_identity_actual_nvs_startup_record_public_metadata_restart);
    RUN_TEST(test_device_identity_nvs_bad_type_length_or_second_read_preserves_target);
    RUN_TEST(test_device_identity_actual_nvs_set_or_commit_uncertain_stays_closed);
    RUN_TEST(test_device_identity_actual_nvs_freezes_record_and_serializes_other_instance);
    RUN_TEST(test_device_identity_startup_entropy_call_order_and_zero_target_preservation);
    RUN_TEST(test_public_device_identity_persistence_never_opens_game_authority);
    RUN_TEST(test_application_file_owner_spans_receive_seal_read_and_cleanup);
    RUN_TEST(test_application_file_running_refuses_all_backend_io_preserves_game);
    RUN_TEST(test_application_file_empty_or_pending_refuses_collection);
    RUN_TEST(test_application_file_foreign_bridge_cannot_steal_or_release_owner);
    RUN_TEST(test_application_file_invalid_bounds_no_io_or_accidental_release);
    RUN_TEST(test_application_file_backend_failure_keeps_start_fence_until_cleanup);
    RUN_TEST(test_application_file_failed_discard_quarantine_survives_address_reuse);
    RUN_TEST(test_application_file_abandoned_source_needs_explicit_cleanup_not_new_begin);
    RUN_TEST(test_application_file_gate_precedes_every_external_call_and_freezes_append);
    RUN_TEST(test_application_file_invalid_reader_status_preserves_target_and_fences);
    RUN_TEST(test_application_file_actual_esp_receiving_blocks_gm_start_then_abort_releases);
    RUN_TEST(test_application_file_actual_esp_running_metadata_does_no_vfs_work);
    RUN_TEST(test_application_file_revoke_or_expiry_failed_cleanup_keeps_start_fence);
    RUN_TEST(test_application_file_actual_import_reads_inside_operating_guards);
    RUN_TEST(test_application_file_finish_cleanup_failure_never_grants_apply_or_start);
    RUN_TEST(test_application_file_actual_esp_cleanup_quarantine_observation);
    RUN_TEST(test_application_file_import_revoke_cleans_inside_application_guard);
    RUN_TEST(test_application_file_startup_cleanup_blocks_nested_boot_and_preserves_quarantine);
    RUN_TEST(test_profile_followup_failed_open_cannot_apply_previous_import_receipt);
    RUN_TEST(test_profile_followup_wrong_route_cannot_cancel_current_receipt);
    RUN_TEST(test_profile_followup_expired_metadata_preserves_current_receipt);
    RUN_TEST(test_profile_followup_attempt_during_selected_apply_preserves_confirmation);
    RUN_TEST(test_gatts_response_queue_actual_command_application_and_tool_observation);
    RUN_TEST(test_gatts_response_queue_profile_control_private_import_and_saved_reply);
    RUN_TEST(test_gatts_response_queue_uninitialized_invalid_status_binding_and_future);
    RUN_TEST(test_gatts_response_queue_full_fifo_begin_idempotent_and_no_retry);
    RUN_TEST(test_gatts_response_queue_freezes_all_fields_before_clock_observation);
    RUN_TEST(test_gatts_response_queue_original_handle_interface_peer_attribute_never_relabels);
    RUN_TEST(test_gatts_response_queue_drops_only_four_stale_and_keeps_output_unchanged);
    RUN_TEST(test_gatts_response_queue_absolute_age_boundary_and_clock_wrap);
    RUN_TEST(test_gatts_response_queue_close_after_admission_is_rechecked_by_consumer);
    RUN_TEST(test_gatts_response_dispatch_close_or_expire_after_selection_never_posts);
    RUN_TEST(test_gatts_response_dispatch_submitter_rechecks_original_boundary_without_retry);
    RUN_TEST(test_gatts_response_dispatch_reentry_cannot_take_next_response);
    RUN_TEST(test_gatts_response_queue_closed_gm_can_receive_authorization_error_on_original_link);
    RUN_TEST(test_gatts_response_queue_full_or_failed_post_preserves_original_input);
    RUN_TEST(test_gatts_response_queue_gm_revoke_keeps_copy_ack_but_cannot_execute_input);
    RUN_TEST(test_gatts_response_queue_all_sdk_statuses_and_zero_transaction_are_bounded);
    RUN_TEST(test_gatts_response_queue_raw_write_without_response_never_enqueues_output);
    RUN_TEST(test_gatts_response_queue_actual_input_full_is_resource_error_not_success);
    RUN_TEST(test_identity_old_disconnect_preserves_replacement_and_actual_game_control);
    RUN_TEST(test_identity_retired_owner_cannot_regrant_reconnect_or_reset);
    RUN_TEST(test_identity_disconnect_uses_role_authority_not_original_connection_token);
    RUN_TEST(test_authority_retirement_current_allocates_closed_epoch_without_wrap);
    RUN_TEST(test_authority_retirement_full_observation_mismatch_preserves_current_and_destination);
    RUN_TEST(test_authority_retirement_unavailable_maximum_and_grant_policy_are_separate);
    RUN_TEST(test_authority_retirement_adapter_freezes_expected_before_lock_observation);
    RUN_TEST(test_identity_disconnect_rechecks_replacement_at_atomic_adapter_boundary);
    RUN_TEST(test_identity_suspended_disconnect_can_reconnect_with_fresh_original_token);
    RUN_TEST(test_authority_retirement_uninitialized_adapter_preserves_destination);
    RUN_TEST(test_identity_retirement_stale_copied_frame_cannot_execute_or_close_new_gm);
    RUN_TEST(test_identity_old_security_failure_preserves_replacement_authority);
    RUN_TEST(test_identity_old_role_revocation_preserves_replacement_authority);
    RUN_TEST(test_service_actual_sdk_callbacks_original_installation_start_and_game);
    RUN_TEST(test_service_sdk_api_post_is_not_completion_and_duplicates_never_repost);
    RUN_TEST(test_service_wrong_uuid_handle_order_instance_primary_and_failed_events);
    RUN_TEST(test_service_local_sdk_post_failures_each_close_without_retry);
    RUN_TEST(test_service_full_callback_queue_and_wrong_producer_close_without_sdk);
    RUN_TEST(test_service_missing_registration_wrong_interface_and_order_never_start);
    RUN_TEST(test_service_first_deadline_not_refreshed_and_clock_wrap);
    RUN_TEST(test_service_endpoint_original_queue_age_failure_and_connect_before_install);
    RUN_TEST(test_service_callbacks_and_endpoint_job_freeze_before_observation);
    RUN_TEST(test_service_wrong_worker_reentrant_api_and_current_ready_sdk_instance);
    RUN_TEST(test_endpoint_registration_tokens_cannot_rebind_other_instances);
    RUN_TEST(test_endpoint_fixed_once_slot_duplicate_rejected_retry_and_snapshot_scope);
    RUN_TEST(test_service_start_confirmation_wrong_failed_late_and_premature);
    RUN_TEST(test_service_lost_start_confirmation_expires_without_retry);
    RUN_TEST(test_service_callback_loss_after_ready_immediately_closes_readiness);
    RUN_TEST(test_endpoint_failed_post_wrong_task_and_replaced_sdk_never_retry);
    RUN_TEST(test_source_original_sdk_post_actual_frames_and_application);
    RUN_TEST(test_source_posted_old_connection_dropped_after_same_handle_reconnect);
    RUN_TEST(test_source_full_fixed_pool_failed_post_and_slot_reuse);
    RUN_TEST(test_source_deadline_rechecked_after_post_without_refresh);
    RUN_TEST(test_source_wrong_task_sdk_instance_and_reentrant_consumption);
    RUN_TEST(test_source_revoked_gm_original_copy_ack_never_executes);
    RUN_TEST(test_source_null_overlong_prepared_execute_and_unknown_writes);
    RUN_TEST(test_source_owned_write_and_pending_response_survive_external_mutation);
    RUN_TEST(test_source_configuration_before_any_link_and_wrong_actor_fail_closed);
    RUN_TEST(test_source_closed_context_profile_admission_and_endpoint_immutability);
    RUN_TEST(
        test_source_registration_wrapper_preserves_other_callbacks_and_captures_before_forward);
    RUN_TEST(test_lifecycle_closed_sdk_context_grants_no_authority_but_copies_att_error);
    RUN_TEST(test_lifecycle_actual_frames_start_and_original_status_fixture);
    RUN_TEST(test_lifecycle_reused_handle_rejects_original_queued_output_and_old_retirement);
    RUN_TEST(test_lifecycle_wrong_context_and_bad_endpoints_preserve_outputs);
    RUN_TEST(test_lifecycle_unexpected_connect_closes_old_transport_and_rights);
    RUN_TEST(test_lifecycle_freezes_callback_bytes_before_authority_observation);
    RUN_TEST(test_lifecycle_retirement_closes_transport_before_external_authority_observation);
    RUN_TEST(test_lifecycle_current_role_revoke_keeps_error_response_context);
    RUN_TEST(test_lifecycle_profile_endpoint_uses_actual_independent_queue);
    RUN_TEST(test_lifecycle_failed_replacement_closes_transport_preserving_new_authority);
    RUN_TEST(test_lifecycle_reentrant_callback_does_not_relabel_original);
    RUN_TEST(test_lifecycle_first_time_is_not_refreshed_by_capture);
    RUN_TEST(test_lifecycle_original_live_freezes_binding_before_lock);
    RUN_TEST(test_lifecycle_payload_bounds_preparation_and_offset_retain_original_error_context);
    RUN_TEST(test_peer_challenge_and_proof_canonical_bytes_and_decoder_destinations);
    RUN_TEST(test_peer_fixed_frame_total_must_match_and_preserve_decoder_target);
    RUN_TEST(test_peer_challenge_malformed_fields_preserve_decoder_target);
    RUN_TEST(test_peer_proof_malformed_header_sequence_signature_and_extent_preserve);
    RUN_TEST(test_peer_one_challenge_per_original_connection_and_unchanged_retry);
    RUN_TEST(test_peer_absolute_challenge_deadline_including_verification_and_wrap);
    RUN_TEST(test_peer_deadline_crossed_inside_actual_crypto_port_stays_consumed);
    RUN_TEST(test_peer_conflicting_duplicate_closes_proof_without_second_crypto);
    RUN_TEST(test_peer_old_and_wrong_origins_do_not_consume_current_attempt);
    RUN_TEST(test_peer_reconnect_same_handle_requires_new_sequence_and_original);
    RUN_TEST(test_peer_invalid_context_credentials_and_entropy_fail_closed);
    RUN_TEST(test_peer_key_revocation_before_and_after_crypto_never_publishes);
    RUN_TEST(test_peer_source_closure_before_and_after_crypto_never_publishes);
    RUN_TEST(test_peer_signature_attempt_consumed_before_reentry_and_without_retry);
    RUN_TEST(test_peer_issue_reentry_and_frozen_inputs_before_clock_observation);
    RUN_TEST(test_peer_duplicate_does_not_refresh_age_or_renew_key_proof);
    RUN_TEST(test_peer_counter_exhaustion_never_wraps_and_wrong_sequence_preserves);
    RUN_TEST(test_peer_proof_cannot_fabricate_transport_security_or_game_master);
    RUN_TEST(test_peer_receiver_six_actual_twenty_byte_frames_and_full_duplicate);
    RUN_TEST(test_peer_receiver_bad_order_message_overlap_conflict_and_header);
    RUN_TEST(test_peer_receiver_original_age_and_snapshot_freeze_before_observation);
    RUN_TEST(test_peer_queue_four_fixed_copies_zero_wait_and_no_callback_crypto);
    RUN_TEST(test_peer_queue_freezes_bytes_origin_and_first_timestamp);
    RUN_TEST(test_peer_queue_old_connection_ready_loss_and_expiry_never_rebind);
    RUN_TEST(test_peer_queue_bad_write_and_closed_port_preserve_destination);
    RUN_TEST(test_peer_queue_receiver_reentry_cannot_take_another_item);
#if defined(LARP_REAL_PEER_CRYPTO)
    RUN_TEST(test_peer_actual_mbedtls_p256_independent_signature_and_bad_public_inputs);
    RUN_TEST(test_peer_actual_crypto_owner_frames_explicit_role_and_application);
#endif
    RUN_TEST(test_peer_registry_canonical_record_crc_sorted_unique_and_target_preservation);
    RUN_TEST(test_peer_registry_two_peers_duplicate_keys_principals_labels_and_order);
    RUN_TEST(test_peer_registry_missing_then_key_only_and_explicit_role_persist_without_opening);
    RUN_TEST(test_peer_registry_role_change_closes_original_proof_and_keeps_running_game);
    RUN_TEST(test_peer_registry_rotation_generation_and_old_credential_revocation);
    RUN_TEST(test_peer_registry_stale_duplicate_invalid_edits_do_not_write_or_close_live_link);
    RUN_TEST(test_peer_registry_restore_corruption_device_and_read_failure_never_restore_roles);
    RUN_TEST(test_peer_registry_uncertain_set_commit_readback_and_shared_same_boot_fence);
    RUN_TEST(test_peer_registry_compare_entire_record_conflict_never_overwrites_other_owner);
    RUN_TEST(test_peer_registry_record_freeze_busy_reentry_and_closed_roles_before_nvs_write);
    RUN_TEST(test_peer_registry_init_busy_invalid_direct_write_and_fenced_read_preserve_targets);
    RUN_TEST(test_peer_registry_counter_exhaustion_and_repeat_begin_cannot_reset_fence);
    RUN_TEST(test_peer_registry_closed_assignment_replacement_preserves_new_authority);
    RUN_TEST(test_peer_registry_revoke_during_proof_crypto_cannot_publish_old_verified_peer);
#if defined(LARP_REAL_PEER_CRYPTO)
    RUN_TEST(test_peer_registry_actual_curve_restore_and_invalid_point_rejection);
    RUN_TEST(test_peer_registry_actual_crypto_restored_explicit_role_frames_game_and_revocation);
#endif
    RUN_TEST(test_peer_registry_owner_separate_readback_failure_and_confirmed_revoke_corruption);
    RUN_TEST(test_registry_actor_pending_ack_precedes_io_and_snapshot_publication);
    RUN_TEST(test_registry_actor_snapshot_is_readable_on_crypto_worker_and_survives_fresh_links);
    RUN_TEST(test_registry_actor_role_change_closes_link_before_nvs_and_retains_game);
    RUN_TEST(test_registry_actor_lost_close_ack_expires_without_any_nvs_or_retry);
    RUN_TEST(test_registry_actor_new_connection_and_foreign_authority_preserved_by_old_job);
    RUN_TEST(test_registry_actor_install_wrong_sdk_task_never_publishes_stored_role);
    RUN_TEST(test_registry_actor_nvs_uncertain_after_close_has_no_install_or_key_publication);
    RUN_TEST(test_registry_actor_sdk_token_replay_cannot_consume_reused_slot);
    RUN_TEST(test_registry_actor_pending_inputs_and_conflicting_retries_keep_frozen_snapshot);
    RUN_TEST(test_registry_actor_role_slot_duplicate_full_and_wrong_worker_preserve_pending);
    RUN_TEST(test_registry_actor_public_snapshot_freezes_peer_before_mutex_observation);
    RUN_TEST(test_registry_actor_expired_install_and_overall_budget_never_refresh);
    RUN_TEST(test_registry_actor_wrong_sdk_instance_and_token_exhaustion_fail_without_repost);
    RUN_TEST(test_registry_actor_closed_proof_copy_is_original_and_closes_before_role_ports);
    RUN_TEST(test_registry_actor_foreign_authority_before_request_is_never_retired);
    RUN_TEST(test_registry_actor_copied_policy_remains_original_after_verification_and_revoke);
    RUN_TEST(test_registry_actor_owner_observer_is_single_and_failed_ack_preserves_output);
#if defined(LARP_REAL_PEER_CRYPTO)
    RUN_TEST(test_registry_actor_actual_crypto_original_sdk_roles_frames_game_and_revoke);
#endif
    RUN_TEST(test_registry_actor_fixed_job_rejects_invalid_origin_roles_and_order);
    RUN_TEST(test_security_core_requires_original_start_and_all_resolved_mode_bits);
    RUN_TEST(test_security_core_rejects_cancel_failure_br_and_weak_link_state);
    RUN_TEST(test_security_core_deadline_wrap_duplicate_start_does_not_refresh);
    RUN_TEST(test_security_core_abandoned_attempt_cannot_attach_to_recycled_connection);
    RUN_TEST(test_security_core_repair_invalidates_previous_mode_until_terminal_drain);
    RUN_TEST(test_security_source_original_callbacks_forward_without_roles_and_reader_needs_start);
    RUN_TEST(test_security_source_sdk_metadata_rejections_preserve_destination);
    RUN_TEST(test_security_source_wrong_actor_sdk_instance_and_forwarding_cannot_read);
    RUN_TEST(test_security_source_sdk_port_input_mutation_stops_before_next_query);
    RUN_TEST(test_security_source_borrowed_completion_frozen_before_clock_port);
    RUN_TEST(test_security_source_reconnect_during_sdk_read_preserves_fresh_binding);
    RUN_TEST(test_security_source_disconnect_pending_drains_old_completion_before_new_pairing);
    RUN_TEST(test_security_source_failure_and_repair_revoke_exact_owned_gm_without_stopping_game);
    RUN_TEST(test_security_source_security_failure_preserves_foreign_publisher);
    RUN_TEST(test_security_source_legacy_cancel_timeout_and_restored_bond_cannot_prove_sc);
    RUN_TEST(test_security_source_stale_proof_binding_never_queries_sdk);
    RUN_TEST(test_security_source_sdk_registration_failure_is_closed_without_retry);
#if defined(LARP_REAL_PEER_CRYPTO)
    RUN_TEST(test_security_actual_crypto_original_smp_metadata_frames_game_and_revoke);
    RUN_TEST(test_grant_actual_crypto_worker_original_sdk_ack_frames_game_and_revoke);
#endif
    RUN_TEST(test_grant_actor_apply_precedes_worker_confirmation_and_completed_age_does_not_revoke);
    RUN_TEST(test_grant_wrong_boot_and_unverified_owner_cannot_post_or_change_target);
    RUN_TEST(test_grant_cancel_before_actor_closes_exact_original_and_reconnect_recovers);
    RUN_TEST(test_grant_lost_ack_timeout_retires_applied_rights_and_preserves_running_game);
    RUN_TEST(test_grant_queue_and_proof_deadlines_expire_without_refresh_across_wrap);
    RUN_TEST(test_grant_new_connection_and_foreign_publisher_survive_old_cancel);
    RUN_TEST(test_grant_worker_binding_wrong_actor_sdk_and_full_slot_are_closed);
    RUN_TEST(test_grant_expiry_during_sdk_read_is_rechecked_before_authentication);
    RUN_TEST(test_grant_credential_revocation_during_metadata_never_authenticates);
    RUN_TEST(test_grant_immutable_context_wrong_boot_scope_and_record_fail_closed);
    RUN_TEST(test_grant_input_frozen_before_external_clock_and_observation_ports);
    RUN_TEST(test_identity_original_auth_conflict_preserves_foreign_publisher);
    RUN_TEST(test_grant_deadline_crossed_after_clock_sample_never_confirms_lost_ack);
    RUN_TEST(test_grant_rejected_sdk_post_is_consumed_and_fresh_reconnect_can_recover);
    RUN_TEST(test_grant_opaque_counter_exhaustion_never_posts_or_opens_rights);
    RUN_TEST(test_grant_key_possession_without_stored_gm_assignment_never_opens_game_control);
    RUN_TEST(test_grant_cancel_during_metadata_never_authenticates_or_disturbs_closed_publisher);
    RUN_TEST(test_grant_foreign_publisher_after_applied_ack_survives_worker_rejection);
    RUN_TEST(test_grant_close_ack_requires_exact_owned_connection_authority_and_closed_publisher);
    RUN_TEST(test_grant_applied_cancel_and_queued_role_revoke_share_exact_own_close_ack);
    RUN_TEST(test_grant_publisher_freezes_expected_before_lock_and_preserves_replacement);
    RUN_TEST(test_grant_poll_during_actor_publication_remains_pending_without_mutable_owner_read);
    RUN_TEST(test_challenge_read_original_blob_values_keep_one_nonce_and_no_gm);
    RUN_TEST(test_challenge_read_missing_challenge_and_bad_offset_have_bounded_att_errors);
    RUN_TEST(
        test_challenge_read_wrong_worker_no_response_unknown_reads_forward_and_no_rsp_is_dropped);
    RUN_TEST(test_challenge_read_queue_is_four_static_entries_one_response_per_service);
    RUN_TEST(test_challenge_read_queue_deadline_wrap_never_refreshes_and_stale_drain_is_bounded);
    RUN_TEST(test_challenge_read_post_deadline_and_absolute_issue_deadline_drop_sdk_output);
    RUN_TEST(test_challenge_read_reconnect_and_old_opaque_callback_cannot_consume_new_slot);
    RUN_TEST(test_challenge_read_sdk_metadata_loss_never_emits_positive_data);
    RUN_TEST(test_challenge_read_registry_revoke_while_pending_drops_original_answer);
    RUN_TEST(test_challenge_read_post_failure_and_busy_slot_have_no_retry_or_duplicate_posts);
    RUN_TEST(test_challenge_read_frozen_reply_and_invalid_scope_never_changes_original);
    RUN_TEST(test_challenge_read_request_frozen_before_external_clock_and_late_security_recheck);
    RUN_TEST(test_challenge_read_service_installs_original_read_attribute_before_start);
    RUN_TEST(test_challenge_read_nonce_snapshot_failure_preserves_destination_and_deadline);
    RUN_TEST(test_challenge_read_exhausted_callback_counter_and_foreign_caller_cannot_post);
    RUN_TEST(test_challenge_read_revoke_or_deadline_during_sdk_metadata_never_sends);
    RUN_TEST(test_challenge_read_borrowed_clock_closure_cannot_adopt_recycled_link);
    RUN_TEST(test_challenge_read_invalid_installation_bundle_and_order_fail_before_sdk_start);
    RUN_TEST(test_challenge_read_requires_the_same_existing_crypto_worker_before_any_owner_getter);
#if defined(LARP_REAL_PEER_CRYPTO)
    RUN_TEST(test_challenge_read_actual_crypto_from_original_sdk_blob_snapshot_to_grant_ack);
#endif
    RUN_TEST(test_issue_codec_exact_selector_no_credentials_and_failed_destination_preserved);
    RUN_TEST(test_issue_actual_sdk_copy_worker_registry_and_read_without_gm);
    RUN_TEST(test_issue_invalid_write_modes_sizes_context_and_missing_peer_never_call_crypto);
    RUN_TEST(test_issue_four_static_copies_one_current_operation_and_no_duplicate_nonce_refresh);
    RUN_TEST(test_issue_wrong_worker_and_wrong_sdk_actor_cannot_consume_or_rebind);
    RUN_TEST(test_issue_expiry_wrap_and_reconnect_drop_old_work_without_adopting_fresh_link);
    RUN_TEST(test_issue_sdk_borrow_and_original_scope_frozen_before_external_clock);
    RUN_TEST(test_issue_registry_revoke_entropy_failure_and_expired_challenge_do_not_retry);
    RUN_TEST(test_issue_optional_service_installs_nine_original_handles_before_start);
    RUN_TEST(test_issue_setup_rejects_incomplete_bundle_wrong_handle_order_and_late_configuration);
#if defined(LARP_REAL_PEER_CRYPTO)
    RUN_TEST(test_issue_actual_crypto_original_sdk_write_to_read_and_existing_proof_grant);
#endif
    RUN_TEST(test_auth_codec_exact_states_correlations_and_failed_destination);
    RUN_TEST(test_auth_sdk_fragments_pending_ack_and_correlated_granted_result);
    RUN_TEST(test_auth_result_blob_pins_receiving_snapshot_across_grant_and_new_read_gets_latest);
    RUN_TEST(test_auth_result_missing_bad_offsets_pin_expiry_and_absolute_challenge_deadline);
    RUN_TEST(test_auth_signature_rejection_is_visible_without_grant_or_nonce_retry);
    RUN_TEST(test_auth_invalid_write_modes_frames_foreign_task_and_first_bad_worker_binding);
    RUN_TEST(test_auth_four_proof_copies_and_reads_full_and_one_item_per_service);
    RUN_TEST(test_auth_proof_and_read_age_wrap_and_reconnect_never_rebind_old_work);
    RUN_TEST(test_auth_sdk_borrows_frozen_before_clock_and_clock_reconnect_drops_original);
    RUN_TEST(test_auth_post_failure_busy_slot_and_old_callback_cannot_send_new_reply);
    RUN_TEST(test_auth_positive_reply_rechecks_sdk_security_credential_and_original_connection);
    RUN_TEST(test_auth_duplicate_terminal_proof_never_reposts_grant_and_expiry_does_not_revoke);
    RUN_TEST(test_auth_authority_retirement_at_every_reply_clock_observation_drops_old_success);
    RUN_TEST(test_auth_non_gm_ack_has_authenticated_result_without_principal);
    RUN_TEST(test_auth_forged_positive_job_ack_principal_and_scope_never_post);
    RUN_TEST(test_auth_service_thirteen_handles_and_complete_source_ack_before_start);
    RUN_TEST(test_auth_setup_wrong_handles_incomplete_bundle_and_late_configuration_fail_closed);
#if defined(LARP_REAL_PEER_CRYPTO)
    RUN_TEST(test_auth_actual_crypto_sdk_issue_reads_proof_writes_ack_and_result_blobs);
#endif
    return UNITY_END();
}
