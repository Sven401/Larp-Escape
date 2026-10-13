#pragma once
using larp::integration::ApplicationProfileStaging;
void startFenced() {
    TEST_ASSERT_TRUE(f->application.profileFileBlocked());
    TEST_ASSERT_FALSE(f->application.gameRunning());
    const auto revision = f->application.activation().configurationRevision;
    const auto state = f->application.execution().stateRevision;
    const auto result = f->application.startGame(revision, ++f->now);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(larp::execution::Outcome::Busy),
                      static_cast<unsigned>(result.outcome));
    TEST_ASSERT_EQUAL_UINT32(state, f->application.execution().stateRevision);
}
void test_application_file_owner_spans_receive_seal_read_and_cleanup() {
    StageStore backend;
    ApplicationProfileStaging file(f->application, backend);
    TEST_ASSERT_TRUE(file.begin(2));
    startFenced();
    TEST_ASSERT_FALSE(f->application.busy()); // Import admission still works.
    TEST_ASSERT_TRUE(file.append("ab", 2));
    startFenced();
    TEST_ASSERT_TRUE(file.seal());
    std::uint8_t byte = 99;
    fileRead(ReadStatus::Byte, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT8('a', byte);
    fileRead(ReadStatus::Byte, file.readByte(byte));
    byte = 99;
    fileRead(ReadStatus::End, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT8(99, byte);
    startFenced();
    TEST_ASSERT_TRUE(file.discard());
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    TEST_ASSERT_TRUE(f->application.startGame(1, ++f->now));
}
void test_application_file_running_refuses_all_backend_io_preserves_game() {
    StageStore backend;
    ApplicationProfileStaging file(f->application, backend);
    TEST_ASSERT_TRUE(f->application.startGame(1, ++f->now));
    const auto state = f->application.execution().stateRevision;
    const auto old = f->store.record(0);
    std::uint8_t byte = 99;
    TEST_ASSERT_FALSE(file.begin(1));
    TEST_ASSERT_FALSE(file.append("x", 1));
    TEST_ASSERT_FALSE(file.seal());
    TEST_ASSERT_FALSE(file.discard());
    fileRead(ReadStatus::Failure, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT8(99, byte);
    TEST_ASSERT_EQUAL_UINT32(0, backend.begins + backend.appends + backend.seals +
                                    backend.discards + backend.reads);
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(state, f->application.execution().stateRevision);
    TEST_ASSERT_EQUAL_STRING(old.c_str(), f->store.record(0).c_str());
}
void test_application_file_empty_or_pending_refuses_collection() {
    Owners owners;
    MemoryStore journal;
    ProfileApplication app(owners, owners, journal);
    StageStore backend;
    ApplicationProfileStaging file(app, backend);
    TEST_ASSERT_FALSE(file.begin(1));
    TEST_ASSERT_TRUE(file.discard()); // Trusted stopped startup cleanup is allowed.
    TEST_ASSERT_TRUE(app.begin());
    TEST_ASSERT_FALSE(file.begin(1));
    TEST_ASSERT_FALSE(file.discard()); // Never interleave with boot activation/save.
    TEST_ASSERT_EQUAL_UINT32(0, backend.begins);
    TEST_ASSERT_EQUAL_UINT32(1, backend.discards);
    TEST_ASSERT_FALSE(app.profileFileBlocked());
}
void test_application_file_foreign_bridge_cannot_steal_or_release_owner() {
    StageStore one, two;
    ApplicationProfileStaging first(f->application, one), second(f->application, two);
    TEST_ASSERT_TRUE(first.begin(1));
    TEST_ASSERT_FALSE(second.begin(1));
    TEST_ASSERT_FALSE(second.discard());
    TEST_ASSERT_EQUAL_UINT32(0, two.begins + two.discards);
    startFenced();
    TEST_ASSERT_TRUE(first.discard());
    TEST_ASSERT_TRUE(second.begin(1));
    TEST_ASSERT_TRUE(second.discard());
}
void test_application_file_invalid_bounds_no_io_or_accidental_release() {
    StageStore backend;
    ApplicationProfileStaging file(f->application, backend);
    TEST_ASSERT_FALSE(file.begin(0));
    TEST_ASSERT_FALSE(file.begin(131073));
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    TEST_ASSERT_EQUAL_UINT32(0, backend.begins);
    TEST_ASSERT_TRUE(file.begin(2));
    TEST_ASSERT_FALSE(file.begin(2));
    TEST_ASSERT_FALSE(file.append(nullptr, 1));
    TEST_ASSERT_FALSE(file.append("x", 0));
    const std::string tooBig(121, 'x');
    TEST_ASSERT_FALSE(file.append(tooBig.data(), tooBig.size()));
    TEST_ASSERT_FALSE(file.append("abc", 3));
    TEST_ASSERT_FALSE(file.seal());
    TEST_ASSERT_EQUAL_UINT32(0, backend.appends + backend.seals);
    startFenced();
    TEST_ASSERT_TRUE(file.append("ab", 2));
    TEST_ASSERT_TRUE(file.seal());
    TEST_ASSERT_TRUE(file.discard());
}
void test_application_file_backend_failure_keeps_start_fence_until_cleanup() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        StageStore backend;
        ApplicationProfileStaging file(f->application, backend);
        backend.failBegin = mode == 0;
        if (mode == 0)
            TEST_ASSERT_FALSE(file.begin(2));
        else {
            TEST_ASSERT_TRUE(file.begin(2));
            backend.failAppend = mode == 1;
            if (mode == 1)
                TEST_ASSERT_FALSE(file.append("ab", 2));
            else {
                TEST_ASSERT_TRUE(file.append("ab", 2));
                backend.failSeal = mode == 2;
                if (mode == 2)
                    TEST_ASSERT_FALSE(file.seal());
                else {
                    TEST_ASSERT_TRUE(file.seal());
                    backend.failRead = 0;
                    std::uint8_t byte = 99;
                    fileRead(ReadStatus::Failure, file.readByte(byte));
                    TEST_ASSERT_EQUAL_UINT8(99, byte);
                }
            }
        }
        startFenced();
        TEST_ASSERT_FALSE(file.begin(2));
        TEST_ASSERT_TRUE(file.discard());
        TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    }
}
void test_application_file_failed_discard_quarantine_survives_address_reuse() {
    StageStore backend;
    alignas(ApplicationProfileStaging) unsigned char place[sizeof(ApplicationProfileStaging)];
    auto *file = new (place) ApplicationProfileStaging(f->application, backend);
    TEST_ASSERT_TRUE(file->begin(1));
    backend.failDiscard = true;
    TEST_ASSERT_FALSE(file->discard());
    const auto discards = backend.discards;
    file->~ApplicationProfileStaging(); // No backend IO, no dangling owner.
    TEST_ASSERT_EQUAL_UINT32(discards, backend.discards);
    file = new (place) ApplicationProfileStaging(f->application, backend);
    TEST_ASSERT_FALSE(file->begin(1));
    TEST_ASSERT_FALSE(file->discard());
    startFenced();
    backend.failDiscard = false;
    TEST_ASSERT_TRUE(file->discard());
    TEST_ASSERT_TRUE(file->begin(1));
    TEST_ASSERT_TRUE(file->discard());
    file->~ApplicationProfileStaging();
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
}
void test_application_file_abandoned_source_needs_explicit_cleanup_not_new_begin() {
    StageStore backend;
    {
        ApplicationProfileStaging abandoned(f->application, backend);
        TEST_ASSERT_TRUE(abandoned.begin(1));
        TEST_ASSERT_TRUE(abandoned.append("x", 1));
    }
    TEST_ASSERT_EQUAL_UINT32(0, backend.discards);
    startFenced();
    ApplicationProfileStaging replacement(f->application, backend);
    TEST_ASSERT_FALSE(replacement.begin(1));
    TEST_ASSERT_EQUAL_UINT32(1, backend.begins);
    TEST_ASSERT_TRUE(replacement.discard());
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
}
struct AppStageHook : StageStore {
    ApplicationProfileStaging *bridge = nullptr;
    unsigned checked = 0;
    std::string *input = nullptr;
};
void applicationFileHook(StageStore &value) {
    auto &hook = static_cast<AppStageHook &>(value);
    startFenced();
    TEST_ASSERT_FALSE(hook.bridge->discard()); // Same bridge is currently operating.
    TEST_ASSERT_FALSE(f->application.activateDraft(f->application.storage().draftRevision, 1, 0));
    TEST_ASSERT_FALSE(f->application.restoreStored(f->application.storage().draftRevision, 1));
    if (hook.input)
        hook.input->assign("zz");
    ++hook.checked;
}
void test_application_file_gate_precedes_every_external_call_and_freezes_append() {
    AppStageHook backend;
    ApplicationProfileStaging file(f->application, backend);
    backend.bridge = &file;
    const auto callAt = [&](unsigned at) {
        backend.on = at;
        backend.hook = applicationFileHook;
    };
    callAt(1);
    TEST_ASSERT_TRUE(file.begin(2));
    std::string input = "ab";
    backend.input = &input;
    callAt(2);
    TEST_ASSERT_TRUE(file.append(input.data(), 2));
    TEST_ASSERT_EQUAL_STRING("ab", backend.bytes.c_str());
    backend.input = nullptr;
    callAt(3);
    TEST_ASSERT_TRUE(file.seal());
    std::uint8_t byte = 0;
    callAt(5);
    fileRead(ReadStatus::Byte, file.readByte(byte));
    callAt(4);
    TEST_ASSERT_TRUE(file.discard());
    TEST_ASSERT_EQUAL_UINT32(5, backend.checked);
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
}
void test_application_file_invalid_reader_status_preserves_target_and_fences() {
    FaultTemporaryFilePort port;
    ProfileFileStaging backend(port);
    ApplicationProfileStaging file(f->application, backend);
    TEST_ASSERT_TRUE(file.begin(1));
    TEST_ASSERT_TRUE(file.append("x", 1));
    TEST_ASSERT_TRUE(file.seal());
    port.invalidRead = true;
    std::uint8_t byte = 99;
    fileRead(ReadStatus::Failure, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT8(99, byte);
    startFenced();
    TEST_ASSERT_TRUE(file.discard());
}
void test_application_file_actual_esp_receiving_blocks_gm_start_then_abort_releases() {
    using namespace host_profile_vfs;
    reset();
    EspControlFixture x;
    const auto bytes = importExample();
    copyProfileValue(x.queue, queuedProfileEnvelope(bytes), 0);
    TEST_ASSERT_TRUE(x.pump.service().metadata);
    const auto first = f->execute(f->command());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::ExecutionRejected),
                      static_cast<unsigned>(first.reply.outcome));
    TEST_ASSERT_EQUAL_STRING("busy", first.reply.executionOutcome.c_str());
    TEST_ASSERT_FALSE(f->application.gameRunning());
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Abort)), 0);
    TEST_ASSERT_TRUE(x.pump.service().control);
    RoutedProfileResponse aborted;
    TEST_ASSERT_TRUE(x.control.takeResponse(aborted));
    responseCode(ProfileResponseCode::Aborted, aborted);
    TEST_ASSERT_FALSE(present[0]);
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(f->execute(f->command()).reply.outcome));
    assertNoJournalVfs();
}
void test_application_file_actual_esp_running_metadata_does_no_vfs_work() {
    using namespace host_profile_vfs;
    reset();
    EspControlFixture x;
    TEST_ASSERT_TRUE(f->application.startGame(1, ++f->now));
    const auto before = opens + reads + writes + removes + closes + flushes;
    copyProfileValue(x.queue, queuedProfileEnvelope(importExample()), 0);
    const auto result = x.pump.service();
    TEST_ASSERT_TRUE(result.metadata);
    receiveOutcome(ProfileReceiveOutcome::StorageFailure, result.outcome);
    TEST_ASSERT_EQUAL_UINT32(before, opens + reads + writes + removes + closes + flushes);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    assertNoJournalVfs();
}
void test_application_file_revoke_or_expiry_failed_cleanup_keeps_start_fence() {
    for (unsigned mode = 0; mode < 2; ++mode) {
        StageStore backend;
        ImportClock clock;
        ApplicationProfileStaging file(f->application, backend);
        ProfileStaging source(file, f->channel, clock);
        receiveOutcome(ProfileReceiveOutcome::Receiving,
                       source.start(importEnvelope(importExample()), authority(1), 0));
        backend.failDiscard = true;
        if (mode == 1)
            f->channel.actual.updateAuthority(authority(2, false));
        else
            clock.now = 120000;
        receiveOutcome(ProfileReceiveOutcome::StorageFailure, source.service());
        startFenced();
        backend.failDiscard = false;
        receiveOutcome(ProfileReceiveOutcome::Discarded, source.discard());
        TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    }
}
void test_application_file_actual_import_reads_inside_operating_guards() {
    using namespace host_profile_vfs;
    reset();
    EspControlFixture x;
    const auto bytes = importExample();
    copyProfileValue(x.queue, queuedProfileEnvelope(bytes), 0);
    TEST_ASSERT_TRUE(x.pump.service().metadata);
    for (const auto &value : profileFrames(bytes, 128)) {
        copyProfileValue(x.queue, value, 0);
        TEST_ASSERT_TRUE(x.pump.service().received);
    }
    startFenced();
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Finish)), 0);
    TEST_ASSERT_TRUE(x.pump.service().control);
    RoutedProfileResponse ready;
    TEST_ASSERT_TRUE(x.control.takeResponse(ready));
    responseCode(ProfileResponseCode::DraftReady, ready);
    TEST_ASSERT_EQUAL_UINT32(3, ready.response.draftRevision);
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    TEST_ASSERT_FALSE(present[0]);
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply, 2)), 0);
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_TRUE(x.control.takeResponse(ready));
    responseCode(ProfileResponseCode::ApplyQueued, ready);
    pumpProfileApply();
    x.control.service();
    TEST_ASSERT_TRUE(x.control.takeResponse(ready));
    responseCode(ProfileResponseCode::Saved, ready);
    assertNoJournalVfs();
}
void test_application_file_finish_cleanup_failure_never_grants_apply_or_start() {
    using namespace host_profile_vfs;
    reset();
    EspControlFixture x;
    const auto bytes = importExample();
    copyProfileValue(x.queue, queuedProfileEnvelope(bytes), 0);
    TEST_ASSERT_TRUE(x.pump.service().metadata);
    for (const auto &value : profileFrames(bytes, 128)) {
        copyProfileValue(x.queue, value, 0);
        TEST_ASSERT_TRUE(x.pump.service().received);
    }
    failRemove = true;
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Finish)), 0);
    TEST_ASSERT_TRUE(x.pump.service().control);
    RoutedProfileResponse response;
    TEST_ASSERT_TRUE(x.control.takeResponse(response));
    responseCode(ProfileResponseCode::ReceiveRejected, response);
    startFenced();
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply, 2)), 0);
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_TRUE(x.control.takeResponse(response));
    responseCode(ProfileResponseCode::ApplyRejected, response);
    TEST_ASSERT_EQUAL_UINT32(1, f->application.snapshot().storedGeneration);
    failRemove = false;
    receiveOutcome(ProfileReceiveOutcome::Discarded, x.source.discard());
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    assertNoJournalVfs();
}

