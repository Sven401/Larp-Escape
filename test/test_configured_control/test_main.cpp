#include "CompanionIntegration/GameControl.h"
#include "FakeOwners.h"
#include "FreeRtosGameControlChannel.h"
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unity.h>
// Compile the actual adapter against the deterministic RTOS model.
#include "../../lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.cpp"
namespace host_rtos {
void (*beforeEnter)() = nullptr;
unsigned depth = 0;
bool nonzeroWait = false;
} // namespace host_rtos
namespace {
using namespace larp::companion;
class MemoryStore : public IJournalStore {
  public:
    JournalOpen openRead(std::uint8_t slot) override {
        if (open_ || slot > 1)
            return JournalOpen::Failure;
        if (!present_[slot])
            return JournalOpen::Missing;
        slot_ = slot;
        position_ = 0;
        open_ = true;
        writing_ = false;
        return JournalOpen::Present;
    }
    bool closeRead() override {
        bool okay = open_ && !writing_;
        open_ = false;
        return okay;
    }
    ReadStatus readByte(std::uint8_t &byte) override {
        if (!open_ || writing_)
            return ReadStatus::Failure;
        if (position_ == records_[slot_].size())
            return ReadStatus::End;
        byte = static_cast<std::uint8_t>(records_[slot_][position_++]);
        return ReadStatus::Byte;
    }
    bool beginWrite(std::uint8_t slot) override {
        if (open_ || slot > 1)
            return false;
        slot_ = slot;
        records_[slot].clear();
        present_[slot] = true;
        open_ = writing_ = true;
        return true;
    }
    bool write(const char *bytes, std::size_t count) override {
        if (!open_ || !writing_)
            return false;
        records_[slot_].append(bytes, count);
        return true;
    }
    JournalCommit finishWrite() override {
        const bool okay = open_ && writing_;
        open_ = writing_ = false;
        return okay ? JournalCommit::Confirmed : JournalCommit::Uncertain;
    }
    void abortWrite() override {
        open_ = writing_ = false;
    }

  private:
    std::string records_[2];
    bool present_[2] = {};
    std::size_t position_ = 0;
    std::uint8_t slot_ = 0;
    bool open_ = false, writing_ = false;
};
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
    TEST_ASSERT_EQUAL_UINT32(count + 1, read);
    TEST_ASSERT_EQUAL_CHAR('\n', expected[count]);
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
    return UNITY_END();
}
