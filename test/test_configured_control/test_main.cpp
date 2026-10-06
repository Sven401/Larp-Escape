#include "Companion/CommandStream.h"
#include "Companion/ProfileCapabilities.h"
#include "CompanionIntegration/GameControl.h"
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
// Compile the actual adapter against the deterministic RTOS model.
#include "../../lib/FreeRtosCommandFrameQueue/FreeRtosCommandFrameQueue.cpp"
#include "../../lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.cpp"
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
    bool publish(const ConfiguredStatus &value) noexcept override {
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
    ConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readStatus(status));
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
    return UNITY_END();
}
