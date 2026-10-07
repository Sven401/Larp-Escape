#pragma once
class SealedFrames : public ISealedProfileFrames {
  public:
    std::vector<std::string> values;
    std::size_t position = 0, calls = 0, failAt = SIZE_MAX;
    bool oversized = false, dirtyEnd = false;
    void (*hook)(SealedFrames &) = nullptr;
    ProfileSourceEnvelope *callerEnvelope = nullptr;
    TrustedAuthority *callerRoute = nullptr;
    ImportClock *clock = nullptr;
    ProfileFrameRead readFrame(char *bytes, std::size_t capacity, std::size_t &written) override {
        ++calls;
        written = 0;
        if (position == failAt)
            return ProfileFrameRead::Failure;
        if (position == values.size()) {
            written = dirtyEnd ? 1 : 0;
            return ProfileFrameRead::End;
        }
        const auto &value = values[position++];
        if (value.size() > capacity)
            return ProfileFrameRead::Failure;
        std::memcpy(bytes, value.data(), value.size());
        written = oversized ? capacity + 1 : value.size();
        if (hook) {
            auto callback = hook;
            hook = nullptr;
            callback(*this);
        }
        return ProfileFrameRead::Value;
    }
};
std::vector<std::string> profileFrames(const std::string &bytes, std::size_t maximum,
                                       std::uint16_t transfer = 1) {
    std::vector<std::string> result;
    for (std::size_t offset = 0; offset < bytes.size();) {
        ProfileDataFrame frame;
        frame.transfer = transfer;
        frame.offset = static_cast<std::uint32_t>(offset);
        frame.payload = bytes.data() + offset;
        frame.count = std::min(maximum - ProfileFrameLimits::HeaderBytes, bytes.size() - offset);
        char encoded[ProfileFrameLimits::ValueBytes];
        std::size_t written = 0;
        TEST_ASSERT_TRUE(encodeProfileDataFrame(frame, encoded, sizeof(encoded), written));
        result.emplace_back(encoded, written);
        offset += frame.count;
    }
    return result;
}
std::string frameHex(const std::vector<std::string> &values) {
    std::string text;
    for (const auto &value : values) {
        for (unsigned char byte : value) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02X", byte);
            text += hex;
        }
        text += char(10);
    }
    return text;
}
void test_profile_frames_codec_32bit_offsets_exact_bounds_and_in_place_encoding() {
    char bytes[132];
    std::fill(bytes, bytes + sizeof(bytes), 'x');
    ProfileDataFrame frame;
    frame.transfer = UINT16_MAX;
    frame.offset = 65536;
    frame.payload = bytes;
    frame.count = 120;
    std::size_t written = 0;
    TEST_ASSERT_TRUE(encodeProfileDataFrame(frame, bytes + 1, 128, written));
    TEST_ASSERT_EQUAL_UINT32(128, written);
    TEST_ASSERT_EQUAL_CHAR('x', bytes[0]);
    TEST_ASSERT_EQUAL_CHAR('x', bytes[129]);
    ProfileDataFrame decoded;
    TEST_ASSERT_TRUE(decodeProfileDataFrame(bytes + 1, written, decoded));
    TEST_ASSERT_EQUAL_UINT32(65536, decoded.offset);
    TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, decoded.transfer);
    TEST_ASSERT_EQUAL_UINT32(120, decoded.count);
    for (std::size_t i = 0; i < decoded.count; ++i)
        TEST_ASSERT_EQUAL_CHAR('x', decoded.payload[i]);
    frame.offset = ImportLimits::DocumentBytes - 1;
    frame.count = 1;
    TEST_ASSERT_TRUE(encodeProfileDataFrame(frame, bytes, 9, written));
    TEST_ASSERT_TRUE(decodeProfileDataFrame(bytes, written, decoded));
    TEST_ASSERT_EQUAL_UINT32(131071, decoded.offset);
    frame.count = 2;
    TEST_ASSERT_FALSE(frame.valid());
    frame.count = 1;
    frame.offset = UINT32_MAX;
    TEST_ASSERT_FALSE(frame.valid());
}
void test_profile_frames_codec_rejects_bad_values_without_mutating_destination() {
    const auto known = profileFrames("abc", 20).front();
    for (unsigned bad = 0; bad < 9; ++bad) {
        auto value = known;
        if (bad == 0)
            value.resize(8);
        if (bad == 1)
            value.resize(129);
        if (bad == 2)
            value[0] = 2;
        if (bad == 3)
            value[1] = 1;
        if (bad == 4)
            value[2] = value[3] = 0;
        if (bad == 5)
            std::memset(&value[4], 0xff, 4);
        if (bad == 6) {
            value[4] = value[5] = 0;
            value[6] = 2;
            value[7] = 0;
        }
        if (bad == 7) {
            value[4] = value[5] = char(0xff);
            value[6] = 1;
            value[7] = 0;
        }
        ProfileDataFrame destination;
        destination.transfer = 99;
        TEST_ASSERT_FALSE(
            decodeProfileDataFrame(bad == 8 ? nullptr : value.data(), value.size(), destination));
        TEST_ASSERT_EQUAL_UINT16(99, destination.transfer);
    }
    for (unsigned bad = 0; bad < 7; ++bad) {
        char output[128];
        std::fill(output, output + sizeof(output), 'x');
        ProfileDataFrame frame;
        frame.transfer = bad == 1 ? 0 : 1;
        frame.payload = bad == 2 ? nullptr : "abc";
        frame.count = bad == 3 ? 0 : (bad == 4 ? 121 : 3);
        frame.offset = bad == 5 ? UINT32_MAX : 0;
        std::size_t written = 99;
        TEST_ASSERT_FALSE(encodeProfileDataFrame(frame, bad == 0 ? nullptr : output,
                                                 bad == 6 ? 10 : sizeof(output), written));
        TEST_ASSERT_EQUAL_UINT32(0, written);
        for (char byte : output)
            TEST_ASSERT_EQUAL_CHAR('x', byte);
    }
    FreeRtosCommandFrameQueue commands(f->channel.actual);
    TEST_ASSERT_TRUE(commands.begin());
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(FrameCopyOutcome::UnsupportedFrame),
        static_cast<unsigned>(commands.copyWrite(known.data(), known.size(), 7, 1, 0)));
}
void actualFramedProfile(std::size_t maximum) {
    const auto bytes = importExample();
    const auto envelope = importEnvelope(bytes);
    SealedFrames source;
    source.values = profileFrames(bytes, maximum);
    TEST_ASSERT_EQUAL_UINT32(maximum == 20 ? 1685 : 169, source.values.size());
    FramedProfileReader reader(source, envelope);
    ImportClock clock;
    const auto imported = larp::integration::importProfileSource(
        f->application, f->channel.actual, reader, envelope, authority(1), clock, 0);
    importOutcome(larp::integration::ProfileImportOutcome::Ready, imported);
    TEST_ASSERT_EQUAL_UINT32(bytes.size(), imported.consumed);
    TEST_ASSERT_EQUAL_UINT32(source.values.size() + 1, source.calls);
    char encoded[20];
    std::size_t written = 0;
    TEST_ASSERT_TRUE(encodeProfileSourceEnvelope(envelope, encoded, 20, written));
    auto golden = source.values;
    golden.insert(golden.begin(), std::string(encoded, written));
    profileSourceFixture(maximum == 20 ? "frames-20-v1.txt" : "frames-128-v1.txt",
                         frameHex(golden));
    applyOutcome(larp::integration::ProfileApplyOutcome::Queued, applyReceipt(imported.receipt));
    pumpProfileApply();
    TEST_ASSERT_TRUE(f->application.snapshot().storedMatchesActive);
    TEST_ASSERT_FALSE(f->application.gameRunning());
    TEST_ASSERT_EQUAL_UINT32(2, f->application.snapshot().storedGeneration);
}
void test_profile_frames_actual_20byte_complete_import_apply_and_golden_values() {
    actualFramedProfile(20);
}
void test_profile_frames_actual_128byte_complete_import_apply_and_golden_values() {
    actualFramedProfile(128);
}
void assertProfileFrameFailure(SealedFrames &source, const ProfileSourceEnvelope &envelope) {
    FramedProfileReader reader(source, envelope);
    std::uint8_t byte = 0;
    ReadStatus status = ReadStatus::Byte;
    for (std::size_t i = 0; i <= envelope.bytes + 1 && status == ReadStatus::Byte; ++i)
        status = reader.readByte(byte);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Failure), static_cast<unsigned>(status));
    const auto calls = source.calls;
    byte = 0x7b;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Failure),
                      static_cast<unsigned>(reader.readByte(byte)));
    TEST_ASSERT_EQUAL_HEX8(0x7b, byte);
    TEST_ASSERT_EQUAL_UINT32(calls, source.calls);
}
void test_profile_frames_missing_duplicate_reordered_foreign_and_extra_are_terminal() {
    const std::string bytes(30, 'x');
    const auto envelope = importEnvelope(bytes);
    for (unsigned bad = 0; bad < 11; ++bad) {
        SealedFrames source;
        source.values = profileFrames(bytes, 20);
        if (bad == 0)
            source.values.erase(source.values.begin());
        if (bad == 1)
            source.values.pop_back();
        if (bad == 2)
            source.values.insert(source.values.begin() + 1, source.values[0]);
        if (bad == 3)
            std::swap(source.values[0], source.values[1]);
        if (bad == 4)
            source.values[1][2] = 2;
        if (bad == 5)
            source.values[1][4] = 13;
        if (bad == 6)
            source.values.push_back(source.values.back());
        if (bad == 7)
            source.values.back() += 'x';
        if (bad == 8)
            source.failAt = 1;
        if (bad == 9)
            source.oversized = true;
        if (bad == 10)
            source.dirtyEnd = true;
        assertProfileFrameFailure(source, envelope);
    }
}
void test_profile_frames_invalid_metadata_never_reads_and_end_remains_final() {
    SealedFrames source;
    auto envelope = importEnvelope("abc");
    envelope.bytes = 0;
    assertProfileFrameFailure(source, envelope);
    TEST_ASSERT_EQUAL_UINT32(0, source.calls);
    envelope = importEnvelope("abc");
    source.values = profileFrames("abc", 20);
    FramedProfileReader reader(source, envelope);
    std::uint8_t byte = 0;
    for (unsigned i = 0; i < 3; ++i)
        TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::Byte),
                          static_cast<unsigned>(reader.readByte(byte)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::End),
                      static_cast<unsigned>(reader.readByte(byte)));
    const auto calls = source.calls;
    source.values.push_back(source.values[0]);
    byte = 0x7b;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::End),
                      static_cast<unsigned>(reader.readByte(byte)));
    TEST_ASSERT_EQUAL_HEX8(0x7b, byte);
    TEST_ASSERT_EQUAL_UINT32(calls, source.calls);
}
void test_profile_frames_owned_value_and_metadata_survive_caller_mutation() {
    SealedFrames source;
    auto envelope = importEnvelope("abc");
    source.callerEnvelope = &envelope;
    source.values = profileFrames("abc", 20);
    source.hook = [](SealedFrames &value) {
        *value.callerEnvelope = ProfileSourceEnvelope();
        std::fill(value.values[0].begin(), value.values[0].end(), 'z');
    };
    FramedProfileReader reader(source, envelope);
    std::string output;
    std::uint8_t byte = 0;
    while (reader.readByte(byte) == ReadStatus::Byte)
        output += static_cast<char>(byte);
    TEST_ASSERT_EQUAL_STRING("abc", output.c_str());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(ReadStatus::End),
                      static_cast<unsigned>(reader.readByte(byte)));
    TEST_ASSERT_EQUAL_UINT32(2, source.calls);
}
void test_profile_frames_crc_mismatch_has_no_receipt_activation_or_save() {
    const auto bytes = importExample();
    const auto envelope = importEnvelope(bytes);
    SealedFrames source;
    source.values = profileFrames(bytes, 128);
    source.values.back().back() = ' '; // Valid trailing whitespace, different CRC.
    FramedProfileReader reader(source, envelope);
    ImportClock clock;
    const auto oldRecord = f->store.record(0);
    const auto imported = larp::integration::importProfileSource(
        f->application, f->channel.actual, reader, envelope, authority(1), clock, 0);
    importOutcome(larp::integration::ProfileImportOutcome::SourceDigestMismatch, imported);
    TEST_ASSERT_FALSE(imported.receipt.issued());
    TEST_ASSERT_FALSE(f->application.storage().draftReady);
    TEST_ASSERT_EQUAL_UINT32(1, f->application.activation().configurationRevision);
    TEST_ASSERT_EQUAL_STRING(oldRecord.c_str(), f->store.record(0).c_str());
}
void test_profile_frames_partial_or_extra_source_cannot_become_accepted_draft() {
    const auto bytes = importExample();
    for (unsigned bad = 0; bad < 2; ++bad) {
        SealedFrames source;
        source.values = profileFrames(bytes, 128);
        if (bad == 0)
            source.values.pop_back();
        else
            source.values.push_back(source.values.back());
        const auto envelope = importEnvelope(bytes);
        FramedProfileReader reader(source, envelope);
        ImportClock clock;
        const auto imported = larp::integration::importProfileSource(
            f->application, f->channel.actual, reader, envelope, authority(1), clock, 0);
        importOutcome(larp::integration::ProfileImportOutcome::SourceFailure, imported);
        TEST_ASSERT_FALSE(imported.receipt.issued());
        TEST_ASSERT_FALSE(f->application.storage().draftReady);
        TEST_ASSERT_FALSE(f->application.busy());
    }
}
void test_profile_frames_revoke_or_expire_during_frame_copy_prevents_acceptance() {
    const auto bytes = importExample();
    for (unsigned bad = 0; bad < 2; ++bad) {
        f->channel.actual.updateAuthority(authority(bad + 1));
        SealedFrames source;
        source.values = profileFrames(bytes, 128);
        ImportClock clock;
        source.clock = &clock;
        source.hook = bad == 0 ? +[](SealedFrames &) { f->channel.actual.suspendAuthority(); }
                               : +[](SealedFrames &value) {
                                     value.clock->now += ProfileSourceLimits::DeadlineMilliseconds;
                                 };
        const auto envelope = importEnvelope(bytes);
        FramedProfileReader reader(source, envelope);
        const auto imported = larp::integration::importProfileSource(
            f->application, f->channel.actual, reader, envelope, authority(bad + 1), clock, 0);
        importOutcome(bad == 0 ? larp::integration::ProfileImportOutcome::Unauthorized
                               : larp::integration::ProfileImportOutcome::Expired,
                      imported);
        TEST_ASSERT_FALSE(imported.receipt.issued());
        TEST_ASSERT_EQUAL_UINT32(1, source.calls);
        TEST_ASSERT_EQUAL_UINT32(0, imported.consumed);
    }
}
void test_profile_frames_original_route_and_metadata_copied_before_source_call() {
    const auto bytes = importExample();
    auto envelope = importEnvelope(bytes);
    auto route = authority(1);
    SealedFrames source;
    source.values = profileFrames(bytes, 128);
    source.callerEnvelope = &envelope;
    source.callerRoute = &route;
    source.hook = [](SealedFrames &value) {
        *value.callerEnvelope = ProfileSourceEnvelope();
        *value.callerRoute = TrustedAuthority();
    };
    FramedProfileReader reader(source, envelope);
    ImportClock clock;
    const auto imported = larp::integration::importProfileSource(f->application, f->channel.actual,
                                                                 reader, envelope, route, clock, 0);
    importOutcome(larp::integration::ProfileImportOutcome::Ready, imported);
    TEST_ASSERT_EQUAL_UINT32(1, imported.receipt.original().epoch);
    TEST_ASSERT_EQUAL_UINT32(bytes.size(), imported.receipt.source().bytes);
}
void test_profile_frames_maximum_document_crosses_16bit_boundary_in_private_slot() {
    auto bytes = importExample();
    bytes.resize(ImportLimits::DocumentBytes, ' ');
    SealedFrames source;
    source.values = profileFrames(bytes, 128);
    TEST_ASSERT_EQUAL_UINT32(1093, source.values.size());
    const auto envelope = importEnvelope(bytes);
    FramedProfileReader reader(source, envelope);
    ImportClock clock;
    const auto imported = larp::integration::importProfileSource(
        f->application, f->channel.actual, reader, envelope, authority(1), clock, 0);
    importOutcome(larp::integration::ProfileImportOutcome::Ready, imported);
    TEST_ASSERT_EQUAL_UINT32(131072, imported.consumed);
    TEST_ASSERT_EQUAL_UINT32(1094, source.calls);
    TEST_ASSERT_TRUE(imported.receipt.issued());
    TEST_ASSERT_FALSE(f->application.busy());
    TEST_ASSERT_FALSE(f->application.gameRunning());
}
