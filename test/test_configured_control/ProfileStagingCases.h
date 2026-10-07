#pragma once
class StageStore : public IProfileStagingStore {
  public:
    std::string bytes;
    std::uint32_t limit = 0;
    std::size_t position = 0;
    unsigned begins = 0, appends = 0, seals = 0, discards = 0, reads = 0;
    bool failBegin = false, failAppend = false, failSeal = false, failDiscard = false;
    std::size_t failRead = SIZE_MAX;
    bool sealed = false;
    void (*hook)(StageStore &) = nullptr;
    unsigned on = 0;
    ProfileStaging *owner = nullptr;
    ProfileSourceEnvelope *callerEnvelope = nullptr;
    TrustedAuthority *callerRoute = nullptr;
    std::string *callerFrame = nullptr;
    ImportClock *clock = nullptr;
    void call(unsigned operation) {
        if (hook && on == operation) {
            auto selected = hook;
            hook = nullptr;
            selected(*this);
        }
    }
    bool begin(std::uint32_t count) noexcept override {
        ++begins;
        call(1);
        limit = count;
        return !failBegin;
    }
    bool append(const char *value, std::size_t count) noexcept override {
        ++appends;
        call(2);
        if (failAppend) {
            bytes.append(value, count / 2);
            return false;
        }
        bytes.append(value, count);
        return true;
    }
    bool seal() noexcept override {
        ++seals;
        call(3);
        sealed = true;
        return !failSeal;
    }
    bool discard() noexcept override {
        ++discards;
        call(4);
        bytes.clear();
        limit = 0;
        position = 0;
        sealed = false;
        return !failDiscard;
    }
    ReadStatus readByte(std::uint8_t &byte) override {
        ++reads;
        call(5);
        if (!sealed || position == failRead)
            return ReadStatus::Failure;
        if (position == bytes.size())
            return ReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes[position++]);
        return ReadStatus::Byte;
    }
};
void receiveOutcome(ProfileReceiveOutcome expected, ProfileReceiveOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void stageFrames(ProfileStaging &source, const std::string &bytes, std::size_t maximum = 128) {
    for (const auto &value : profileFrames(bytes, maximum))
        receiveOutcome(ProfileReceiveOutcome::Receiving,
                       source.data(value.data(), value.size(), authority(1)));
}
void actualStagedProfile(std::size_t maximum) {
    const auto bytes = importExample();
    const auto envelope = importEnvelope(bytes);
    TemporaryProfileFile file;
    ImportClock clock;
    ProfileStaging source(file, f->channel.actual, clock);
    const auto oldRecord = f->store.record(0);
    receiveOutcome(ProfileReceiveOutcome::Receiving, source.start(envelope, authority(1), 0));
    stageFrames(source, bytes, maximum);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Receiving),
                      static_cast<unsigned>(source.phase()));
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), envelope.transfer));
    const auto imported = larp::integration::importProfileSource(
        f->application, f->channel.actual, source, source.envelope(), source.original(), clock,
        source.receivedAt());
    importOutcome(larp::integration::ProfileImportOutcome::Ready, imported);
    applyOutcome(larp::integration::ProfileApplyOutcome::Queued, applyReceipt(imported.receipt));
    pumpProfileApply();
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_STRING(oldRecord.c_str(), f->store.record(0).c_str());
    receiveOutcome(ProfileReceiveOutcome::Discarded, source.discard());
    if (maximum == 128) {
        char observed[512];
        const int count = std::snprintf(
            observed, sizeof(observed),
            "{\"scope\":\"host_profile_staging\",\"bytes\":%lu,\"dataFrames\":169,"
            "\"temporarySource\":\"actual_host_file\",\"configurationRevision\":%lu,"
            "\"storedGeneration\":%lu,\"storedMatchesActive\":true,\"running\":false,"
            "\"storageScope\":\"host_journal_model\",\"radioMeasured\":false}",
            static_cast<unsigned long>(bytes.size()),
            static_cast<unsigned long>(f->application.activation().configurationRevision),
            static_cast<unsigned long>(f->application.snapshot().storedGeneration));
        TEST_ASSERT_TRUE(count > 0 && std::size_t(count) < sizeof(observed));
        profileSourceFixture("staging-observation.json", std::string(observed, count) + char(10));
        f->now += ControlLimits::PublishMilliseconds;
        f->control.service(f->now);
        RoutedConfiguredStatus status;
        TEST_ASSERT_TRUE(f->channel.actual.readCurrentStatus(status));
        char encoded[maxConfiguredStatusJsonBytes];
        std::size_t written = 0;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ConfiguredEncodeResult::Success),
                          static_cast<unsigned>(encodeConfiguredStatus(status.status, encoded,
                                                                       sizeof(encoded), written)));
        profileSourceFixture("staging-status-v2.json", std::string(encoded, written) + char(10));
    }
}
void test_profile_staging_actual_file_20byte_frames_import_apply_confirm() {
    actualStagedProfile(20);
}
void test_profile_staging_actual_file_128byte_frames_import_apply_confirm() {
    actualStagedProfile(128);
}
void test_profile_staging_finish_is_explicit_and_end_is_final() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    const auto envelope = importEnvelope("abc");
    receiveOutcome(ProfileReceiveOutcome::Receiving, source.start(envelope, authority(1), 0));
    stageFrames(source, "abc");
    std::uint8_t byte = 0x7b;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Failure),
                      static_cast<unsigned>(source.readByte(byte)));
    TEST_ASSERT_EQUAL_HEX8(0x7b, byte);
    TEST_ASSERT_EQUAL_UINT32(0, store.reads);
    TEST_ASSERT_EQUAL_UINT32(0, store.seals);
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    receiveOutcome(ProfileReceiveOutcome::NotReceiving, source.finish(authority(1), 1));
    for (unsigned i = 0; i < 3; ++i)
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Byte),
                          static_cast<unsigned>(source.readByte(byte)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::End),
                      static_cast<unsigned>(source.readByte(byte)));
    const auto calls = store.reads;
    byte = 0x7b;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::End),
                      static_cast<unsigned>(source.readByte(byte)));
    TEST_ASSERT_EQUAL_HEX8(0x7b, byte);
    TEST_ASSERT_EQUAL_UINT32(calls, store.reads);
}
void test_profile_staging_wrong_route_or_id_does_not_mutate_original_transfer() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope("abc"), authority(1), 0));
    const auto frame = profileFrames("abc", 20).front();
    receiveOutcome(ProfileReceiveOutcome::WrongRoute,
                   source.data(frame.data(), frame.size(), authority(2)));
    receiveOutcome(ProfileReceiveOutcome::WrongRoute, source.finish(authority(1), 2));
    receiveOutcome(ProfileReceiveOutcome::Busy,
                   source.start(importEnvelope("abc"), authority(1), 999));
    TEST_ASSERT_EQUAL_UINT32(0, store.appends);
    TEST_ASSERT_EQUAL_UINT32(0, source.received());
    TEST_ASSERT_EQUAL_UINT32(0, source.receivedAt());
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.data(frame.data(), frame.size(), authority(1)));
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
}
void test_profile_staging_bad_missing_duplicate_extra_or_digest_never_seals() {
    for (unsigned bad = 0; bad < 7; ++bad) {
        StageStore store;
        ImportClock clock;
        ProfileStaging source(store, f->channel.actual, clock);
        receiveOutcome(ProfileReceiveOutcome::Receiving,
                       source.start(importEnvelope("abc"), authority(1), 0));
        auto frame = profileFrames("abc", 20).front();
        if (bad == 0)
            frame[4] = 1;
        if (bad == 1)
            frame[2] = 2;
        if (bad == 2)
            frame.resize(8);
        if (bad == 3)
            frame += 'x';
        if (bad < 4)
            receiveOutcome(ProfileReceiveOutcome::InvalidFrame,
                           source.data(frame.data(), frame.size(), authority(1)));
        if (bad == 4)
            receiveOutcome(ProfileReceiveOutcome::Incomplete, source.finish(authority(1), 1));
        if (bad >= 5) {
            if (bad == 6)
                frame.back() = 'x';
            receiveOutcome(ProfileReceiveOutcome::Receiving,
                           source.data(frame.data(), frame.size(), authority(1)));
            if (bad == 5)
                receiveOutcome(ProfileReceiveOutcome::InvalidFrame,
                               source.data(frame.data(), frame.size(), authority(1)));
            else
                receiveOutcome(ProfileReceiveOutcome::DigestMismatch,
                               source.finish(authority(1), 1));
        }
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                          static_cast<unsigned>(source.phase()));
        TEST_ASSERT_EQUAL_UINT32(0, store.seals);
        TEST_ASSERT_TRUE(store.bytes.empty());
        TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
    }
}
void test_profile_staging_backend_open_write_seal_and_discard_failures_are_fenced() {
    for (unsigned bad = 0; bad < 4; ++bad) {
        StageStore store;
        store.failBegin = bad == 0;
        store.failAppend = bad == 1;
        store.failSeal = bad == 2;
        store.failDiscard = bad == 3;
        ImportClock clock;
        ProfileStaging source(store, f->channel.actual, clock);
        const auto started = source.start(importEnvelope("abc"), authority(1), 0);
        if (bad == 0 || bad == 3) {
            receiveOutcome(ProfileReceiveOutcome::StorageFailure, started);
            if (bad == 3)
                TEST_ASSERT_EQUAL_UINT32(0, store.begins);
        } else {
            receiveOutcome(ProfileReceiveOutcome::Receiving, started);
            const auto frame = profileFrames("abc", 20).front();
            const auto copied = source.data(frame.data(), frame.size(), authority(1));
            if (bad == 1)
                receiveOutcome(ProfileReceiveOutcome::StorageFailure, copied);
            else {
                receiveOutcome(ProfileReceiveOutcome::Receiving, copied);
                receiveOutcome(ProfileReceiveOutcome::StorageFailure,
                               source.finish(authority(1), 1));
            }
        }
        const auto discards = store.discards;
        receiveOutcome(ProfileReceiveOutcome::StorageFailure, source.service());
        TEST_ASSERT_EQUAL_UINT32(discards, store.discards);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                          static_cast<unsigned>(source.phase()));
        TEST_ASSERT_FALSE(f->application.busy());
    }
}
void test_profile_staging_waiting_revoke_or_deadline_service_discards_without_game_stop() {
    for (unsigned bad = 0; bad < 2; ++bad) {
        f->channel.actual.updateAuthority(authority(bad + 1));
        StageStore store;
        ImportClock clock;
        ProfileStaging source(store, f->channel.actual, clock);
        receiveOutcome(ProfileReceiveOutcome::Receiving,
                       source.start(importEnvelope("abc"), authority(bad + 1), 0));
        if (bad == 0)
            f->channel.actual.suspendAuthority();
        else
            clock.now = ProfileSourceLimits::DeadlineMilliseconds;
        receiveOutcome(bad == 0 ? ProfileReceiveOutcome::Unauthorized
                                : ProfileReceiveOutcome::Expired,
                       source.service());
        TEST_ASSERT_TRUE(store.bytes.empty());
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                          static_cast<unsigned>(source.phase()));
        TEST_ASSERT_FALSE(f->application.busy());
    }
}
void test_profile_staging_seal_does_not_restart_original_deadline() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope("abc"), authority(1), 0));
    clock.now = ProfileSourceLimits::DeadlineMilliseconds - 1;
    stageFrames(source, "abc");
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    clock.now += 1;
    std::uint8_t byte = 0x7b;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Failure),
                      static_cast<unsigned>(source.readByte(byte)));
    receiveOutcome(ProfileReceiveOutcome::Expired, source.outcome());
    TEST_ASSERT_EQUAL_HEX8(0x7b, byte);
    TEST_ASSERT_EQUAL_UINT32(0, store.reads);
}
void test_profile_staging_revoke_or_expire_inside_backend_prevents_complete() {
    for (unsigned bad = 0; bad < 4; ++bad) {
        f->channel.actual.updateAuthority(authority(bad + 1));
        StageStore store;
        ImportClock clock;
        ProfileStaging source(store, f->channel.actual, clock);
        const auto route = authority(bad + 1);
        receiveOutcome(ProfileReceiveOutcome::Receiving,
                       source.start(importEnvelope("abc"), route, 0));
        store.clock = &clock;
        store.on = bad < 2 ? 2 : 3;
        store.hook = bad % 2 == 0
                         ? +[](StageStore &) { f->channel.actual.suspendAuthority(); }
                         : +[](StageStore &value) {
                               value.clock->now += ProfileSourceLimits::DeadlineMilliseconds;
                           };
        const auto frame = profileFrames("abc", 20).front();
        const auto copied = source.data(frame.data(), frame.size(), route);
        const auto expected =
            bad % 2 == 0 ? ProfileReceiveOutcome::Unauthorized : ProfileReceiveOutcome::Expired;
        if (bad < 2)
            receiveOutcome(expected, copied);
        else {
            receiveOutcome(ProfileReceiveOutcome::Receiving, copied);
            receiveOutcome(expected, source.finish(route, 1));
        }
        TEST_ASSERT_TRUE(store.bytes.empty());
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                          static_cast<unsigned>(source.phase()));
    }
}
void test_profile_staging_source_io_length_and_crc_are_checked_again_after_seal() {
    for (unsigned bad = 0; bad < 4; ++bad) {
        StageStore store;
        ImportClock clock;
        ProfileStaging source(store, f->channel.actual, clock);
        receiveOutcome(ProfileReceiveOutcome::Receiving,
                       source.start(importEnvelope("abc"), authority(1), 0));
        stageFrames(source, "abc");
        receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
        if (bad == 0)
            store.bytes.pop_back();
        if (bad == 1)
            store.bytes += 'x';
        if (bad == 2)
            store.bytes.back() = 'x';
        if (bad == 3)
            store.failRead = 1;
        std::uint8_t byte = 0;
        ReadStatus status = ReadStatus::Byte;
        for (unsigned i = 0; i < 5 && status == ReadStatus::Byte; ++i)
            status = source.readByte(byte);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Failure),
                          static_cast<unsigned>(status));
        receiveOutcome(bad == 2 ? ProfileReceiveOutcome::DigestMismatch
                                : ProfileReceiveOutcome::StorageFailure,
                       source.outcome());
        const auto reads = store.reads;
        source.readByte(byte);
        TEST_ASSERT_EQUAL_UINT32(reads, store.reads);
    }
}
void test_profile_staging_reentrant_storage_cannot_replace_discard_or_finish_owner() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    store.owner = &source;
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope("abc"), authority(1), 0));
    store.on = 2;
    store.hook = [](StageStore &value) {
        receiveOutcome(ProfileReceiveOutcome::Busy, value.owner->discard());
        receiveOutcome(ProfileReceiveOutcome::Busy, value.owner->service());
        receiveOutcome(ProfileReceiveOutcome::Busy, value.owner->finish(authority(1), 1));
        receiveOutcome(ProfileReceiveOutcome::Busy,
                       value.owner->start(ProfileSourceEnvelope(), authority(1), 999));
        receiveOutcome(ProfileReceiveOutcome::Busy, value.owner->data(nullptr, 0, authority(1)));
        std::uint8_t byte = 0x7b;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Failure),
                          static_cast<unsigned>(value.owner->readByte(byte)));
        TEST_ASSERT_EQUAL_HEX8(0x7b, byte);
    };
    stageFrames(source, "abc");
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    TEST_ASSERT_EQUAL_UINT32(3, source.received());
    TEST_ASSERT_EQUAL_STRING("abc", store.bytes.c_str());
}
void test_profile_staging_copies_metadata_route_and_payload_before_backend_call() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    auto envelope = importEnvelope("abc");
    auto route = authority(1);
    store.callerEnvelope = &envelope;
    store.callerRoute = &route;
    store.on = 1;
    store.hook = [](StageStore &value) {
        *value.callerEnvelope = ProfileSourceEnvelope();
        *value.callerRoute = TrustedAuthority();
    };
    receiveOutcome(ProfileReceiveOutcome::Receiving, source.start(envelope, route, 0));
    auto frame = profileFrames("abc", 20).front();
    store.callerFrame = &frame;
    store.on = 2;
    store.hook = [](StageStore &value) {
        std::fill(value.callerFrame->begin(), value.callerFrame->end(), 'z');
    };
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.data(frame.data(), frame.size(), authority(1)));
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    TEST_ASSERT_EQUAL_STRING("abc", store.bytes.c_str());
    TEST_ASSERT_EQUAL_UINT32(1, source.original().epoch);
    TEST_ASSERT_EQUAL_UINT32(3, source.envelope().bytes);
}
void test_profile_staging_actual_file_maximum_is_complete_in_existing_private_slot() {
    auto bytes = importExample();
    bytes.resize(ImportLimits::DocumentBytes, ' ');
    TemporaryProfileFile store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope(bytes), authority(1), 0));
    stageFrames(source, bytes);
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    const auto imported = larp::integration::importProfileSource(
        f->application, f->channel.actual, source, source.envelope(), source.original(), clock,
        source.receivedAt());
    importOutcome(larp::integration::ProfileImportOutcome::Ready, imported);
    TEST_ASSERT_EQUAL_UINT32(131072, imported.consumed);
    TEST_ASSERT_FALSE(f->application.busy());
    receiveOutcome(ProfileReceiveOutcome::Discarded, source.discard());
}
void test_profile_staging_discard_invalid_authority_and_future_timestamp_do_not_open() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    receiveOutcome(ProfileReceiveOutcome::InvalidEnvelope,
                   source.start(ProfileSourceEnvelope(), authority(1), 0));
    receiveOutcome(ProfileReceiveOutcome::Unauthorized,
                   source.start(importEnvelope("abc"), authority(1, false), 0));
    receiveOutcome(ProfileReceiveOutcome::Expired,
                   source.start(importEnvelope("abc"), authority(1), 1));
    TEST_ASSERT_EQUAL_UINT32(0, store.begins);
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope("abc"), authority(1), 0));
    stageFrames(source, "abc");
    receiveOutcome(ProfileReceiveOutcome::Discarded, source.discard());
    TEST_ASSERT_TRUE(store.bytes.empty());
    TEST_ASSERT_EQUAL_UINT32(0, store.seals);
    receiveOutcome(ProfileReceiveOutcome::NotReceiving, source.finish(authority(1), 1));
}
void test_profile_staging_clock_wrap_retains_original_full_transfer_budget() {
    StageStore store;
    ImportClock clock;
    clock.now = UINT32_MAX - 20;
    ProfileStaging source(store, f->channel.actual, clock);
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope("abc"), authority(1), clock.now));
    clock.now += 21;
    stageFrames(source, "abc");
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    clock.now = source.receivedAt() + ProfileSourceLimits::DeadlineMilliseconds;
    receiveOutcome(ProfileReceiveOutcome::Expired, source.service());
}
void test_profile_staging_revoke_inside_source_read_preserves_byte_and_closes_source() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope("abc"), authority(1), 0));
    stageFrames(source, "abc");
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    store.on = 5;
    store.hook = [](StageStore &) { f->channel.actual.suspendAuthority(); };
    std::uint8_t byte = 0x7b;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Failure),
                      static_cast<unsigned>(source.readByte(byte)));
    TEST_ASSERT_EQUAL_HEX8(0x7b, byte);
    receiveOutcome(ProfileReceiveOutcome::Unauthorized, source.outcome());
    TEST_ASSERT_TRUE(store.bytes.empty());
    TEST_ASSERT_EQUAL_UINT32(1, store.reads);
}

