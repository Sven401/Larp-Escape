#pragma once
class FaultTemporaryFilePort : public IProfileTemporaryFilePort {
  public:
    std::string bytes;
    std::size_t position = 0;
    unsigned opens = 0, writes = 0, seals = 0, reads = 0, discards = 0;
    bool failOpen = false, failWrite = false, failSeal = false, failDiscard = false,
         failRead = false, invalidRead = false;
    void (*hook)(FaultTemporaryFilePort &) = nullptr;
    ProfileFileStaging *store = nullptr;
    std::string *caller = nullptr;
    unsigned nested = 0;
    void call() {
        if (hook) {
            auto selected = hook;
            hook = nullptr;
            selected(*this);
        }
    }
    bool openTemporary() noexcept override {
        ++opens;
        call();
        bytes.clear();
        position = 0;
        return !failOpen;
    }
    bool writeTemporary(const char *value, std::size_t count) noexcept override {
        ++writes;
        call();
        bytes.append(value, failWrite ? count / 2 : count);
        return !failWrite;
    }
    bool sealTemporary() noexcept override {
        ++seals;
        call();
        position = 0;
        return !failSeal;
    }
    ReadStatus readTemporary(std::uint8_t &byte) noexcept override {
        ++reads;
        call();
        if (invalidRead)
            return static_cast<ReadStatus>(77);
        if (failRead)
            return ReadStatus::Failure;
        if (position == bytes.size())
            return ReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes[position++]);
        return ReadStatus::Byte;
    }
    bool discardTemporary() noexcept override {
        ++discards;
        call();
        if (failDiscard)
            return false;
        bytes.clear();
        position = 0;
        return true;
    }
};
void fileRead(ReadStatus expected, ReadStatus actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void test_file_staging_exact_length_bounds_and_target_preservation() {
    FaultTemporaryFilePort port;
    ProfileFileStaging file(port);
    std::uint8_t byte = 99;
    TEST_ASSERT_FALSE(file.begin(0));
    TEST_ASSERT_FALSE(file.begin(131073));
    TEST_ASSERT_EQUAL_UINT32(0, port.opens);
    TEST_ASSERT_TRUE(file.begin(3));
    TEST_ASSERT_FALSE(file.begin(3));
    TEST_ASSERT_FALSE(file.append(nullptr, 1));
    TEST_ASSERT_FALSE(file.append("x", 0));
    const std::string large(121, 'x');
    TEST_ASSERT_FALSE(file.append(large.data(), large.size()));
    TEST_ASSERT_FALSE(file.append("abcd", 4));
    TEST_ASSERT_FALSE(file.seal());
    fileRead(ReadStatus::Failure, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT8(99, byte);
    TEST_ASSERT_EQUAL_UINT32(0, port.writes);
    TEST_ASSERT_EQUAL_UINT32(0, port.seals);
    TEST_ASSERT_TRUE(file.append("abc", 3));
    TEST_ASSERT_TRUE(file.seal());
    TEST_ASSERT_FALSE(file.append("x", 1));
    TEST_ASSERT_FALSE(file.seal());
    for (const char expected : std::string("abc")) {
        fileRead(ReadStatus::Byte, file.readByte(byte));
        TEST_ASSERT_EQUAL_UINT8(expected, byte);
    }
    byte = 99;
    fileRead(ReadStatus::End, file.readByte(byte));
    fileRead(ReadStatus::End, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT8(99, byte);
    TEST_ASSERT_EQUAL_UINT32(4, port.reads);
    TEST_ASSERT_TRUE(file.discard());
    TEST_ASSERT_TRUE(file.discard());
}
void test_file_staging_partial_write_poison_and_failed_cleanup_fence() {
    FaultTemporaryFilePort port;
    ProfileFileStaging file(port);
    TEST_ASSERT_TRUE(file.begin(4));
    port.failWrite = true;
    TEST_ASSERT_FALSE(file.append("abcd", 4));
    TEST_ASSERT_EQUAL_UINT32(2, port.bytes.size());
    port.failWrite = false;
    TEST_ASSERT_FALSE(file.append("abcd", 4));
    TEST_ASSERT_FALSE(file.seal());
    port.failDiscard = true;
    TEST_ASSERT_FALSE(file.discard());
    TEST_ASSERT_FALSE(file.begin(4));
    port.failDiscard = false;
    TEST_ASSERT_TRUE(file.discard());
    TEST_ASSERT_TRUE(file.begin(4));
    TEST_ASSERT_TRUE(file.append("abcd", 4));
    TEST_ASSERT_TRUE(file.seal());
}
void test_file_staging_failed_open_or_seal_requires_discard() {
    for (unsigned mode = 0; mode < 2; ++mode) {
        FaultTemporaryFilePort port;
        ProfileFileStaging file(port);
        port.failOpen = mode == 0;
        if (mode == 0)
            TEST_ASSERT_FALSE(file.begin(1));
        else {
            TEST_ASSERT_TRUE(file.begin(1));
            TEST_ASSERT_TRUE(file.append("x", 1));
            port.failSeal = true;
            TEST_ASSERT_FALSE(file.seal());
        }
        port.failOpen = port.failSeal = false;
        TEST_ASSERT_FALSE(file.begin(1));
        TEST_ASSERT_FALSE(file.seal());
        TEST_ASSERT_TRUE(file.discard());
        TEST_ASSERT_TRUE(file.begin(1));
    }
}
void test_file_staging_short_extra_fault_or_invalid_end_poison_read() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        FaultTemporaryFilePort port;
        ProfileFileStaging file(port);
        TEST_ASSERT_TRUE(file.begin(2));
        TEST_ASSERT_TRUE(file.append("ab", 2));
        TEST_ASSERT_TRUE(file.seal());
        if (mode == 0)
            port.bytes = "a";
        if (mode == 1)
            port.bytes = "abc";
        port.failRead = mode == 2;
        port.invalidRead = mode == 3;
        std::uint8_t byte = 99;
        for (unsigned i = 0; i < (mode == 0 ? 1u : mode == 1 ? 2u : 0u); ++i)
            fileRead(ReadStatus::Byte, file.readByte(byte));
        byte = 99;
        fileRead(ReadStatus::Failure, file.readByte(byte));
        TEST_ASSERT_EQUAL_UINT8(99, byte);
        const auto reads = port.reads;
        fileRead(ReadStatus::Failure, file.readByte(byte));
        TEST_ASSERT_EQUAL_UINT32(reads, port.reads);
        TEST_ASSERT_FALSE(file.seal());
        TEST_ASSERT_FALSE(file.begin(1));
        TEST_ASSERT_TRUE(file.discard());
    }
}
void nestedFileCall(FaultTemporaryFilePort &port) {
    ++port.nested;
    if (port.caller)
        (*port.caller)[0] = '!';
    std::uint8_t byte = 99;
    TEST_ASSERT_FALSE(port.store->begin(1));
    TEST_ASSERT_FALSE(port.store->append("x", 1));
    TEST_ASSERT_FALSE(port.store->seal());
    TEST_ASSERT_FALSE(port.store->discard());
    fileRead(ReadStatus::Failure, port.store->readByte(byte));
    TEST_ASSERT_EQUAL_UINT8(99, byte);
}
void test_file_staging_freezes_append_and_fences_all_nested_operations() {
    FaultTemporaryFilePort port;
    ProfileFileStaging file(port);
    port.store = &file;
    port.hook = nestedFileCall;
    TEST_ASSERT_TRUE(file.begin(1));
    std::string caller = "x";
    port.caller = &caller;
    port.hook = nestedFileCall;
    TEST_ASSERT_TRUE(file.append(caller.data(), 1));
    TEST_ASSERT_EQUAL_STRING("x", port.bytes.c_str());
    port.hook = nestedFileCall;
    TEST_ASSERT_TRUE(file.seal());
    std::uint8_t byte = 0;
    port.hook = nestedFileCall;
    fileRead(ReadStatus::Byte, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT8('x', byte);
    port.hook = nestedFileCall;
    TEST_ASSERT_TRUE(file.discard());
    TEST_ASSERT_EQUAL_UINT32(5, port.nested);
    TEST_ASSERT_EQUAL_UINT32(1, port.opens);
    TEST_ASSERT_EQUAL_UINT32(1, port.writes);
    TEST_ASSERT_EQUAL_UINT32(1, port.seals);
    TEST_ASSERT_EQUAL_UINT32(1, port.reads);
    TEST_ASSERT_EQUAL_UINT32(1, port.discards);
}
void test_file_staging_actual_host_maximum_source_and_reuse() {
    TemporaryProfileFile file;
    std::string bytes(131072, 'x');
    for (unsigned i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<char>(i % 251);
    TEST_ASSERT_TRUE(file.begin(static_cast<std::uint32_t>(bytes.size())));
    for (std::size_t offset = 0; offset < bytes.size();) {
        const auto count = std::min<std::size_t>(120, bytes.size() - offset);
        TEST_ASSERT_TRUE(file.append(bytes.data() + offset, count));
        offset += count;
    }
    TEST_ASSERT_TRUE(file.seal());
    std::uint8_t byte = 0;
    for (const auto expected : bytes) {
        fileRead(ReadStatus::Byte, file.readByte(byte));
        TEST_ASSERT_EQUAL_UINT8(static_cast<unsigned char>(expected), byte);
    }
    fileRead(ReadStatus::End, file.readByte(byte));
    TEST_ASSERT_TRUE(file.discard());
    TEST_ASSERT_TRUE(file.begin(1));
}
void assertNoJournalVfs() {
    TEST_ASSERT_EQUAL_UINT32(0, host_profile_vfs::journalTouches);
    TEST_ASSERT_EQUAL_UINT32(0, host_profile_vfs::unknown);
    TEST_ASSERT_EQUAL_UINT32(0, host_profile_vfs::syncs);
}
void test_esp_staging_requires_existing_mount_and_never_formats() {
    using namespace host_profile_vfs;
    reset();
    Esp32ProfileStore mount;
    Esp32ProfileStagingStore file(mount);
    TEST_ASSERT_FALSE(file.begin(1));
    TEST_ASSERT_EQUAL_UINT32(0, opens);
    TEST_ASSERT_EQUAL_UINT32(0, mounts);
    TEST_ASSERT_TRUE(file.discard());
    mountResult = 1;
    TEST_ASSERT_FALSE(mount.begin());
    mountResult = 0;
    TEST_ASSERT_TRUE(mount.begin());
    TEST_ASSERT_FALSE(mount.begin());
    TEST_ASSERT_EQUAL_UINT32(2, mounts);
    TEST_ASSERT_EQUAL_STRING("/larp-profiles", lastMount.base_path);
    TEST_ASSERT_EQUAL_STRING("spiffs", lastMount.partition_label);
    TEST_ASSERT_FALSE(lastMount.format_if_mount_failed);
    TEST_ASSERT_FALSE(lastMount.grow_on_mount);
    TEST_ASSERT_FALSE(lastMount.dont_mount);
    TEST_ASSERT_TRUE(file.begin(1));
    TEST_ASSERT_TRUE(file.discard());
    assertNoJournalVfs();
}
void test_esp_staging_claim_blocks_second_source_until_confirmed_cleanup() {
    using namespace host_profile_vfs;
    reset();
    Esp32ProfileStore mount;
    TEST_ASSERT_TRUE(mount.begin());
    Esp32ProfileStagingStore first(mount), second(mount);
    TEST_ASSERT_TRUE(first.begin(1));
    const auto before = opens;
    TEST_ASSERT_FALSE(second.begin(1));
    TEST_ASSERT_FALSE(second.discard());
    TEST_ASSERT_EQUAL_UINT32(before, opens);
    TEST_ASSERT_TRUE(first.append("x", 1));
    TEST_ASSERT_TRUE(first.seal());
    failRemove = true;
    TEST_ASSERT_FALSE(first.discard());
    TEST_ASSERT_FALSE(first.begin(1));
    TEST_ASSERT_FALSE(second.discard());
    failRemove = false;
    TEST_ASSERT_TRUE(first.discard());
    TEST_ASSERT_TRUE(second.discard());
    TEST_ASSERT_TRUE(second.begin(1));
    TEST_ASSERT_TRUE(second.discard());
    assertNoJournalVfs();
}
void test_esp_staging_failed_open_partial_write_flush_close_or_reopen_no_source() {
    using namespace host_profile_vfs;
    for (unsigned mode = 0; mode < 5; ++mode) {
        reset();
        Esp32ProfileStore mount;
        TEST_ASSERT_TRUE(mount.begin());
        Esp32ProfileStagingStore file(mount);
        if (mode == 0)
            failWriteOpen = true;
        if (mode == 0)
            TEST_ASSERT_FALSE(file.begin(2));
        else {
            TEST_ASSERT_TRUE(file.begin(2));
            if (mode == 1)
                failWrite = true;
            if (mode == 1)
                TEST_ASSERT_FALSE(file.append("ab", 2));
            else {
                TEST_ASSERT_TRUE(file.append("ab", 2));
                failFlush = mode == 2;
                failClose = mode == 3;
                failReadOpen = mode == 4;
                TEST_ASSERT_FALSE(file.seal());
            }
        }
        failWriteOpen = failWrite = failFlush = failClose = failReadOpen = false;
        std::uint8_t byte = 99;
        fileRead(ReadStatus::Failure, file.readByte(byte));
        TEST_ASSERT_EQUAL_UINT8(99, byte);
        TEST_ASSERT_FALSE(file.begin(2));
        TEST_ASSERT_TRUE(file.discard());
        TEST_ASSERT_FALSE(present[0]);
        TEST_ASSERT_TRUE(file.begin(1));
        TEST_ASSERT_TRUE(file.discard());
        assertNoJournalVfs();
    }
}
void test_esp_staging_read_only_exact_end_and_corruption_fences() {
    using namespace host_profile_vfs;
    for (unsigned mode = 0; mode < 3; ++mode) {
        reset();
        Esp32ProfileStore mount;
        TEST_ASSERT_TRUE(mount.begin());
        Esp32ProfileStagingStore file(mount);
        TEST_ASSERT_TRUE(file.begin(2));
        TEST_ASSERT_TRUE(file.append("ab", 2));
        readAdjustment = mode == 0 ? -1 : mode == 1 ? 1 : 0;
        TEST_ASSERT_TRUE(file.seal());
        failRead = mode == 2;
        std::uint8_t byte = 99;
        for (unsigned i = 0; i < (mode == 0 ? 1u : mode == 1 ? 2u : 0u); ++i)
            fileRead(ReadStatus::Byte, file.readByte(byte));
        byte = 99;
        fileRead(ReadStatus::Failure, file.readByte(byte));
        TEST_ASSERT_EQUAL_UINT8(99, byte);
        failRead = false;
        TEST_ASSERT_TRUE(file.discard());
        assertNoJournalVfs();
    }
}
void test_esp_staging_old_boot_source_discard_preserves_both_journal_names() {
    using namespace host_profile_vfs;
    reset();
    present[0] = present[1] = present[2] = true;
    records[0] = "old incomplete secret-free bytes";
    records[1] = "confirmed a";
    records[2] = "confirmed b";
    Esp32ProfileStore mount;
    TEST_ASSERT_TRUE(mount.begin());
    Esp32ProfileStagingStore file(mount);
    std::uint8_t byte = 99;
    fileRead(ReadStatus::Failure, file.readByte(byte));
    TEST_ASSERT_TRUE(file.discard());
    TEST_ASSERT_FALSE(present[0]);
    TEST_ASSERT_TRUE(present[1] && present[2]);
    TEST_ASSERT_EQUAL_STRING("confirmed a", records[1].c_str());
    TEST_ASSERT_EQUAL_STRING("confirmed b", records[2].c_str());
    TEST_ASSERT_TRUE(file.begin(1));
    TEST_ASSERT_TRUE(file.append("x", 1));
    TEST_ASSERT_TRUE(file.seal());
    fileRead(ReadStatus::Byte, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT8('x', byte);
    TEST_ASSERT_TRUE(file.discard());
    assertNoJournalVfs();
}
struct EspControlFixture {
    Esp32ProfileStore mount;
    Esp32ProfileStagingStore file{mount};
    ImportClock clock;
    ProfileStaging source{file, f->channel, clock};
    ProfileTransferControl control{f->application, f->channel, source, clock};
    FreeRtosProfileFrameQueue queue{f->channel.actual};
    ProfileFramePump pump{queue, source, clock, &control};
    EspControlFixture() {
        TEST_ASSERT_TRUE(mount.begin());
        TEST_ASSERT_TRUE(queue.begin());
    }
};
void test_esp_staging_actual_raw_queue_private_apply_output_saved_model() {
    using namespace host_profile_vfs;
    reset();
    EspControlFixture x;
    const auto binding = profileGattBinding();
    TestWriteResponse replies;
    replies.live = binding;
    FreeRtosProfileResponseQueue output(f->channel, x.clock);
    TEST_ASSERT_TRUE(output.begin());
    TestReplyBearer bearer;
    ProfileResponseSender sender(f->channel, bearer);
    ProfileResponseReader reader(output, sender);
    const auto bytes = importExample();
    const auto oldRecord = f->store.record(0);
    unsigned inputCopies = 0;
    const auto admit = [&](const std::string &value) {
        auto event = gattEvent(binding, value);
        event.transaction = ++inputCopies;
        const auto result = admitGattsProfileWrite(event, binding, f->channel, x.queue, replies, 0);
        gattOutcome(GattsWriteOutcome::Copied, AttWriteStatus::Ok, result);
        TEST_ASSERT_TRUE(result.responseCopied);
    };
    admit(queuedProfileEnvelope(bytes));
    TEST_ASSERT_TRUE(x.pump.service().metadata);
    for (const auto &value : profileFrames(bytes, 128)) {
        admit(value);
        TEST_ASSERT_TRUE(x.pump.service().received);
    }
    admit(controlBytes(controlCommand(ProfileOperation::Finish)));
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_FALSE(present[0]);
    TEST_ASSERT_TRUE(opened.empty());
    const auto send = [&](ProfileResponseCode expected) {
        TEST_ASSERT_TRUE(x.control.offerResponse(output));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Loaded),
                          static_cast<unsigned>(reader.select(0)));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseSend::Copied),
                          static_cast<unsigned>(sender.service(0)));
        ProfileResponseFrame wire;
        TEST_ASSERT_TRUE(decodeProfileResponse(bearer.last.bytes.data(), 20, wire));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(wire.code));
        return wire;
    };
    send(ProfileResponseCode::DraftReady);
    admit(controlBytes(controlCommand(ProfileOperation::Apply, 2)));
    TEST_ASSERT_TRUE(x.pump.service().control);
    send(ProfileResponseCode::ApplyQueued);
    pumpProfileApply();
    x.control.service();
    const auto saved = send(ProfileResponseCode::Saved);
    RoutedProfileResponse response;
    response.original = binding.authority;
    response.response = saved;
    responseFixture("esp-staging-saved-v1.txt", response);
    TEST_ASSERT_EQUAL_UINT32(172, inputCopies);
    TEST_ASSERT_EQUAL_UINT32(172, replies.copies);
    TEST_ASSERT_EQUAL_UINT32(3, bearer.calls);
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_EQUAL_STRING(oldRecord.c_str(), f->store.record(0).c_str());
    TEST_ASSERT_FALSE(f->application.gameRunning());
    assertNoJournalVfs();
    char observed[640];
    const auto count = std::snprintf(
        observed, sizeof(observed),
        "{\"scope\":\"host_esp_staging_model\",\"writeCopies\":%u,\"attResponseCopies\":%u,"
        "\"semanticResponseCopies\":%u,"
        "\"code\":%u,\"configurationRevision\":%lu,\"storedGeneration\":%lu,\"temporaryDeleted\":%"
        "s,"
        "\"journalPathsTouched\":false,\"journalStorage\":\"host_model\",\"temporaryFsyncCalls\":%"
        "u,"
        "\"formatEnabled\":false,\"peerDeliveryMeasured\":false,\"radioInitialized\":false}",
        inputCopies, replies.copies, bearer.calls, static_cast<unsigned>(saved.code),
        static_cast<unsigned long>(saved.configurationRevision),
        static_cast<unsigned long>(f->application.snapshot().storedGeneration),
        present[0] ? "false" : "true", syncs);
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observed));
    profileSourceFixture("esp-staging-observation.json", std::string(observed, count) + char(10));
}
void test_esp_staging_source_revoke_discards_only_temporary_and_keeps_game_journal() {
    using namespace host_profile_vfs;
    reset();
    EspControlFixture x;
    const auto bytes = importExample();
    const auto oldRecord = f->store.record(0);
    copyProfileValue(x.queue, queuedProfileEnvelope(bytes), 0);
    TEST_ASSERT_TRUE(x.pump.service().metadata);
    copyProfileValue(x.queue, profileFrames(bytes, 128).front(), 0);
    TEST_ASSERT_TRUE(x.pump.service().received);
    TEST_ASSERT_TRUE(present[0]);
    f->channel.actual.suspendAuthority();
    x.pump.service();
    TEST_ASSERT_FALSE(present[0]);
    TEST_ASSERT_TRUE(opened.empty());
    receiveOutcome(ProfileReceiveOutcome::Unauthorized, x.source.outcome());
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
    TEST_ASSERT_EQUAL_STRING(oldRecord.c_str(), f->store.record(0).c_str());
    assertNoJournalVfs();
}
void test_esp_staging_actual_maximum_source_with_fixed_append_budget() {
    using namespace host_profile_vfs;
    reset();
    Esp32ProfileStore mount;
    TEST_ASSERT_TRUE(mount.begin());
    Esp32ProfileStagingStore file(mount);
    TEST_ASSERT_TRUE(file.begin(131072));
    std::string bytes(120, 'z');
    for (unsigned offset = 0; offset < 131072;) {
        const auto count = std::min<unsigned>(120, 131072 - offset);
        TEST_ASSERT_TRUE(file.append(bytes.data(), count));
        offset += count;
    }
    TEST_ASSERT_TRUE(file.seal());
    TEST_ASSERT_EQUAL_UINT32(1093, writes);
    std::uint8_t byte = 0;
    for (unsigned i = 0; i < 131072; ++i) {
        fileRead(ReadStatus::Byte, file.readByte(byte));
        TEST_ASSERT_EQUAL_UINT8('z', byte);
    }
    fileRead(ReadStatus::End, file.readByte(byte));
    TEST_ASSERT_EQUAL_UINT32(131073, reads);
    TEST_ASSERT_TRUE(file.discard());
    assertNoJournalVfs();
}