void test_application_file_actual_esp_cleanup_quarantine_observation() {
    using namespace host_profile_vfs;
    reset();
    Esp32ProfileStore mount;
    TEST_ASSERT_TRUE(mount.begin());
    Esp32ProfileStagingStore backend(mount);
    bool blocked = false, failed = false, abandoned = false, newBegin = true, started = false;
    {
        ApplicationProfileStaging file(f->application, backend);
        TEST_ASSERT_TRUE(file.begin(1));
        blocked = !f->application.startGame(1, ++f->now);
        TEST_ASSERT_TRUE(file.append("x", 1));
        TEST_ASSERT_TRUE(file.seal());
        failRemove = true;
        TEST_ASSERT_FALSE(file.discard());
        failed = !f->application.startGame(1, ++f->now);
    }
    abandoned = !f->application.startGame(1, ++f->now);
    ApplicationProfileStaging replacement(f->application, backend);
    newBegin = replacement.begin(1);
    TEST_ASSERT_FALSE(newBegin);
    failRemove = false;
    TEST_ASSERT_TRUE(replacement.discard());
    started = static_cast<bool>(f->application.startGame(1, ++f->now));
    TEST_ASSERT_TRUE(started);
    const auto before = opens + reads + writes + removes + closes + flushes;
    TEST_ASSERT_FALSE(replacement.begin(1));
    TEST_ASSERT_FALSE(replacement.discard());
    const auto runningIo = opens + reads + writes + removes + closes + flushes - before;
    TEST_ASSERT_TRUE(blocked && failed && abandoned && !newBegin && started);
    TEST_ASSERT_EQUAL_UINT32(0, runningIo);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_FALSE(present[0]);
    char observed[768];
    const auto count = std::snprintf(
        observed, sizeof(observed),
        "{\"scope\":\"host_application_file_owner\",\"temporarySource\":\"esp_adapter_host_vfs\","
        "\"journalStorage\":\"host_model\",\"blockedBeforeCleanup\":%s,\"failedCleanupBlocked\":%s,"
        "\"abandonBlocked\":%s,\"newBeginBeforeCleanup\":%s,\"cleanedStart\":%s,"
        "\"runningIoCalls\":%u,\"profileImportExecuted\":false,\"radioMeasured\":false}",
        blocked ? "true" : "false", failed ? "true" : "false", abandoned ? "true" : "false",
        newBegin ? "true" : "false", started ? "true" : "false", runningIo);
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observed));
    profileSourceFixture("application-file-owner-observation.json",
                         std::string(observed, count) + char(10));
    assertNoJournalVfs();
}

