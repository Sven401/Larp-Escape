#pragma once
using larp::integration::ProfileTransferControl;
ProfileControlFrame controlCommand(ProfileOperation operation, std::uint32_t request = 1,
                                   std::uint16_t transfer = 1) {
    ProfileControlFrame value;
    value.transfer = transfer;
    value.request = request;
    value.operation = operation;
    return value;
}
std::string controlBytes(const ProfileControlFrame &command) {
    char bytes[12];
    std::size_t written = 0;
    TEST_ASSERT_TRUE(encodeProfileControl(command, bytes, sizeof(bytes), written));
    return std::string(bytes, written);
}
void responseCode(ProfileResponseCode code, const RoutedProfileResponse &value) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(code), static_cast<unsigned>(value.response.code));
    TEST_ASSERT_TRUE(value.response.valid());
    TEST_ASSERT_EQUAL_UINT32(7, value.original.principal);
    TEST_ASSERT_EQUAL_UINT32(1, value.original.epoch);
}
struct ControlFixture {
    TemporaryProfileFile file;
    ImportClock clock;
    ProfileStaging source{file, f->channel, clock};
    ProfileTransferControl control{f->application, f->channel, source, clock};
    FreeRtosProfileFrameQueue queue{f->channel.actual};
    ProfileFramePump pump{queue, source, clock, &control};
    ControlFixture() {
        TEST_ASSERT_TRUE(queue.begin());
    }
    void receive(const std::string &bytes, std::size_t maximum = 128) {
        copyProfileValue(queue, queuedProfileEnvelope(bytes), clock.now);
        TEST_ASSERT_TRUE(pump.service().received);
        for (const auto &value : profileFrames(bytes, maximum)) {
            copyProfileValue(queue, value, clock.now);
            TEST_ASSERT_TRUE(pump.service().received);
        }
    }
    void command(ProfileOperation operation, std::uint32_t request = 1, std::uint16_t transfer = 1,
                 std::uint32_t receivedAt = 0) {
        copyProfileValue(queue, controlBytes(controlCommand(operation, request, transfer)),
                         receivedAt);
        TEST_ASSERT_TRUE(pump.service().control);
    }
    RoutedProfileResponse take(ProfileResponseCode code) {
        RoutedProfileResponse value;
        TEST_ASSERT_TRUE(control.takeResponse(value));
        responseCode(code, value);
        return value;
    }
    void imported() {
        receive(importExample());
        command(ProfileOperation::Finish);
        take(ProfileResponseCode::DraftReady);
    }
};
void commandFixture(const char *name, const ProfileControlFrame &command) {
    static const char hex[] = "0123456789ABCDEF";
    std::string text;
    for (const auto byte : controlBytes(command)) {
        const auto value = static_cast<unsigned char>(byte);
        text += hex[value >> 4];
        text += hex[value & 15];
    }
    profileSourceFixture(name, text + char(10));
}
void responseFixture(const char *name, const RoutedProfileResponse &value) {
    char bytes[20];
    std::size_t count = 0;
    TEST_ASSERT_TRUE(encodeProfileResponse(value.response, bytes, sizeof(bytes), count));
    static const char hex[] = "0123456789ABCDEF";
    std::string text;
    for (std::size_t i = 0; i < count; ++i) {
        const auto byte = static_cast<unsigned char>(bytes[i]);
        text += hex[byte >> 4];
        text += hex[byte & 15];
    }
    profileSourceFixture(name, text + char(10));
}
void test_profile_control_codec_exact_layout_limits_and_failed_destination() {
    const auto value = controlCommand(ProfileOperation::Apply, UINT32_MAX, 65535);
    const auto encoded = controlBytes(value);
    const unsigned char expected[] = {1, 7, 255, 255, 3, 0, 0, 0, 255, 255, 255, 255};
    TEST_ASSERT_EQUAL_MEMORY(expected, encoded.data(), 12);
    ProfileControlFrame decoded;
    TEST_ASSERT_TRUE(decodeProfileControl(encoded.data(), 12, decoded));
    TEST_ASSERT_TRUE(decoded == value);
    for (unsigned bad = 0; bad < 11; ++bad) {
        auto changed = encoded;
        if (bad == 0)
            changed.resize(11);
        if (bad == 1)
            changed += 'x';
        if (bad == 2)
            changed[0] = 2;
        if (bad == 3)
            changed[1] = 8;
        if (bad == 4)
            changed[2] = changed[3] = 0;
        if (bad == 5)
            changed[4] = 0;
        if (bad == 6)
            changed[4] = 4;
        if (bad >= 7 && bad <= 9)
            changed[bad - 2] = 1;
        if (bad == 10)
            changed[8] = changed[9] = changed[10] = changed[11] = 0;
        decoded = controlCommand(ProfileOperation::Abort, 42, 42);
        TEST_ASSERT_FALSE(decodeProfileControl(changed.data(), changed.size(), decoded));
        TEST_ASSERT_EQUAL_UINT32(42, decoded.request);
    }
    char destination[20];
    std::memset(destination, '#', 20);
    std::size_t count = 99;
    TEST_ASSERT_FALSE(encodeProfileControl(value, destination, 11, count));
    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_CHAR('#', destination[0]);
    ProfileResponseFrame response;
    response.command = controlCommand(ProfileOperation::Apply, 2);
    response.code = ProfileResponseCode::Saved;
    response.detail = ProfileResponseDetail::None;
    response.draftRevision = 3;
    response.configurationRevision = 2;
    TEST_ASSERT_TRUE(encodeProfileResponse(response, destination, 20, count));
    TEST_ASSERT_EQUAL_UINT32(20, count);
    ProfileResponseFrame out;
    TEST_ASSERT_TRUE(decodeProfileResponse(destination, count, out));
    TEST_ASSERT_EQUAL_UINT32(2, out.command.request);
    for (unsigned bad = 0; bad < 8; ++bad) {
        std::string changed(destination, 20);
        if (bad == 0)
            changed.resize(19);
        if (bad == 1)
            changed[1] = 7;
        if (bad == 2)
            changed[8] = 1;
        if (bad == 3)
            changed[9] = 0;
        if (bad == 4)
            changed[9] = 14;
        if (bad == 5)
            changed[10] = 1;
        if (bad == 6)
            changed[12] = 0;
        if (bad == 7)
            changed[16] = 0;
        out.draftRevision = 42;
        TEST_ASSERT_FALSE(decodeProfileResponse(changed.data(), changed.size(), out));
        TEST_ASSERT_EQUAL_UINT32(42, out.draftRevision);
    }
    response.command.operation = ProfileOperation::Finish;
    count = 99;
    std::memset(destination, '#', 20);
    TEST_ASSERT_FALSE(encodeProfileResponse(response, destination, 20, count));
    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_CHAR('#', destination[0]);
    TEST_ASSERT_FALSE(decodeProfileControl(nullptr, 12, decoded));
    TEST_ASSERT_FALSE(decodeProfileResponse(nullptr, 20, out));
}
void test_profile_control_actual_queue_source_finish_apply_saved_current_status() {
    ControlFixture x;
    const auto bytes = importExample();
    const auto old = f->store.record(0);
    x.receive(bytes, 20);
    TEST_ASSERT_FALSE(x.pump.service().received);
    TEST_ASSERT_FALSE(f->application.storage().draftReady);
    commandFixture("control-finish-command-v1.txt", controlCommand(ProfileOperation::Finish));
    commandFixture("control-apply-command-v1.txt", controlCommand(ProfileOperation::Apply, 2));
    commandFixture("control-abort-command-v1.txt", controlCommand(ProfileOperation::Abort, 3));
    x.command(ProfileOperation::Finish);
    const auto draft = x.take(ProfileResponseCode::DraftReady);
    responseFixture("control-draft-v1.txt", draft);
    TEST_ASSERT_EQUAL_UINT32(3, draft.response.draftRevision);
    TEST_ASSERT_EQUAL_UINT32(1, draft.response.configurationRevision);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Empty),
                      static_cast<unsigned>(x.source.phase()));
    x.command(ProfileOperation::Apply, 2);
    const auto queued = x.take(ProfileResponseCode::ApplyQueued);
    responseFixture("control-queued-v1.txt", queued);
    TEST_ASSERT_TRUE(f->application.busy());
    TEST_ASSERT_FALSE(x.control.ready());
    pumpProfileApply();
    x.control.service();
    const auto saved = x.take(ProfileResponseCode::Saved);
    responseFixture("control-saved-v1.txt", saved);
    TEST_ASSERT_EQUAL_UINT32(2, saved.response.configurationRevision);
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_EQUAL_UINT32(2, f->application.snapshot().storedGeneration);
    TEST_ASSERT_EQUAL_STRING(old.c_str(), f->store.record(0).c_str());
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_TRUE(x.control.ready());
    f->now += ControlLimits::PublishMilliseconds;
    f->control.service(f->now);
    RoutedConfiguredStatus status;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
    char encoded[maxConfiguredStatusJsonBytes];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                      static_cast<unsigned>(encodeConfiguredStatus(status.status, encoded,
                                                                   sizeof(encoded), written)));
    profileSourceFixture("control-status-v2.json", std::string(encoded, written) + char(10));
}
void test_profile_control_duplicate_finish_apply_replays_without_import_or_save() {
    ControlFixture x;
    x.imported();
    const auto revision = f->application.storage().draftRevision;
    x.command(ProfileOperation::Finish);
    x.take(ProfileResponseCode::DraftReady);
    TEST_ASSERT_EQUAL_UINT32(revision, f->application.storage().draftRevision);
    x.command(ProfileOperation::Apply, 2);
    x.take(ProfileResponseCode::ApplyQueued);
    const auto writes = f->store.writeCalls;
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply, 2)));
    TEST_ASSERT_FALSE(x.pump.service().received);
    pumpProfileApply();
    x.control.service();
    x.take(ProfileResponseCode::Saved);
    TEST_ASSERT_TRUE(x.pump.service().control);
    x.take(ProfileResponseCode::Saved);
    TEST_ASSERT_EQUAL_UINT32(writes + 1, f->store.writeCalls);
}
void test_profile_control_request_conflict_old_order_no_reexecution() {
    ControlFixture x;
    x.imported();
    const auto revision = f->application.storage().draftRevision;
    x.command(ProfileOperation::Abort, 1);
    x.take(ProfileResponseCode::RequestConflict);
    x.command(ProfileOperation::Apply, 3);
    x.take(ProfileResponseCode::OutOfOrder);
    x.command(ProfileOperation::Apply, 2, 2);
    const auto rejected = x.take(ProfileResponseCode::ApplyRejected);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseDetail::NoReceipt),
                      static_cast<unsigned>(rejected.response.detail));
    x.command(ProfileOperation::Finish, 1);
    x.take(ProfileResponseCode::RequestExpired);
    TEST_ASSERT_EQUAL_UINT32(revision, f->application.storage().draftRevision);
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_control_abort_clears_only_temporary_and_server_proof() {
    ControlFixture x;
    x.imported();
    const auto old = f->store.record(0);
    const auto revision = f->application.storage().draftRevision;
    x.command(ProfileOperation::Abort, 2);
    const auto aborted = x.take(ProfileResponseCode::Aborted);
    responseFixture("control-aborted-v1.txt", aborted);
    x.command(ProfileOperation::Apply, 3);
    x.take(ProfileResponseCode::ApplyRejected);
    TEST_ASSERT_TRUE(f->application.storage().draftReady);
    TEST_ASSERT_EQUAL_UINT32(revision, f->application.storage().draftRevision);
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
    TEST_ASSERT_EQUAL_STRING(old.c_str(), f->store.record(0).c_str());
}
void test_profile_control_incomplete_finish_and_semantic_failure_issue_no_proof() {
    ControlFixture x;
    x.receive("abc");
    x.command(ProfileOperation::Finish);
    x.take(ProfileResponseCode::ImportRejected);
    TEST_ASSERT_FALSE(f->application.storage().draftReady);
    x.command(ProfileOperation::Apply, 2);
    x.take(ProfileResponseCode::ApplyRejected);
    ControlFixture y;
    copyProfileValue(y.queue, queuedProfileEnvelope(importExample()));
    TEST_ASSERT_TRUE(y.pump.service().received);
    y.command(ProfileOperation::Finish);
    const auto value = y.take(ProfileResponseCode::ReceiveRejected);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseDetail::Incomplete),
                      static_cast<unsigned>(value.response.detail));
}
void test_profile_control_running_game_denies_finish_and_preserves_game() {
    ControlFixture x;
    x.receive(importExample());
    TEST_ASSERT_TRUE(f->application.startGame(1, f->now));
    const auto revision = f->application.storage().draftRevision;
    x.command(ProfileOperation::Finish);
    const auto reply = x.take(ProfileResponseCode::ImportRejected);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseDetail::GameRunning),
                      static_cast<unsigned>(reply.response.detail));
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(revision, f->application.storage().draftRevision);
}
void test_profile_control_stale_draft_denies_apply() {
    ControlFixture x;
    x.imported();
    TEST_ASSERT_TRUE(
        f->application.createDraft(FactoryProfile::Hint, f->application.storage().draftRevision));
    x.command(ProfileOperation::Apply, 2);
    const auto reply = x.take(ProfileResponseCode::ApplyRejected);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseDetail::StaleRevision),
                      static_cast<unsigned>(reply.response.detail));
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_control_late_apply_has_fresh_rights_without_source_deadline_reset() {
    ControlFixture x;
    x.imported();
    x.clock.now = ProfileSourceLimits::DeadlineMilliseconds + 1;
    x.command(ProfileOperation::Apply, 2, 1, x.clock.now);
    x.take(ProfileResponseCode::ApplyQueued);
    TEST_ASSERT_TRUE(f->application.busy());
    TEST_ASSERT_EQUAL_UINT32(0, x.source.receivedAt());
}
void test_profile_control_own_queue_age_future_wrap_and_response_backpressure() {
    ControlFixture x;
    x.imported();
    x.clock.now = 10000;
    x.command(ProfileOperation::Apply, 2);
    x.take(ProfileResponseCode::Expired);
    TEST_ASSERT_FALSE(f->application.busy());
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply, 2)), 10001);
    const auto future = x.pump.service();
    TEST_ASSERT_FALSE(future.control);
    receiveOutcome(ProfileReceiveOutcome::Expired, future.outcome);
    x.clock.now = 5;
    x.command(ProfileOperation::Apply, 2, 1, UINT32_MAX - 2);
    x.take(ProfileResponseCode::ApplyQueued);
}
void test_profile_control_pending_response_polls_source_deadline_and_blocks_take() {
    ControlFixture x;
    x.receive("abc");
    x.command(ProfileOperation::Apply, 1); // no finish/proof
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Abort, 2)));
    TEST_ASSERT_FALSE(x.pump.service().received);
    x.clock.now = ProfileSourceLimits::DeadlineMilliseconds;
    // Source deadline still serviced when reply/input is blocked; reply expires.
    const auto dropped = x.pump.service();
    TEST_ASSERT_TRUE(dropped.received);
    TEST_ASSERT_FALSE(dropped.control);
    receiveOutcome(ProfileReceiveOutcome::Expired, dropped.outcome);
    RoutedProfileResponse reply;
    TEST_ASSERT_FALSE(x.control.takeResponse(reply));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                      static_cast<unsigned>(x.source.phase()));
}
void test_profile_control_revoke_reconnect_never_relabels_old_proof_or_response() {
    ControlFixture x;
    x.imported();
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply, 2)));
    f->channel.actual.suspendAuthority();
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_FALSE(f->application.busy());
    RoutedProfileResponse out;
    out.response.draftRevision = 99;
    TEST_ASSERT_FALSE(x.control.takeResponse(out));
    TEST_ASSERT_EQUAL_UINT32(99, out.response.draftRevision);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(3))));
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply)), 0, 3);
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_TRUE(x.control.takeResponse(out));
    TEST_ASSERT_EQUAL_UINT32(3, out.original.epoch);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseCode::ApplyRejected),
                      static_cast<unsigned>(out.response.code));
}
void test_profile_control_final_rights_observation_blocks_apply_and_response() {
    ControlFixture x;
    x.imported();
    const auto command = controlCommand(ProfileOperation::Apply, 2);
    f->channel.revokeBefore = f->channel.reads + 4; // owner2 + apply2 observations
    x.control.handle(command, authority(1), 0);
    RoutedProfileResponse out;
    TEST_ASSERT_FALSE(x.control.takeResponse(out));
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_control_later_revoke_finishes_selected_transaction_silently() {
    ControlFixture x;
    x.imported();
    x.command(ProfileOperation::Apply, 2);
    x.take(ProfileResponseCode::ApplyQueued);
    f->channel.actual.suspendAuthority();
    pumpProfileApply();
    x.control.service();
    RoutedProfileResponse out;
    TEST_ASSERT_FALSE(x.control.takeResponse(out));
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_EQUAL_UINT32(2, f->application.activation().configurationRevision);
}
void test_profile_control_uncertain_journal_produces_failed_never_saved() {
    ControlFixture x;
    x.imported();
    const auto old = f->store.record(0);
    f->store.uncertainFinish = true;
    x.command(ProfileOperation::Apply, 2);
    x.take(ProfileResponseCode::ApplyQueued);
    pumpProfileApply();
    x.control.service();
    const auto failed = x.take(ProfileResponseCode::ApplyFailed);
    responseFixture("control-failed-v1.txt", failed);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ApplicationPhase::Fault),
                      static_cast<unsigned>(f->application.snapshot().phase));
    TEST_ASSERT_FALSE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_EQUAL_STRING(old.c_str(), f->store.record(0).c_str());
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
void test_profile_control_abort_after_selected_apply_waits_preserves_transaction() {
    ControlFixture x;
    x.imported();
    x.command(ProfileOperation::Apply, 2);
    x.take(ProfileResponseCode::ApplyQueued);
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Abort, 3)));
    TEST_ASSERT_FALSE(x.pump.service().received);
    pumpProfileApply();
    x.control.service();
    x.take(ProfileResponseCode::Saved);
    TEST_ASSERT_TRUE(x.pump.service().control);
    x.take(ProfileResponseCode::Aborted);
    TEST_ASSERT_EQUAL_UINT32(2, f->application.activation().configurationRevision);
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
}
void test_profile_response_sender_original_copy_busy_priority_expiry() {
    ControlFixture x;
    x.imported();
    x.command(ProfileOperation::Apply, 2);
    auto response = x.take(ProfileResponseCode::ApplyQueued);
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(sender.load(response, 0)));
    response.response.command.request = 99;
    bearer.busy = true;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Deferred),
                      static_cast<unsigned>(sender.service(1, true)));
    TEST_ASSERT_EQUAL_UINT32(0, bearer.calls);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Busy),
                      static_cast<unsigned>(sender.service(1)));
    const auto old = bearer.attempt;
    bearer.busy = false;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(2)));
    TEST_ASSERT_EQUAL_MEMORY(old.bytes.data(), bearer.last.bytes.data(), 20);
    ProfileResponseFrame decoded;
    TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), 20, decoded));
    TEST_ASSERT_EQUAL_UINT32(2, decoded.command.request);
    response.response.command.request = 2;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Expired),
                      static_cast<unsigned>(sender.load(response, 10000)));
    TEST_ASSERT_FALSE(sender.pending());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(sender.load(response, 0)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Expired),
                      static_cast<unsigned>(sender.service(10000, true)));
    TEST_ASSERT_FALSE(sender.pending());
}
void test_profile_response_sender_bearer_recheck_and_old_generation() {
    ControlFixture x;
    x.receive("abc");
    x.command(ProfileOperation::Abort);
    auto response = x.take(ProfileResponseCode::Aborted);
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(sender.load(response, 0)));
    bearer.revokeBefore = true;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Unavailable),
                      static_cast<unsigned>(sender.service(1)));
    TEST_ASSERT_TRUE(bearer.frames.empty());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(AuthorityUpdate::Published),
                      static_cast<unsigned>(f->channel.actual.updateAuthority(authority(3))));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Unauthorized),
                      static_cast<unsigned>(sender.load(response, 1)));
    TEST_ASSERT_FALSE(sender.pending());
}
void test_profile_response_scheduler_completing_game_reply_owns_entire_step() {
    ControlFixture x;
    x.imported();
    x.command(ProfileOperation::Abort, 2);
    const auto response = x.take(ProfileResponseCode::Aborted);
    const auto game = executeWithCurrentEpoch(GameCommandKind::Start);
    RoutedConfiguredStatus current;
    TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(current));
    TestReplyBearer bearer;
    ReplyFrameSender gameSender(f->channel, bearer);
    ProfileResponseSender profile(f->channel, bearer);
    StatusFrameSender status(f->channel, bearer);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReplySendOutcome::Loaded),
                      static_cast<unsigned>(gameSender.load(game, f->now)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(profile.load(response, f->now)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(StatusSendOutcome::Loaded),
                      static_cast<unsigned>(status.load(current, f->now)));
    while (gameSender.pending()) {
        const auto calls = bearer.calls;
        const auto step = serviceProfileConfiguredFrames(gameSender, profile, status, f->now, 128);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Deferred),
                          static_cast<unsigned>(step.profile));
        TEST_ASSERT_EQUAL_UINT32(calls + 1, bearer.calls);
    }
    const auto calls = bearer.calls;
    const auto step = serviceProfileConfiguredFrames(gameSender, profile, status, f->now, 128);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(step.profile));
    TEST_ASSERT_EQUAL_UINT32(calls + 1, bearer.calls);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(StatusSendOutcome::Deferred),
                      static_cast<unsigned>(step.status));
    TEST_ASSERT_TRUE(status.pending());
}