void test_esp_staging_close_failure_keeps_claim_even_after_file_removal() {
    using namespace host_profile_vfs;
    reset();
    Esp32ProfileStore mount;
    TEST_ASSERT_TRUE(mount.begin());
    Esp32ProfileStagingStore first(mount), second(mount);
    TEST_ASSERT_TRUE(first.begin(1));
    TEST_ASSERT_TRUE(first.append("x", 1));
    failClose = true;
    TEST_ASSERT_FALSE(first.discard());
    TEST_ASSERT_FALSE(present[0]);
    TEST_ASSERT_TRUE(opened.empty());
    TEST_ASSERT_FALSE(second.begin(1));
    TEST_ASSERT_FALSE(second.discard());
    failClose = false;
    TEST_ASSERT_TRUE(first.discard());
    TEST_ASSERT_TRUE(second.discard());
    TEST_ASSERT_TRUE(second.begin(1));
    TEST_ASSERT_TRUE(second.discard());
    assertNoJournalVfs();
}
void test_esp_staging_import_cleanup_failure_issues_no_apply_receipt() {
    using namespace host_profile_vfs;
    reset();
    EspControlFixture x;
    const auto bytes = importExample();
    const auto oldRecord = f->store.record(0);
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
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseDetail::StorageFailure),
                      static_cast<unsigned>(response.response.detail));
    TEST_ASSERT_TRUE(f->application.storage().draftReady);
    TEST_ASSERT_TRUE(present[0]);
    failRemove = false;
    copyProfileValue(x.queue, controlBytes(controlCommand(ProfileOperation::Apply, 2)), 0);
    TEST_ASSERT_TRUE(x.pump.service().control);
    TEST_ASSERT_TRUE(x.control.takeResponse(response));
    responseCode(ProfileResponseCode::ApplyRejected, response);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ProfileResponseDetail::NoReceipt),
                      static_cast<unsigned>(response.response.detail));
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
    TEST_ASSERT_EQUAL_STRING(oldRecord.c_str(), f->store.record(0).c_str());
    receiveOutcome(ProfileReceiveOutcome::Discarded, x.source.discard());
    TEST_ASSERT_FALSE(present[0]);
    assertNoJournalVfs();
}