void test_application_file_import_revoke_cleans_inside_application_guard() {
    StageStore backend;
    ImportClock clock;
    ApplicationProfileStaging file(f->application, backend);
    ProfileStaging source(file, f->channel, clock);
    const auto bytes = importExample();
    const auto envelope = importEnvelope(bytes);
    receiveOutcome(ProfileReceiveOutcome::Receiving, source.start(envelope, authority(1), 0));
    stageFrames(source, bytes);
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    backend.on = 5;
    backend.hook = [](StageStore &) { f->channel.actual.updateAuthority(authority(2, false)); };
    const auto result = larp::integration::importProfileSource(f->application, f->channel, source,
                                                               envelope, authority(1), clock, 0);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(larp::integration::ProfileImportOutcome::Unauthorized),
                      static_cast<unsigned>(result.outcome));
    TEST_ASSERT_FALSE(result.receipt.issued());
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    TEST_ASSERT_EQUAL_UINT32(2, backend.discards); // pre-begin and revocation cleanup.
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(1, f->application.snapshot().storedGeneration);
}

void test_application_file_startup_cleanup_blocks_nested_boot_and_preserves_quarantine() {
    Owners owners;
    MemoryStore journal;
    ProfileApplication app(owners, owners, journal);
    struct StartupBackend : StageStore {
        ProfileApplication *app = nullptr;
        bool nestedAccepted = true;
    } backend;
    backend.app = &app;
    ApplicationProfileStaging file(app, backend);
    backend.on = 4;
    backend.hook = [](StageStore &value) {
        auto &selected = static_cast<StartupBackend &>(value);
        selected.nestedAccepted = static_cast<bool>(selected.app->begin());
    };
    backend.failDiscard = true;
    TEST_ASSERT_FALSE(file.discard());
    TEST_ASSERT_FALSE(backend.nestedAccepted);
    TEST_ASSERT_FALSE(app.storage().initialized);
    TEST_ASSERT_TRUE(app.profileFileBlocked());
    TEST_ASSERT_FALSE(app.begin());
    backend.failDiscard = false;
    TEST_ASSERT_TRUE(file.discard());
    TEST_ASSERT_TRUE(app.begin());
    TEST_ASSERT_TRUE(app.storage().initialized);
}