void test_profile_control_queue_admits_exact12_rejects_malformed_and_full_retry() {
    FreeRtosProfileFrameQueue queue(f->channel.actual);
    TEST_ASSERT_TRUE(queue.begin());
    const auto value = controlBytes(controlCommand(ProfileOperation::Abort));
    for (unsigned bad = 0; bad < 7; ++bad) {
        auto changed = value;
        if (bad == 0)
            changed.resize(11);
        if (bad == 1)
            changed += 'x';
        if (bad == 2)
            changed[4] = 4;
        if (bad == 3)
            changed[5] = 1;
        if (bad == 4)
            changed[8] = 0;
        if (bad == 5)
            changed[1] = 8;
        if (bad == 6)
            changed[0] = 2;
        profileCopyOutcome(bad >= 5 ? ProfileCopyOutcome::UnsupportedFrame
                                    : ProfileCopyOutcome::InvalidWrite,
                           queue.copyWrite(changed.data(), changed.size(), 7, 1, 17));
    }
    for (unsigned i = 0; i < 4; ++i)
        copyProfileValue(queue, value, i);
    profileCopyOutcome(ProfileCopyOutcome::QueueFull,
                       queue.copyWrite(value.data(), value.size(), 7, 1, 4));
    CopiedProfileFrame out;
    TEST_ASSERT_TRUE(queue.take(out));
    copyProfileValue(queue, value, 4);
    for (unsigned i = 1; i < 5; ++i) {
        TEST_ASSERT_TRUE(queue.take(out));
        TEST_ASSERT_EQUAL_UINT32(i, out.receivedAt);
        TEST_ASSERT_EQUAL_MEMORY(value.data(), out.bytes.data(), 12);
    }
    TEST_ASSERT_FALSE(queue.take(out));
}
void test_profile_control_apply_while_running_is_rejected_without_game_change() {
    ControlFixture x;
    x.imported();
    TEST_ASSERT_TRUE(f->application.startGame(1, f->now));
    const auto revision = f->application.execution().stateRevision;
    x.command(ProfileOperation::Apply, 2);
    const auto response = x.take(ProfileResponseCode::ApplyRejected);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseDetail::GameRunning),
                      static_cast<unsigned>(response.response.detail));
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(revision, f->application.execution().stateRevision);
    TEST_ASSERT_FALSE(f->application.busy());
}
void test_profile_control_reply_pending_revoke_and_same_source_deadline() {
    ControlFixture x;
    x.receive("abc");
    x.command(ProfileOperation::Apply); // pending NoReceipt response
    f->channel.actual.suspendAuthority();
    TEST_ASSERT_FALSE(x.pump.service().received);
    TEST_ASSERT_TRUE(x.control.ready());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                      static_cast<unsigned>(x.source.phase()));
    RoutedProfileResponse out;
    TEST_ASSERT_FALSE(x.control.takeResponse(out));
    TEST_ASSERT_FALSE(f->application.busy());
}
ProfileTransferControl *nestedControl = nullptr;
ProfileControlFrame *mutableControlCommand = nullptr;
void test_profile_control_freezes_command_and_reentrant_call_has_no_effect() {
    ControlFixture x;
    x.receive(importExample());
    auto command = controlCommand(ProfileOperation::Finish);
    nestedControl = &x.control;
    mutableControlCommand = &command;
    host_rtos::beforeEnter = [] {
        *mutableControlCommand = controlCommand(ProfileOperation::Abort, 42, 42);
        nestedControl->handle(*mutableControlCommand, authority(1), 0);
        nestedControl->service();
        RoutedProfileResponse out;
        TEST_ASSERT_FALSE(nestedControl->takeResponse(out));
    };
    x.control.handle(command, authority(1), 0);
    nestedControl = nullptr;
    mutableControlCommand = nullptr;
    const auto reply = x.take(ProfileResponseCode::DraftReady);
    TEST_ASSERT_EQUAL_UINT32(1, reply.response.command.request);
    TEST_ASSERT_EQUAL_UINT32(42, command.request);
    TEST_ASSERT_EQUAL_UINT32(3, f->application.storage().draftRevision);
}
void test_profile_control_new_metadata_invalidates_previous_private_proof() {
    ControlFixture x;
    x.imported();
    x.receive(importExample()); // fresh expected draft rev3, no Finish yet
    x.command(ProfileOperation::Apply, 2);
    x.take(ProfileResponseCode::ApplyRejected);
    TEST_ASSERT_FALSE(f->application.busy());
    x.command(ProfileOperation::Finish, 3);
    const auto imported = x.take(ProfileResponseCode::DraftReady);
    TEST_ASSERT_EQUAL_UINT32(4, imported.response.draftRevision);
    x.command(ProfileOperation::Apply, 4);
    x.take(ProfileResponseCode::ApplyQueued);
    TEST_ASSERT_TRUE(f->application.busy());
}
void test_profile_response_sender_selected_bytes_survive_caller_mutation() {
    ControlFixture x;
    x.receive("abc");
    x.command(ProfileOperation::Abort);
    auto response = x.take(ProfileResponseCode::Aborted);
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    static RoutedProfileResponse *caller;
    caller = &response;
    host_rtos::beforeEnter = [] {
        caller->response.command.request = 42;
        caller->original = authority(3);
    };
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                      static_cast<unsigned>(sender.load(response, 0)));
    caller = nullptr;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                      static_cast<unsigned>(sender.service(1)));
    ProfileResponseFrame decoded;
    TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), 20, decoded));
    TEST_ASSERT_EQUAL_UINT32(1, decoded.command.request);
    TEST_ASSERT_EQUAL_UINT32(1, bearer.last.epoch);
}
