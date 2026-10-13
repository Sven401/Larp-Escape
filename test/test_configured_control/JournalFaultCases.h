#pragma once
// Included in native fixture namespace; no access to private firmware controllers.
struct BootFixture {
    Owners owners;
    MemoryStore store;
    ProfileApplication app{owners, owners, store};
    std::uint32_t now = 1;
    explicit BootFixture(const MemoryStore &source) : store(source) {
        store.clearFaults();
    }
    ApplicationResult begin() {
        return app.begin(BootPolicy::ExistingOnly);
    }
    void pump() {
        for (unsigned i = 0; i < 256 && app.busy(); ++i) {
            owners.pump(now);
            app.service(now);
            now += 5;
        }
        TEST_ASSERT_FALSE(app.busy());
    }
    void ready() {
        TEST_ASSERT_TRUE(begin());
        pump();
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ApplicationPhase::Ready),
                          static_cast<unsigned>(app.snapshot().phase));
        TEST_ASSERT_TRUE(app.snapshot().storedMatchesActive);
        TEST_ASSERT_FALSE(app.gameRunning());
    }
    void hint() {
        TEST_ASSERT_TRUE(app.createDraft(FactoryProfile::Hint, app.storage().draftRevision));
        TEST_ASSERT_TRUE(app.activateDraft(app.storage().draftRevision,
                                           app.activation().configurationRevision, now));
        pump();
    }
};
void applicationPhase(ApplicationPhase phase, const ProfileApplication &app) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(phase), static_cast<unsigned>(app.snapshot().phase));
}
void journalOutcome(JournalOutcome outcome, const ProfileApplication &app) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(outcome),
                      static_cast<unsigned>(app.snapshot().journal));
}
std::uint32_t testCrc(const char *bytes, std::size_t count) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < count; ++i) {
        crc ^= static_cast<unsigned char>(bytes[i]);
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    return crc ^ 0xffffffffu;
}
void putRecord32(std::string &bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<char>(value >> (8 * i));
}
std::string envelope(const std::string &payload, std::uint32_t generation) {
    std::string value(32, '\0');
    value.replace(0, 8, "LARPPRF1");
    putRecord32(value, 8, 1);
    putRecord32(value, 12, generation);
    putRecord32(value, 16, payload.size());
    const auto crc = testCrc(payload.data(), payload.size());
    putRecord32(value, 20, crc);
    putRecord32(value, 28, testCrc(value.data(), 28));
    value += payload;
    const auto footer = value.size();
    value.append(24, '\0');
    value.replace(footer, 8, "LARPCMT1");
    putRecord32(value, footer + 8, generation);
    putRecord32(value, footer + 12, crc);
    putRecord32(value, footer + 16, payload.size());
    putRecord32(value, footer + 20, testCrc(value.data() + footer, 20));
    return value;
}
void test_existing_only_empty_and_corrupt_boot_never_create_records() {
    MemoryStore empty;
    BootFixture missing(empty);
    TEST_ASSERT_FALSE(missing.begin());
    applicationPhase(ApplicationPhase::Fault, missing.app);
    journalOutcome(JournalOutcome::Empty, missing.app);
    TEST_ASSERT_EQUAL_UINT32(0, missing.store.writeCalls);
    TEST_ASSERT_FALSE(missing.store.present(0));
    empty.replace(0, "broken");
    Owners owners;
    ProfileApplication app(owners, owners, empty);
    TEST_ASSERT_FALSE(app.begin(BootPolicy::InitializeEmpty));
    applicationPhase(ApplicationPhase::Fault, app);
    journalOutcome(JournalOutcome::Corrupt, app);
    TEST_ASSERT_EQUAL_STRING("broken", empty.record(0).c_str());
    TEST_ASSERT_EQUAL_UINT32(0, empty.writeCalls);
}
void test_partial_and_failed_truncate_preserve_confirmed_profile_for_reboot() {
    const auto old = f->store.record(0);
    for (unsigned variant = 0; variant < 5; ++variant) {
        BootFixture writing(f->store);
        writing.ready();
        const std::size_t cuts[] = {0, 16, 32, 100};
        if (variant < 4)
            writing.store.failWriteAt = cuts[variant];
        else
            writing.store.failTruncate = true;
        writing.hint();
        applicationPhase(ApplicationPhase::Fault, writing.app);
        journalOutcome(JournalOutcome::WriteUncertain, writing.app);
        TEST_ASSERT_EQUAL_UINT32(1, writing.store.writeCalls);
        TEST_ASSERT_EQUAL_UINT8(1, writing.store.lastWriteSlot);
        TEST_ASSERT_EQUAL_UINT8(0, writing.app.persistence().protectedSlot);
        TEST_ASSERT_EQUAL_STRING(old.c_str(), writing.store.record(0).c_str());
        TEST_ASSERT_FALSE(writing.app.snapshot().storedMatchesActive);
        TEST_ASSERT_FALSE(
            writing.app.startGame(writing.app.activation().configurationRevision, writing.now));
        BootFixture reboot(writing.store);
        reboot.ready();
        TEST_ASSERT_EQUAL_UINT32(f->application.storage().activeProfileRevision,
                                 reboot.app.storage().activeProfileRevision);
        TEST_ASSERT_EQUAL_UINT32(1, reboot.app.snapshot().storedGeneration);
        TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
        TEST_ASSERT_EQUAL_STRING(old.c_str(), reboot.store.record(0).c_str());
    }
}
void test_uncertain_readable_record_cannot_start_until_confirmed_retry() {
    const auto old = f->store.record(0);
    f->store.uncertainFinish = true;
    TEST_ASSERT_TRUE(
        f->application.createDraft(FactoryProfile::Hint, f->application.storage().draftRevision));
    TEST_ASSERT_TRUE(f->application.activateDraft(f->application.storage().draftRevision,
                                                  f->application.activation().configurationRevision,
                                                  f->now));
    for (unsigned i = 0; i < 256 && f->application.busy(); ++i) {
        f->owners.pump(f->now);
        f->application.service(f->now);
        f->now += 5;
    }
    applicationPhase(ApplicationPhase::Fault, f->application);
    journalOutcome(JournalOutcome::WriteUncertain, f->application);
    TEST_ASSERT_TRUE(f->store.present(1));
    TEST_ASSERT_FALSE(f->application.snapshot().storedMatchesActive);
    const auto denied = f->execute(f->command()).reply;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::NotReady),
                      static_cast<unsigned>(denied.outcome));
    TEST_ASSERT_TRUE(denied.consumed);
    f->store.uncertainFinish = false;
    TEST_ASSERT_TRUE(f->application.restoreStored(
        f->application.storage().draftRevision, f->application.activation().configurationRevision));
    TEST_ASSERT_TRUE(f->application.persistence().selectedUnconfirmed);
    TEST_ASSERT_TRUE(f->application.persistence().writeUncertain);
    TEST_ASSERT_EQUAL_UINT8(0, f->application.persistence().protectedSlot);
    TEST_ASSERT_EQUAL_UINT32(2, f->application.persistence().generation);
    TEST_ASSERT_EQUAL_STRING(old.c_str(), f->store.record(0).c_str());
    for (unsigned i = 0; i < 256 && f->application.busy(); ++i) {
        f->owners.pump(f->now);
        f->application.service(f->now);
        f->now += 5;
    }
    applicationPhase(ApplicationPhase::Ready, f->application);
    TEST_ASSERT_EQUAL_UINT32(3, f->application.snapshot().storedGeneration);
    TEST_ASSERT_EQUAL_UINT8(1, f->store.lastWriteSlot);
    TEST_ASSERT_FALSE(f->application.persistence().writeUncertain);
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_EQUAL_STRING(old.c_str(), f->store.record(0).c_str());
    const auto reply = f->execute(f->command()).reply;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GameReplyOutcome::Executed),
                      static_cast<unsigned>(reply.outcome));
    TEST_ASSERT_TRUE(f->application.gameRunning());
}
void test_damaged_newer_records_fall_back_without_auto_start() {
    BootFixture source(f->store);
    source.ready();
    source.hint();
    applicationPhase(ApplicationPhase::Ready, source.app);
    TEST_ASSERT_EQUAL_UINT32(2, source.app.snapshot().storedGeneration);
    const auto newer = source.store.record(1);
    const std::size_t badOffsets[] = {0, 8, 28, 32, newer.size() - 24, newer.size() - 1};
    for (const auto offset : badOffsets) {
        auto bytes = newer;
        bytes[offset] ^= 1;
        MemoryStore store = source.store;
        store.replace(1, bytes);
        BootFixture reboot(store);
        reboot.ready();
        TEST_ASSERT_EQUAL_UINT32(1, reboot.app.snapshot().storedGeneration);
        TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
        TEST_ASSERT_EQUAL_UINT32(f->application.storage().activeProfileRevision,
                                 reboot.app.storage().activeProfileRevision);
    }
    for (const auto length : {std::size_t(0), std::size_t(16), newer.size() - 1}) {
        MemoryStore store = source.store;
        store.replace(1, newer.substr(0, length));
        BootFixture reboot(store);
        reboot.ready();
        TEST_ASSERT_EQUAL_UINT32(1, reboot.app.snapshot().storedGeneration);
    }
    MemoryStore store = source.store;
    store.replace(1, newer + "extra");
    BootFixture reboot(store);
    reboot.ready();
    TEST_ASSERT_EQUAL_UINT32(1, reboot.app.snapshot().storedGeneration);
}
void test_equal_generations_are_ambiguous_and_do_not_overwrite() {
    MemoryStore store = f->store;
    store.replace(1, store.record(0));
    BootFixture reboot(store);
    TEST_ASSERT_FALSE(reboot.begin());
    applicationPhase(ApplicationPhase::Fault, reboot.app);
    journalOutcome(JournalOutcome::Ambiguous, reboot.app);
    TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
    TEST_ASSERT_EQUAL_STRING(store.record(0).c_str(), reboot.store.record(0).c_str());
    TEST_ASSERT_EQUAL_STRING(store.record(1).c_str(), reboot.store.record(1).c_str());
}
void test_future_record_version_preserves_bytes_and_blocks_save() {
    MemoryStore store = f->store;
    auto future = store.record(0);
    putRecord32(future, 8, 2);
    putRecord32(future, 28, testCrc(future.data(), 28));
    store.replace(1, future);
    BootFixture reboot(store);
    reboot.ready();
    TEST_ASSERT_FALSE(reboot.app.persistence().writable);
    reboot.hint();
    applicationPhase(ApplicationPhase::Fault, reboot.app);
    journalOutcome(JournalOutcome::NeedsRestore, reboot.app);
    TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
    TEST_ASSERT_EQUAL_STRING(future.c_str(), reboot.store.record(1).c_str());
}
void test_backend_open_read_close_failures_block_initialization() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        BootFixture reboot(f->store);
        if (!mode)
            reboot.store.failOpen[0] = true;
        else if (mode == 1) {
            reboot.store.failReadSlot = 0;
            reboot.store.failReadAt = 40;
        } else
            reboot.store.failCloseCall = 1;
        TEST_ASSERT_FALSE(reboot.begin());
        applicationPhase(ApplicationPhase::Fault, reboot.app);
        journalOutcome(JournalOutcome::IoFailure, reboot.app);
        TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
        TEST_ASSERT_EQUAL_STRING(f->store.record(0).c_str(), reboot.store.record(0).c_str());
    }
}
void test_import_close_failure_never_leaves_ready_draft() {
    BootFixture reboot(f->store);
    reboot.store.failCloseCall = 2;
    TEST_ASSERT_FALSE(reboot.begin()); // inspect closes once; import-end close fails.
    journalOutcome(JournalOutcome::IoFailure, reboot.app);
    TEST_ASSERT_FALSE(reboot.app.storage().draftReady);
    TEST_ASSERT_FALSE(reboot.app.storage().hasActive);
    TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
}
void test_record_changed_after_inspection_is_rejected() {
    for (const auto offset : {std::size_t(12), std::size_t(50)}) {
        BootFixture reboot(f->store);
        reboot.store.mutateOpenCall = 3;
        reboot.store.mutateOffset = offset;
        TEST_ASSERT_FALSE(reboot.begin()); // Two inspect opens, then actual selected import.
        journalOutcome(JournalOutcome::Corrupt, reboot.app);
        TEST_ASSERT_FALSE(reboot.app.storage().draftReady);
        TEST_ASSERT_FALSE(reboot.app.storage().hasActive);
        TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
    }
}
void test_generation_maximum_never_wraps_or_overwrites() {
    MemoryStore store = f->store;
    const auto old = store.record(0);
    store.replace(0, envelope(old.substr(32, old.size() - 56), UINT32_MAX));
    BootFixture reboot(store);
    reboot.ready();
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, reboot.app.snapshot().storedGeneration);
    const auto preserved = reboot.store.record(0);
    reboot.hint();
    applicationPhase(ApplicationPhase::Fault, reboot.app);
    journalOutcome(JournalOutcome::CounterExhausted, reboot.app);
    TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
    TEST_ASSERT_EQUAL_STRING(preserved.c_str(), reboot.store.record(0).c_str());
    TEST_ASSERT_FALSE(reboot.store.present(1));
}
void test_semantically_invalid_record_falls_back_to_valid_profile() {
    MemoryStore store = f->store;
    store.replace(1, envelope("{}", 2));
    BootFixture reboot(store);
    TEST_ASSERT_TRUE(reboot.begin());
    TEST_ASSERT_EQUAL_UINT32(2, reboot.app.storage().draftRevision); // Failed then valid import.
    reboot.pump();
    applicationPhase(ApplicationPhase::Ready, reboot.app);
    TEST_ASSERT_EQUAL_UINT32(1, reboot.app.snapshot().storedGeneration);
    TEST_ASSERT_EQUAL_UINT32(2, reboot.app.persistence().maximumGeneration);
    TEST_ASSERT_EQUAL_UINT32(3, reboot.app.storage().draftRevision); // Activation consumes draft.
    TEST_ASSERT_EQUAL_UINT32(0, reboot.store.writeCalls);
    TEST_ASSERT_EQUAL_STRING("{}", reboot.store.record(1).substr(32, 2).c_str());
}
void test_missing_previously_confirmed_records_are_not_fresh_empty_store() {
    f->store.erase(0);
    f->store.erase(1);
    const auto writes = f->store.writeCalls;
    TEST_ASSERT_FALSE(f->application.restoreStored(
        f->application.storage().draftRevision, f->application.activation().configurationRevision));
    journalOutcome(JournalOutcome::Corrupt, f->application);
    applicationPhase(ApplicationPhase::Fault, f->application);
    TEST_ASSERT_EQUAL_UINT32(writes, f->store.writeCalls);
    TEST_ASSERT_FALSE(f->store.present(0));
    TEST_ASSERT_FALSE(
        f->application.startGame(f->application.activation().configurationRevision, f->now));
}