void test_profile_followup_failed_open_cannot_apply_previous_import_receipt() {
    using namespace host_profile_vfs;
    reset();
    EspControlFixture x;
    const auto bytes = importExample();
    copyProfileValue(x.queue, queuedProfileEnvelope(bytes), 0);
    TEST_ASSERT_TRUE(x.pump.service().metadata);
    for (const auto &value : profileFrames(bytes, 128)) {
        copyProfileValue(x.queue, value, 0);
        TEST_ASSERT_TRUE(x.pump.service().received);
    }
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Finish)), 0);
    TEST_ASSERT_TRUE(x.pump.service().control);
    RoutedProfileResponse response;
    TEST_ASSERT_TRUE(x.control.takeResponse(response));
    responseCode(ProfileResponseCode::DraftReady, response);
    const auto draft = f->application.storage().draftRevision;
    failWriteOpen = true;
    copyProfileValue(x.queue, queuedProfileEnvelope(bytes), 0);
    receiveOutcome(ProfileReceiveOutcome::StorageFailure, x.pump.service().outcome);
    failWriteOpen = false;
    TEST_ASSERT_FALSE(f->application.profileFileBlocked());
    TEST_ASSERT_EQUAL_UINT32(draft, f->application.storage().draftRevision);
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply, 2)), 0);
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_TRUE(x.control.takeResponse(response));
    responseCode(ProfileResponseCode::ApplyRejected, response);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseDetail::NoReceipt),
                      static_cast<unsigned>(response.response.detail));
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_EQUAL_UINT32(1, f->application.snapshot().storedGeneration);
    responseFixture("followup-no-receipt-v1.txt", response);
    assertNoJournalVfs();
}

