#pragma once
class ImportClock : public IProfileImportClock {
  public:
    std::uint32_t now = 0;
    mutable unsigned reads = 0;
    mutable unsigned expireOnRead = 0;
    std::uint32_t nowMilliseconds() const noexcept override {
        ++reads;
        return expireOnRead && reads >= expireOnRead
                   ? now + ProfileSourceLimits::DeadlineMilliseconds
                   : now;
    }
};
class ImportBytes : public IProfileReader {
  public:
    std::string bytes;
    std::size_t position = 0, calls = 0;
    std::size_t failAt = SIZE_MAX, revokeAt = SIZE_MAX, expireAt = SIZE_MAX;
    ImportClock *clock = nullptr;
    bool revokeAfterEnd = false, expireAfterEnd = false;
    void (*hook)(ImportBytes &) = nullptr;
    ReadStatus readByte(std::uint8_t &byte) override {
        ++calls;
        if (hook) {
            auto callback = hook;
            hook = nullptr;
            callback(*this);
        }
        if (position == revokeAt) {
            revokeAt = SIZE_MAX;
            f->channel.actual.suspendAuthority();
        }
        if (position == expireAt && clock)
            clock->now += ProfileSourceLimits::DeadlineMilliseconds;
        if (position == failAt)
            return ReadStatus::Failure;
        if (position == bytes.size()) {
            if (revokeAfterEnd) {
                // After the guarded EOF read, before post-validation receipt.
                host_rtos::beforeEnter = [] {
                    host_rtos::beforeEnter = [] { f->channel.actual.suspendAuthority(); };
                };
            }
            if (expireAfterEnd && clock)
                clock->expireOnRead = clock->reads + 2;
            return ReadStatus::End;
        }
        byte = static_cast<std::uint8_t>(bytes[position++]);
        return ReadStatus::Byte;
    }
};
std::string importExample() {
    std::unique_ptr<Profile> profile(new Profile);
    TEST_ASSERT_TRUE(makeHintExampleProfile(*profile));
    ProfileBytes encoded;
    TEST_ASSERT_TRUE(writeProfile(*profile, encoded));
    return encoded.bytes + char(10);
}
std::uint32_t importDigest(const std::string &bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (unsigned char byte : bytes)
        crc = profileSourceCrcByte(crc, byte);
    return crc ^ 0xffffffffu;
}
ProfileSourceEnvelope importEnvelope(const std::string &bytes) {
    ProfileSourceEnvelope envelope;
    envelope.transfer = 1;
    envelope.bytes = static_cast<std::uint32_t>(bytes.size());
    envelope.crc32 = importDigest(bytes);
    envelope.expectedDraftRevision = f->application.storage().draftRevision;
    envelope.expectedConfigurationRevision = f->application.activation().configurationRevision;
    return envelope;
}
void importOutcome(larp::integration::ProfileImportOutcome expected,
                   const larp::integration::ProfileImportResult &actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual.outcome));
}
larp::integration::ProfileImportResult
importSource(ImportBytes &source, const ProfileSourceEnvelope &envelope, ImportClock &clock,
             const TrustedAuthority &route = authority(1), std::uint32_t received = 0) {
    return larp::integration::importProfileSource(f->application, f->channel.actual, source,
                                                  envelope, route, clock, received);
}
void test_profile_source_codec_exact_envelope_and_failure_preserves_output() {
    const std::string known = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xcbf43926, importDigest(known));
    auto value = importEnvelope(known);
    char bytes[22];
    std::fill(bytes, bytes + sizeof(bytes), 'x');
    std::size_t written = 99;
    TEST_ASSERT_TRUE(encodeProfileSourceEnvelope(value, bytes + 1, 20, written));
    TEST_ASSERT_EQUAL_UINT32(20, written);
    TEST_ASSERT_EQUAL_CHAR('x', bytes[0]);
    TEST_ASSERT_EQUAL_CHAR('x', bytes[21]);
    ProfileSourceEnvelope decoded;
    TEST_ASSERT_TRUE(decodeProfileSourceEnvelope(bytes + 1, 20, decoded));
    TEST_ASSERT_EQUAL_UINT32(value.crc32, decoded.crc32);
    TEST_ASSERT_EQUAL_UINT32(value.bytes, decoded.bytes);
    for (unsigned bad = 0; bad < 9; ++bad) {
        char changed[20];
        std::memcpy(changed, bytes + 1, 20);
        std::size_t count = 20;
        if (bad == 0)
            count = 19;
        if (bad == 1)
            count = 21;
        if (bad == 2)
            changed[0] = 2;
        if (bad == 3)
            changed[1] = 1;
        if (bad == 4)
            changed[2] = changed[3] = 0;
        if (bad == 5)
            std::memset(changed + 4, 0, 4);
        if (bad == 6) {
            changed[4] = 1;
            changed[5] = 0;
            changed[6] = 2;
            changed[7] = 0;
        }
        if (bad == 7)
            std::memset(changed + 12, 0xff, 4);
        if (bad == 8)
            std::memset(changed + 16, 0, 4);
        decoded.transfer = 99;
        TEST_ASSERT_FALSE(decodeProfileSourceEnvelope(changed, count, decoded));
        TEST_ASSERT_EQUAL_UINT16(99, decoded.transfer);
    }
    decoded.transfer = 99;
    TEST_ASSERT_FALSE(decodeProfileSourceEnvelope(nullptr, 20, decoded));
    TEST_ASSERT_EQUAL_UINT16(99, decoded.transfer);
    for (unsigned bad = 0; bad < 3; ++bad) {
        written = 99;
        TEST_ASSERT_FALSE(encodeProfileSourceEnvelope(bad == 2 ? ProfileSourceEnvelope() : value,
                                                      bad == 0 ? nullptr : bytes,
                                                      bad == 1 ? 19 : 20, written));
        TEST_ASSERT_EQUAL_UINT32(0, written);
    }
    value.transfer = UINT16_MAX;
    value.bytes = ImportLimits::DocumentBytes;
    value.expectedDraftRevision = UINT32_MAX - 1;
    value.expectedConfigurationRevision = UINT32_MAX;
    value.crc32 = 0;
    TEST_ASSERT_TRUE(encodeProfileSourceEnvelope(value, bytes, 20, written));
    TEST_ASSERT_TRUE(decodeProfileSourceEnvelope(bytes, 20, decoded));
    TEST_ASSERT_EQUAL_UINT32(value.bytes, decoded.bytes);
    TEST_ASSERT_EQUAL_UINT32(value.expectedDraftRevision, decoded.expectedDraftRevision);
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    TEST_ASSERT_TRUE(commands.begin());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(FrameCopyOutcome::UnsupportedFrame),
                      static_cast<unsigned>(commands.copyWrite(bytes, 20, 7, 1, 0)));
    CopiedCommandFrame copied;
    TEST_ASSERT_FALSE(commands.take(copied));
}
void profileSourceFixture(const char *name, const std::string &bytes) {
    const auto path = std::string("docs/companion/fixtures/profile-source/") + name;
    const auto *flag = std::getenv("LARP_EXPORT_COMPANION_FIXTURES");
    if (flag && std::strcmp(flag, "1") == 0) {
        auto *file = std::fopen(path.c_str(), "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL_UINT32(bytes.size(), std::fwrite(bytes.data(), 1, bytes.size(), file));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    }
    auto *file = std::fopen(path.c_str(), "rt");
    TEST_ASSERT_NOT_NULL(file);
    std::string actual;
    for (int byte = std::fgetc(file); byte != EOF; byte = std::fgetc(file))
        actual += static_cast<char>(byte);
    TEST_ASSERT_FALSE(std::ferror(file));
    TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(bytes.c_str(), actual.c_str());
}
void test_profile_source_actual_private_draft_receipt_without_activation_or_save() {
    ImportBytes source;
    source.bytes = importExample();
    auto envelope = importEnvelope(source.bytes);
    char encoded[20];
    std::size_t written = 0;
    TEST_ASSERT_TRUE(encodeProfileSourceEnvelope(envelope, encoded, sizeof(encoded), written));
    std::string hex;
    for (unsigned char byte : encoded) {
        char value[3];
        std::snprintf(value, sizeof(value), "%02X", byte);
        hex += value;
    }
    ProfileSourceEnvelope decoded;
    TEST_ASSERT_TRUE(decodeProfileSourceEnvelope(encoded, written, decoded));
    const auto before = f->application.snapshot();
    const auto active = f->application.storage().activeProfileRevision;
    const auto old0 = f->store.record(0), old1 = f->store.record(1);
    ImportClock clock;
    const auto result = importSource(source, decoded, clock);
    importOutcome(larp::integration::ProfileImportOutcome::Ready, result);
    TEST_ASSERT_TRUE(result.receipt.issued() && result.draft);
    TEST_ASSERT_TRUE(result.receipt.original() == authority(1));
    TEST_ASSERT_EQUAL_UINT32(envelope.expectedDraftRevision + 1, result.receipt.draftRevision());
    TEST_ASSERT_EQUAL_UINT32(envelope.bytes, result.consumed);
    TEST_ASSERT_EQUAL_UINT32(envelope.bytes + 1, source.calls);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(envelope.expectedConfigurationRevision,
                             f->application.activation().configurationRevision);
    TEST_ASSERT_EQUAL_UINT32(active, f->application.storage().activeProfileRevision);
    TEST_ASSERT_EQUAL_UINT32(before.storedGeneration, f->application.snapshot().storedGeneration);
    TEST_ASSERT_EQUAL_STRING(old0.c_str(), f->store.record(0).c_str());
    TEST_ASSERT_EQUAL_STRING(old1.c_str(), f->store.record(1).c_str());
    ProfileBytes draft;
    TEST_ASSERT_TRUE(f->application.writeDraft(draft, result.receipt.draftRevision()));
    TEST_ASSERT_EQUAL_STRING(source.bytes.c_str(), (draft.bytes + char(10)).c_str());
    profileSourceFixture("hint-v1.json", source.bytes);
    profileSourceFixture("envelope-v1.txt", hex + char(10));
}
void test_profile_source_unauthorized_inputs_never_consume_or_edit() {
    ImportBytes source;
    source.bytes = importExample();
    const auto envelope = importEnvelope(source.bytes);
    ImportClock clock;
    for (auto route : {authority(1, false), authority(1, true, 0), authority(2)}) {
        importOutcome(larp::integration::ProfileImportOutcome::Unauthorized,
                      importSource(source, envelope, clock, route));
        TEST_ASSERT_EQUAL_UINT32(0, source.calls);
        TEST_ASSERT_EQUAL_UINT32(envelope.expectedDraftRevision,
                                 f->application.storage().draftRevision);
    }
    f->channel.actual.suspendAuthority();
    importOutcome(larp::integration::ProfileImportOutcome::Unauthorized,
                  importSource(source, envelope, clock));
    TEST_ASSERT_EQUAL_UINT32(0, source.calls);
}
void test_profile_source_deadline_and_future_time_never_consume_or_extend() {
    ImportBytes source;
    source.bytes = importExample();
    const auto envelope = importEnvelope(source.bytes);
    ImportClock clock;
    clock.now = ProfileSourceLimits::DeadlineMilliseconds;
    importOutcome(larp::integration::ProfileImportOutcome::Expired,
                  importSource(source, envelope, clock));
    clock.now = 0;
    importOutcome(larp::integration::ProfileImportOutcome::Expired,
                  importSource(source, envelope, clock, authority(1), 1));
    TEST_ASSERT_EQUAL_UINT32(0, source.calls);
    TEST_ASSERT_EQUAL_UINT32(envelope.expectedDraftRevision,
                             f->application.storage().draftRevision);
}
void test_profile_source_stale_running_and_busy_do_not_touch_input() {
    ImportBytes source;
    source.bytes = importExample();
    auto envelope = importEnvelope(source.bytes);
    ImportClock clock;
    ++envelope.expectedDraftRevision;
    importOutcome(larp::integration::ProfileImportOutcome::StaleRevision,
                  importSource(source, envelope, clock));
    envelope = importEnvelope(source.bytes);
    ++envelope.expectedConfigurationRevision;
    importOutcome(larp::integration::ProfileImportOutcome::StaleRevision,
                  importSource(source, envelope, clock));
    envelope = importEnvelope(source.bytes);
    f->execute(f->command());
    importOutcome(larp::integration::ProfileImportOutcome::GameRunning,
                  importSource(source, envelope, clock));
    f->execute(f->command(GameCommandKind::Stop));
    TEST_ASSERT_TRUE(
        f->application.createDraft(FactoryProfile::Hint, envelope.expectedDraftRevision));
    envelope = importEnvelope(source.bytes);
    TEST_ASSERT_TRUE(f->application.activateDraft(envelope.expectedDraftRevision,
                                                  envelope.expectedConfigurationRevision, f->now));
    importOutcome(larp::integration::ProfileImportOutcome::Busy,
                  importSource(source, envelope, clock));
    TEST_ASSERT_EQUAL_UINT32(0, source.calls);
}
void test_profile_source_length_digest_and_io_failure_leave_no_ready_draft() {
    const auto document = importExample();
    const auto protectedRecord = f->store.record(0);
    ImportClock clock;
    for (unsigned bad = 0; bad < 5; ++bad) {
        ImportBytes source;
        source.bytes = document;
        auto envelope = importEnvelope(document);
        auto expected = larp::integration::ProfileImportOutcome::SourceLengthMismatch;
        if (bad == 0)
            source.bytes.pop_back();
        if (bad == 1)
            --envelope.bytes;
        if (bad == 2) {
            envelope.crc32 ^= 1;
            expected = larp::integration::ProfileImportOutcome::SourceDigestMismatch;
        }
        if (bad == 3 || bad == 4) {
            source.failAt = bad == 3 ? 0 : source.bytes.size();
            expected = larp::integration::ProfileImportOutcome::SourceFailure;
        }
        const auto result = importSource(source, envelope, clock);
        importOutcome(expected, result);
        TEST_ASSERT_FALSE(result.receipt.issued());
        TEST_ASSERT_FALSE(f->application.storage().draftReady);
        TEST_ASSERT_FALSE(f->application.gameRunning());
        TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
        TEST_ASSERT_EQUAL_STRING(protectedRecord.c_str(), f->store.record(0).c_str());
    }
}
void test_profile_source_malformed_and_invalid_profiles_never_issue_receipt() {
    ImportClock clock;
    for (const auto &document : {std::string("{}"), std::string("[]"), importExample() + "x"}) {
        ImportBytes source;
        source.bytes = document;
        const auto result = importSource(source, importEnvelope(document), clock);
        importOutcome(larp::integration::ProfileImportOutcome::ImportFailed, result);
        TEST_ASSERT_FALSE(result.receipt.issued());
        TEST_ASSERT_FALSE(f->application.storage().draftReady);
        TEST_ASSERT_FALSE(f->application.gameRunning());
    }
}
void test_profile_source_revoke_during_byte_and_eof_cannot_become_complete() {
    ImportClock clock;
    const auto document = importExample();
    std::uint32_t epoch = 1;
    for (auto position : {std::size_t(0), std::size_t(50), document.size()}) {
        ImportBytes source;
        source.bytes = document;
        source.revokeAt = position;
        const auto result = importSource(source, importEnvelope(document), clock, authority(epoch));
        importOutcome(larp::integration::ProfileImportOutcome::Unauthorized, result);
        TEST_ASSERT_FALSE(result.receipt.issued());
        TEST_ASSERT_FALSE(f->application.storage().draftReady);
        TEST_ASSERT_FALSE(f->application.gameRunning());
        epoch += 2;
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(AuthorityUpdate::Published),
            static_cast<unsigned>(f->channel.actual.updateAuthority(authority(epoch))));
    }
}
ProfileSourceEnvelope *mutatedImportEnvelope = nullptr;
TrustedAuthority *mutatedImportRoute = nullptr;
void test_profile_source_original_envelope_and_route_survive_caller_mutation() {
    ImportBytes source;
    source.bytes = importExample();
    auto envelope = importEnvelope(source.bytes);
    auto route = authority(1);
    const auto saved = envelope;
    mutatedImportEnvelope = &envelope;
    mutatedImportRoute = &route;
    source.hook = [](ImportBytes &) {
        ++mutatedImportEnvelope->bytes;
        mutatedImportEnvelope->crc32 = 0;
        mutatedImportRoute->principal = 0;
    };
    ImportClock clock;
    const auto result = importSource(source, envelope, clock, route);
    importOutcome(larp::integration::ProfileImportOutcome::Ready, result);
    TEST_ASSERT_TRUE(result.receipt.original() == authority(1));
    TEST_ASSERT_EQUAL_UINT32(saved.bytes, result.receipt.source().bytes);
    TEST_ASSERT_EQUAL_UINT32(saved.crc32, result.receipt.source().crc32);
    mutatedImportEnvelope = nullptr;
    mutatedImportRoute = nullptr;
}
void test_profile_source_late_revoke_after_validated_eof_has_no_apply_receipt() {
    ImportBytes source;
    source.bytes = importExample();
    source.revokeAfterEnd = true;
    ImportClock clock;
    const auto result = importSource(source, importEnvelope(source.bytes), clock);
    importOutcome(larp::integration::ProfileImportOutcome::Unauthorized, result);
    TEST_ASSERT_TRUE(result.draft); // Parsed local draft, no current-route acceptance.
    TEST_ASSERT_FALSE(result.receipt.issued());
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
}
void test_profile_source_expiry_during_input_and_after_eof_never_issues_receipt() {
    for (bool late : {false, true}) {
        ImportBytes source;
        source.bytes = importExample();
        ImportClock clock;
        source.clock = &clock;
        if (late)
            source.expireAfterEnd = true;
        else
            source.expireAt = 50;
        const auto result = importSource(source, importEnvelope(source.bytes), clock);
        importOutcome(larp::integration::ProfileImportOutcome::Expired, result);
        TEST_ASSERT_FALSE(result.receipt.issued());
        TEST_ASSERT_EQUAL(late, static_cast<bool>(result.draft));
        TEST_ASSERT_FALSE(f->application.gameRunning());
        TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
    }
}
void test_profile_source_maximum_document_and_clock_wrap_stay_bounded() {
    ImportBytes source;
    source.bytes = importExample();
    source.bytes.append(ImportLimits::DocumentBytes - source.bytes.size(), ' ');
    const auto envelope = importEnvelope(source.bytes);
    ImportClock clock;
    const auto received = UINT32_MAX - 100000U;
    clock.now = received + ProfileSourceLimits::DeadlineMilliseconds - 1U;
    const auto result = importSource(source, envelope, clock, authority(1), received);
    importOutcome(larp::integration::ProfileImportOutcome::Ready, result);
    TEST_ASSERT_TRUE(result.receipt.issued());
    TEST_ASSERT_EQUAL_UINT32(ImportLimits::DocumentBytes, result.consumed);
    TEST_ASSERT_EQUAL_UINT32(ImportLimits::DocumentBytes + 1, source.calls);
}
void test_profile_source_reentrant_source_cannot_edit_start_or_tick_application() {
    ImportBytes source;
    source.bytes = importExample();
    source.hook = [](ImportBytes &) {
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(DraftOutcome::Busy),
            static_cast<unsigned>(
                f->application
                    .createDraft(FactoryProfile::Standard, f->application.storage().draftRevision)
                    .outcome));
        TEST_ASSERT_EQUAL(
            static_cast<unsigned>(larp::execution::Outcome::Busy),
            static_cast<unsigned>(
                f->application.startGame(f->application.activation().configurationRevision, f->now)
                    .outcome));
        f->application.service(++f->now);
    };
    ImportClock clock;
    const auto result = importSource(source, importEnvelope(source.bytes), clock);
    importOutcome(larp::integration::ProfileImportOutcome::Ready, result);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(0, f->application.execution().stateRevision);
}
