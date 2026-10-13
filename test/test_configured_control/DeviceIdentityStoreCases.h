#pragma once
Identity publicLabel(unsigned seed = 1) {
    Identity identity;
    for (unsigned i = 0; i < 16; ++i)
        identity.bytes[i] = static_cast<std::uint8_t>(seed + i);
    return identity;
}
DeviceIdentityRecord publicRecord(unsigned seed = 1) {
    DeviceIdentityRecord record;
    TEST_ASSERT_TRUE(encodeDeviceIdentityRecord(publicLabel(seed), record));
    return record;
}
void identityOutcome(DeviceIdentityOutcome expected, DeviceIdentityOutcome actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void recordRead(IdentityRecordRead expected, IdentityRecordRead actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
void recordCreate(IdentityRecordCreate expected, IdentityRecordCreate actual) {
    TEST_ASSERT_EQUAL(static_cast<unsigned>(expected), static_cast<unsigned>(actual));
}
class ModelIdentityEntropy : public IDeviceIdentityEntropy {
  public:
    Identity value = publicLabel();
    unsigned calls = 0;
    bool fail = false;
    bool make(Identity &destination) noexcept override {
        ++calls;
        destination = value;
        return !fail;
    }
};
class ModelIdentityStore : public IDeviceIdentityRecordStore {
  public:
    DeviceIdentityRecord record = publicRecord();
    IdentityRecordRead readResult = IdentityRecordRead::Missing;
    IdentityRecordCreate createResult = IdentityRecordCreate::Confirmed;
    unsigned reads = 0, creates = 0;
    bool raceExisting = false, mismatchAfterCreate = false, missingAfterCreate = false,
         failAfterCreate = false, corruptAfterCreate = false, persistUncertain = false;
    void (*hook)(ModelIdentityStore &) = nullptr;
    DeviceIdentityOwner *owner = nullptr;
    void observe() {
        if (hook) {
            auto selected = hook;
            hook = nullptr;
            selected(*this);
        }
    }
    IdentityRecordRead read(DeviceIdentityRecord &destination) noexcept override {
        ++reads;
        observe();
        if (readResult == IdentityRecordRead::Present)
            destination = record;
        return readResult;
    }
    IdentityRecordCreate createIfMissing(const DeviceIdentityRecord &selected) noexcept override {
        ++creates;
        observe();
        if (raceExisting) {
            record = publicRecord(42);
            readResult = IdentityRecordRead::Present;
            return IdentityRecordCreate::AlreadyPresent;
        }
        if (createResult == IdentityRecordCreate::Confirmed || persistUncertain) {
            record = selected;
            readResult = IdentityRecordRead::Present;
        }
        if (mismatchAfterCreate)
            record = publicRecord(42);
        if (corruptAfterCreate)
            record.bytes[24] ^= 1;
        if (missingAfterCreate)
            readResult = IdentityRecordRead::Missing;
        if (failAfterCreate)
            readResult = IdentityRecordRead::Failure;
        return createResult;
    }
};
void unavailableIdentity(DeviceIdentityOwner &owner) {
    auto destination = publicLabel(99);
    const auto old = destination;
    TEST_ASSERT_FALSE(owner.identity(destination));
    TEST_ASSERT_TRUE(destination == old);
}
void identityRecordFixture(const char *name, const std::string &bytes) {
    const auto path = std::string("docs/companion/fixtures/device-identity-store/") + name;
    if (std::getenv("LARP_EXPORT_COMPANION_FIXTURES")) {
        auto *file = std::fopen(path.c_str(), "wb");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_EQUAL_UINT32(bytes.size(), std::fwrite(bytes.data(), 1, bytes.size(), file));
        TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    }
    auto *file = std::fopen(path.c_str(), "rb");
    TEST_ASSERT_NOT_NULL(file);
    std::string actual;
    for (int byte = std::fgetc(file); byte != EOF; byte = std::fgetc(file))
        actual += static_cast<char>(byte);
    TEST_ASSERT_EQUAL_INT(0, std::ferror(file));
    TEST_ASSERT_EQUAL_INT(0, std::fclose(file));
    TEST_ASSERT_EQUAL_STRING(bytes.c_str(), actual.c_str());
}
void test_device_identity_record_exact_crc_bounds_and_target_preservation() {
    const auto record = publicRecord();
    Identity decoded;
    TEST_ASSERT_TRUE(decodeDeviceIdentityRecord(record, decoded));
    TEST_ASSERT_TRUE(decoded == publicLabel());
    TEST_ASSERT_EQUAL_UINT32(28, sizeof(record));
    for (unsigned at = 0; at < 28; ++at) {
        auto changed = record;
        changed.bytes[at] ^= 1;
        decoded = publicLabel(99);
        const auto old = decoded;
        TEST_ASSERT_FALSE(decodeDeviceIdentityRecord(changed, decoded));
        TEST_ASSERT_TRUE(decoded == old);
    }
    auto target = record;
    TEST_ASSERT_FALSE(encodeDeviceIdentityRecord({}, target));
    TEST_ASSERT_TRUE(target == record);
    auto zero = record;
    for (unsigned i = 0; i < 16; ++i)
        zero.bytes[8 + i] = 0;
    std::uint32_t crc = 0xffffffffu;
    for (unsigned i = 0; i < 24; ++i)
        crc = profileSourceCrcByte(crc, zero.bytes[i]);
    crc ^= 0xffffffffu;
    for (unsigned i = 0; i < 4; ++i)
        zero.bytes[24 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
    TEST_ASSERT_FALSE(decodeDeviceIdentityRecord(zero, decoded));
}
void test_device_identity_loaded_is_immutable_without_entropy_or_writes() {
    ModelIdentityStore store;
    store.readResult = IdentityRecordRead::Present;
    ModelIdentityEntropy entropy;
    DeviceIdentityOwner owner(store, entropy);
    identityOutcome(DeviceIdentityOutcome::Loaded, owner.begin());
    Identity selected;
    TEST_ASSERT_TRUE(owner.identity(selected));
    TEST_ASSERT_TRUE(selected == publicLabel());
    store.record = publicRecord(42);
    selected = publicLabel(99);
    identityOutcome(DeviceIdentityOutcome::Loaded, owner.begin());
    TEST_ASSERT_TRUE(owner.identity(selected));
    TEST_ASSERT_TRUE(selected == publicLabel());
    TEST_ASSERT_EQUAL_UINT32(1, store.reads);
    TEST_ASSERT_EQUAL_UINT32(0, store.creates);
    TEST_ASSERT_EQUAL_UINT32(0, entropy.calls);
}
void test_device_identity_missing_create_verify_and_restart_keep_same_label() {
    ModelIdentityStore store;
    ModelIdentityEntropy entropy;
    DeviceIdentityOwner first(store, entropy);
    unavailableIdentity(first);
    identityOutcome(DeviceIdentityOutcome::Created, first.begin());
    Identity identity;
    TEST_ASSERT_TRUE(first.identity(identity));
    TEST_ASSERT_TRUE(identity == publicLabel());
    DeviceIdentityOwner restarted(store, entropy);
    identityOutcome(DeviceIdentityOutcome::Loaded, restarted.begin());
    Identity after;
    TEST_ASSERT_TRUE(restarted.identity(after));
    TEST_ASSERT_TRUE(after == identity);
    TEST_ASSERT_EQUAL_UINT32(1, store.creates);
    TEST_ASSERT_EQUAL_UINT32(1, entropy.calls);
    TEST_ASSERT_EQUAL_UINT32(3, store.reads);
}
void test_device_identity_entropy_failure_or_zero_never_writes() {
    for (unsigned mode = 0; mode < 2; ++mode) {
        ModelIdentityStore store;
        ModelIdentityEntropy entropy;
        entropy.fail = mode == 0;
        if (mode == 1)
            entropy.value = {};
        DeviceIdentityOwner owner(store, entropy);
        identityOutcome(DeviceIdentityOutcome::EntropyFailure, owner.begin());
        unavailableIdentity(owner);
        entropy.fail = false;
        entropy.value = publicLabel();
        identityOutcome(DeviceIdentityOutcome::EntropyFailure, owner.begin());
        TEST_ASSERT_EQUAL_UINT32(0, store.creates);
        TEST_ASSERT_EQUAL_UINT32(1, entropy.calls);
    }
}
void test_device_identity_read_failure_or_corrupt_never_regenerates() {
    for (unsigned mode = 0; mode < 2; ++mode) {
        ModelIdentityStore store;
        store.readResult = mode == 0 ? IdentityRecordRead::Failure : IdentityRecordRead::Present;
        if (mode == 1)
            store.record.bytes[0] ^= 1;
        ModelIdentityEntropy entropy;
        DeviceIdentityOwner owner(store, entropy);
        const auto expected =
            mode == 0 ? DeviceIdentityOutcome::ReadFailure : DeviceIdentityOutcome::Corrupt;
        identityOutcome(expected, owner.begin());
        unavailableIdentity(owner);
        store.readResult = IdentityRecordRead::Missing;
        identityOutcome(expected, owner.begin());
        TEST_ASSERT_EQUAL_UINT32(0, store.creates);
        TEST_ASSERT_EQUAL_UINT32(0, entropy.calls);
        TEST_ASSERT_EQUAL_UINT32(1, store.reads);
    }
}
void test_device_identity_uncertain_or_failed_write_never_publishes_candidate() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        ModelIdentityStore store;
        store.createResult =
            mode == 0 ? IdentityRecordCreate::Failure : IdentityRecordCreate::Uncertain;
        store.persistUncertain = mode == 2;
        ModelIdentityEntropy entropy;
        DeviceIdentityOwner owner(store, entropy);
        const auto expected =
            mode == 0 ? DeviceIdentityOutcome::WriteFailure : DeviceIdentityOutcome::Uncertain;
        identityOutcome(expected, owner.begin());
        unavailableIdentity(owner);
        store.createResult = IdentityRecordCreate::Confirmed;
        identityOutcome(expected, owner.begin());
        TEST_ASSERT_EQUAL_UINT32(1, store.creates);
        TEST_ASSERT_EQUAL_UINT32(1, store.reads);
    }
}
void test_device_identity_confirmed_readback_must_match_and_be_complete() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        ModelIdentityStore store;
        store.missingAfterCreate = mode == 0;
        store.mismatchAfterCreate = mode == 1;
        store.failAfterCreate = mode == 2;
        store.corruptAfterCreate = mode == 3;
        ModelIdentityEntropy entropy;
        DeviceIdentityOwner owner(store, entropy);
        identityOutcome(DeviceIdentityOutcome::VerifyFailure, owner.begin());
        unavailableIdentity(owner);
        TEST_ASSERT_EQUAL_UINT32(2, store.reads);
        store.record = publicRecord();
        store.readResult = IdentityRecordRead::Present;
        identityOutcome(DeviceIdentityOutcome::VerifyFailure, owner.begin());
        TEST_ASSERT_EQUAL_UINT32(2, store.reads);
    }
}
void test_device_identity_concurrent_existing_value_wins_without_overwrite() {
    ModelIdentityStore store;
    store.raceExisting = true;
    ModelIdentityEntropy entropy;
    DeviceIdentityOwner owner(store, entropy);
    identityOutcome(DeviceIdentityOutcome::Loaded, owner.begin());
    Identity selected;
    TEST_ASSERT_TRUE(owner.identity(selected));
    TEST_ASSERT_TRUE(selected == publicLabel(42));
    TEST_ASSERT_FALSE(selected == entropy.value);
    TEST_ASSERT_EQUAL_UINT32(1, store.creates);
}
void test_device_identity_reentrant_begin_and_observation_expose_no_pending_label() {
    ModelIdentityStore store;
    ModelIdentityEntropy entropy;
    DeviceIdentityOwner owner(store, entropy);
    store.owner = &owner;
    store.hook = [](ModelIdentityStore &selected) {
        identityOutcome(DeviceIdentityOutcome::Busy, selected.owner->begin());
        unavailableIdentity(*selected.owner);
        selected.hook = [](ModelIdentityStore &again) {
            identityOutcome(DeviceIdentityOutcome::Busy, again.owner->begin());
            unavailableIdentity(*again.owner);
        };
    };
    identityOutcome(DeviceIdentityOutcome::Created, owner.begin());
    TEST_ASSERT_EQUAL_UINT32(2, store.reads);
    TEST_ASSERT_EQUAL_UINT32(1, store.creates);
}
void test_device_identity_nvs_init_full_or_mutex_busy_never_erases_or_opens() {
    using namespace host_identity_nvs;
    reset();
    Esp32DeviceIdentityStore store;
    DeviceIdentityRecord destination = publicRecord(99);
    const auto old = destination;
    recordRead(IdentityRecordRead::Failure, store.read(destination));
    TEST_ASSERT_TRUE(destination == old);
    TEST_ASSERT_EQUAL_UINT32(0, opens);
    host_mutex::failCreate = true;
    TEST_ASSERT_FALSE(store.begin());
    TEST_ASSERT_EQUAL_UINT32(0, inits);
    host_mutex::failCreate = false;
    host_mutex::forceBusy = true;
    TEST_ASSERT_FALSE(store.begin());
    TEST_ASSERT_EQUAL_UINT32(0, inits);
    host_mutex::forceBusy = false;
    initError = 1;
    TEST_ASSERT_FALSE(store.begin());
    TEST_ASSERT_EQUAL_UINT32(1, inits);
    initError = 0;
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_EQUAL_UINT32(2, inits);
    host_mutex::forceBusy = true;
    recordRead(IdentityRecordRead::Failure, store.read(destination));
    recordCreate(IdentityRecordCreate::Failure, store.createIfMissing(publicRecord()));
    TEST_ASSERT_EQUAL_UINT32(0, opens);
    TEST_ASSERT_TRUE(destination == old);
    host_mutex::forceBusy = false;
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_device_identity_actual_nvs_startup_record_public_metadata_restart() {
    using namespace host_identity_nvs;
    reset();
    Esp32DeviceIdentityStore store;
    TEST_ASSERT_TRUE(store.begin());
    Esp32StartupIdentityEntropy entropy;
    DeviceIdentityOwner first(store, entropy);
    identityOutcome(DeviceIdentityOutcome::Created, first.begin());
    Identity identity;
    TEST_ASSERT_TRUE(first.identity(identity));
    TEST_ASSERT_TRUE(identity == publicLabel());
    DeviceIdentityOwner restarted(store, entropy);
    identityOutcome(DeviceIdentityOutcome::Loaded, restarted.begin());
    Identity after;
    TEST_ASSERT_TRUE(restarted.identity(after));
    TEST_ASSERT_TRUE(after == identity);
    TEST_ASSERT_EQUAL_UINT32(1, sets);
    TEST_ASSERT_EQUAL_UINT32(1, commits);
    TEST_ASSERT_EQUAL_UINT32(1, fills);
    TEST_ASSERT_TRUE(handles.empty());
    TEST_ASSERT_FALSE(badEntropyOrder);
    TEST_ASSERT_FALSE(entropyActive);
    TEST_ASSERT_EQUAL_UINT32(0, unknown);
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
    std::string hex;
    char part[3];
    for (const auto byte : value) {
        std::snprintf(part, sizeof(part), "%02X", byte);
        hex += part;
    }
    identityRecordFixture("record-v1.txt", hex + char(10));
    char metadata[256];
    std::size_t written = 0;
    TEST_ASSERT_EQUAL(
        static_cast<unsigned>(ConfiguredEncodeResult::Success),
        static_cast<unsigned>(encodeDeviceIdentity(after, metadata, sizeof(metadata), written)));
    identityRecordFixture("metadata-v1.json", std::string(metadata, written) + char(10));
    char observation[384];
    const auto count =
        std::snprintf(observation, sizeof(observation),
                      "{\"scope\":\"host_nvs_model\",\"recordBytes\":%u,\"setCalls\":%u,"
                      "\"commitCalls\":%u,\"entropyFills\":%u,"
                      "\"sameAfterRestart\":true,\"secretsStored\":false,\"grantsGameMaster\":"
                      "false,\"radioInitialized\":false,\"physicalDurabilityMeasured\":false}",
                      unsigned(value.size()), sets, commits, fills);
    TEST_ASSERT_TRUE(count > 0 && static_cast<std::size_t>(count) < sizeof(observation));
    identityRecordFixture("observation.json", std::string(observation, count) + char(10));
}
void test_device_identity_nvs_bad_type_length_or_second_read_preserves_target() {
    using namespace host_identity_nvs;
    for (unsigned mode = 0; mode < 4; ++mode) {
        reset();
        namespacePresent = present = true;
        const auto expected = publicRecord();
        value.assign(expected.bytes.begin(), expected.bytes.end());
        if (mode == 0)
            getError = ESP_ERR_NVS_TYPE_MISMATCH;
        if (mode == 1)
            value.push_back(1);
        if (mode == 2)
            value.pop_back();
        Esp32DeviceIdentityStore store;
        TEST_ASSERT_TRUE(store.begin());
        DeviceIdentityRecord target = publicRecord(99);
        const auto old = target;
        if (mode == 3)
            beforeGet = [] { beforeGet = [] { value.push_back(1); }; };
        recordRead(IdentityRecordRead::Failure, store.read(target));
        TEST_ASSERT_TRUE(target == old);
        TEST_ASSERT_TRUE(handles.empty());
        TEST_ASSERT_EQUAL_UINT32(0, sets);
    }
}
void test_device_identity_actual_nvs_set_or_commit_uncertain_stays_closed() {
    using namespace host_identity_nvs;
    for (unsigned mode = 0; mode < 4; ++mode) {
        reset();
        setError = mode < 2 ? 1 : 0;
        persistSetError = mode == 1;
        commitError = mode >= 2 ? 1 : 0;
        persistCommitError = mode == 3;
        Esp32DeviceIdentityStore store;
        TEST_ASSERT_TRUE(store.begin());
        ModelIdentityEntropy entropy;
        DeviceIdentityOwner owner(store, entropy);
        identityOutcome(DeviceIdentityOutcome::Uncertain, owner.begin());
        unavailableIdentity(owner);
        TEST_ASSERT_TRUE(handles.empty());
        TEST_ASSERT_EQUAL_UINT32(1, sets);
        TEST_ASSERT_EQUAL_UINT32(mode < 2 ? 0 : 1, commits);
        identityOutcome(DeviceIdentityOutcome::Uncertain, owner.begin());
        TEST_ASSERT_EQUAL_UINT32(1, sets);
    }
}
void test_device_identity_actual_nvs_freezes_record_and_serializes_other_instance() {
    using namespace host_identity_nvs;
    reset();
    Esp32DeviceIdentityStore first, second;
    TEST_ASSERT_TRUE(first.begin());
    TEST_ASSERT_TRUE(second.begin());
    auto input = publicRecord();
    const auto original = input;
    caller = &input;
    nestedStore = &second;
    beforeSet = [] {
        caller->bytes[8] ^= 1;
        DeviceIdentityRecord unchanged = publicRecord(99);
        const auto old = unchanged;
        nestedResult = nestedStore->read(unchanged);
        recordRead(IdentityRecordRead::Failure, nestedResult);
        TEST_ASSERT_TRUE(unchanged == old);
    };
    recordCreate(IdentityRecordCreate::Confirmed, first.createIfMissing(input));
    DeviceIdentityRecord result;
    recordRead(IdentityRecordRead::Present, second.read(result));
    TEST_ASSERT_TRUE(result == original);
    TEST_ASSERT_FALSE(result == input);
    recordCreate(IdentityRecordCreate::AlreadyPresent, second.createIfMissing(publicRecord(42)));
    recordRead(IdentityRecordRead::Present, second.read(result));
    TEST_ASSERT_TRUE(result == original);
    TEST_ASSERT_EQUAL_UINT32(1, sets);
    TEST_ASSERT_EQUAL_UINT32(1, commits);
    TEST_ASSERT_TRUE(handles.empty());
    TEST_ASSERT_FALSE(host_rtos::nonzeroWait);
}
void test_device_identity_startup_entropy_call_order_and_zero_target_preservation() {
    using namespace host_identity_nvs;
    reset();
    Esp32StartupIdentityEntropy entropy;
    Identity identity;
    TEST_ASSERT_TRUE(entropy.make(identity));
    TEST_ASSERT_TRUE(identity == publicLabel());
    TEST_ASSERT_FALSE(entropyActive);
    TEST_ASSERT_FALSE(badEntropyOrder);
    zeroEntropy = true;
    identity = publicLabel(99);
    const auto old = identity;
    TEST_ASSERT_FALSE(entropy.make(identity));
    TEST_ASSERT_TRUE(identity == old);
    TEST_ASSERT_EQUAL_UINT32(2, enables);
    TEST_ASSERT_EQUAL_UINT32(2, fills);
    TEST_ASSERT_EQUAL_UINT32(2, disables);
    TEST_ASSERT_FALSE(badEntropyOrder);
}
void test_public_device_identity_persistence_never_opens_game_authority() {
    using namespace host_identity_nvs;
    reset();
    Esp32DeviceIdentityStore store;
    TEST_ASSERT_TRUE(store.begin());
    ModelIdentityEntropy entropy;
    DeviceIdentityOwner owner(store, entropy);
    identityOutcome(DeviceIdentityOutcome::Created, owner.begin());
    Identity device;
    TEST_ASSERT_TRUE(owner.identity(device));
    FreeRtosGameControlChannel channel;
    TEST_ASSERT_TRUE(channel.begin());
    IdentityAccess policy(channel);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(IdentityAccessOutcome::Initialized),
                      static_cast<unsigned>(policy.begin(device)));
    BoundConnection connection;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(IdentityAccessOutcome::Connected),
                      static_cast<unsigned>(policy.connect(7, connection)));
    TrustedAuthority destination = authority(99);
    const auto old = destination;
    TEST_ASSERT_FALSE(policy.route(connection, destination));
    TEST_ASSERT_TRUE(destination == old);
    VerifiedPeerEvidence claimed;
    claimed.peer = publicLabel(42);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(IdentityAccessOutcome::SecurityRejected),
                      static_cast<unsigned>(policy.authenticate(connection, claimed)));
    TEST_ASSERT_FALSE(policy.route(connection, destination));
    TEST_ASSERT_TRUE(destination == old);
}