void test_profile_followup_wrong_route_cannot_cancel_current_receipt() {
    ControlFixture x;
    x.imported();
    x.control.sourceAttempted(authority(2));
    x.command(ProfileOperation::Apply, 2);
    x.take(ProfileResponseCode::ApplyQueued);
    pumpProfileApply();
    x.control.service();
    x.take(ProfileResponseCode::Saved);
}
void test_profile_followup_expired_metadata_preserves_current_receipt() {
    ControlFixture x;
    x.imported();
    copyProfileValue(x.queue, queuedProfileEnvelope(importExample()), 0);
    x.clock.now = 120000;
    receiveOutcome(ProfileReceiveOutcome::Expired, x.pump.service().outcome);
    f->now = x.clock.now; // Activation and owner completion share the same fake time.
    x.command(ProfileOperation::Apply, 2, 1, x.clock.now);
    x.take(ProfileResponseCode::ApplyQueued);
    pumpProfileApply();
    x.control.service();
    x.take(ProfileResponseCode::Saved);
}
void test_profile_followup_attempt_during_selected_apply_preserves_confirmation() {
    ControlFixture x;
    x.imported();
    x.command(ProfileOperation::Apply, 2);
    x.control.sourceAttempted(authority(1)); // Pending response blocks another input.
    x.take(ProfileResponseCode::ApplyQueued);
    x.control.sourceAttempted(authority(1)); // Selected application work still blocks.
    pumpProfileApply();
    x.control.service();
    x.take(ProfileResponseCode::Saved);
}