void test_esp_staging_destroyed_fault_child_address_reuse_requires_cleanup() {
    using namespace host_profile_vfs;
    reset();
    Esp32ProfileStore mount;
    TEST_ASSERT_TRUE(mount.begin());
    alignas(Esp32ProfileStagingStore) unsigned char storage[sizeof(Esp32ProfileStagingStore)];
    auto *first = new (storage) Esp32ProfileStagingStore(mount);
    TEST_ASSERT_TRUE(first->begin(1));
    TEST_ASSERT_TRUE(first->append("x", 1));
    failRemove = true;
    first->~Esp32ProfileStagingStore();
    TEST_ASSERT_TRUE(present[0]);
    TEST_ASSERT_TRUE(opened.empty());
    const auto previousOpens = opens;
    auto *replacement = new (storage) Esp32ProfileStagingStore(mount);
    TEST_ASSERT_FALSE(replacement->begin(1));
    TEST_ASSERT_EQUAL_UINT32(previousOpens, opens);
    TEST_ASSERT_FALSE(replacement->discard());
    failRemove = false;
    TEST_ASSERT_TRUE(replacement->discard());
    TEST_ASSERT_FALSE(present[0]);
    TEST_ASSERT_TRUE(replacement->begin(1));
    TEST_ASSERT_TRUE(replacement->discard());
    replacement->~Esp32ProfileStagingStore();
    assertNoJournalVfs();
}