std::string *stageCallerFrame = nullptr;
void test_profile_staging_freezes_frame_before_first_authority_observation() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope("abc"), authority(1), 0));
    auto frame = profileFrames("abc", 20).front();
    stageCallerFrame = &frame;
    host_rtos::beforeEnter = [] {
        std::fill(stageCallerFrame->begin(), stageCallerFrame->end(), 'z');
    };
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.data(frame.data(), frame.size(), authority(1)));
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    stageCallerFrame = nullptr;
    TEST_ASSERT_EQUAL_STRING("abc", store.bytes.c_str());
}
void test_profile_staging_revoke_during_cleanup_does_not_open_new_source() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    store.on = 4;
    store.hook = [](StageStore &) { f->channel.actual.suspendAuthority(); };
    receiveOutcome(ProfileReceiveOutcome::Unauthorized,
                   source.start(importEnvelope("abc"), authority(1), 0));
    TEST_ASSERT_EQUAL_UINT32(0, store.begins);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileReceivePhase::Failed),
                      static_cast<unsigned>(source.phase()));
}

void test_profile_staging_running_game_blocks_import_and_revoke_does_not_stop_it() {
    StageStore store;
    ImportClock clock;
    ProfileStaging source(store, f->channel.actual, clock);
    const auto bytes = importExample();
    receiveOutcome(ProfileReceiveOutcome::Receiving,
                   source.start(importEnvelope(bytes), authority(1), 0));
    stageFrames(source, bytes);
    receiveOutcome(ProfileReceiveOutcome::Complete, source.finish(authority(1), 1));
    TEST_ASSERT_TRUE(
        f->application.startGame(f->application.activation().configurationRevision, f->now));
    const auto draft = f->application.storage().draftRevision;
    const auto oldRecord = f->store.record(0);
    const auto imported = larp::integration::importProfileSource(
        f->application, f->channel.actual, source, source.envelope(), source.original(), clock,
        source.receivedAt());
    importOutcome(larp::integration::ProfileImportOutcome::GameRunning, imported);
    TEST_ASSERT_FALSE(imported.receipt.issued());
    TEST_ASSERT_EQUAL_UINT32(0, store.reads);
    TEST_ASSERT_EQUAL_UINT32(draft, f->application.storage().draftRevision);
    f->channel.actual.suspendAuthority();
    receiveOutcome(ProfileReceiveOutcome::Unauthorized, source.service());
    TEST_ASSERT_TRUE(store.bytes.empty());
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
    TEST_ASSERT_EQUAL_STRING(oldRecord.c_str(), f->store.record(0).c_str());
}
