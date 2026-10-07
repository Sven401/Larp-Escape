#pragma once
class ServiceClock : public IProfileImportClock {
  public:
    mutable void (*before)() = nullptr;
    std::uint32_t nowMilliseconds() const noexcept override {
        if (before) {
            auto hook = before;
            before = nullptr;
            hook();
        }
        return f->now;
    }
};
struct GattsServiceFixture {
    const std::uint32_t startedAt = f->now;
    IdentityAccess identity{f->channel.actual};
    ServiceClock clock;
    FreeRtosCommandFrameQueue commands{f->channel.actual};
    FreeRtosProfileFrameQueue profiles{f->channel.actual};
    Esp32GattsSource source{identity, clock, f->channel.actual, commands, profiles};
    FreeRtosGattsResponseQueue responses{source};
    SdkGattsServiceDriver driver;
    Esp32GattsService owner{source, responses, clock, driver};
    GattsServiceFixture() {
        host_sdk_source::reset();
        host_service_sdk::reset();
        btu_thread = reinterpret_cast<void *>(2);
        host_sdk_source::currentTask = reinterpret_cast<void *>(1);
        TEST_ASSERT_TRUE(commands.begin() && profiles.begin() && responses.begin());
        accessOutcome(IdentityAccessOutcome::Initialized, identity.begin(testIdentity(1)));
        TEST_ASSERT_TRUE(source.registeredSourceNow(1));
        host_sdk_source::currentTask = reinterpret_cast<void *>(3);
        TEST_ASSERT_TRUE(owner.begin());
    }
    ~GattsServiceFixture() {
        host_sdk_source::currentTask = reinterpret_cast<void *>(1);
        btu_thread = reinterpret_cast<void *>(2);
        host_sdk_source::drain(); // All synthetic callbacks end while objects live.
    }
    void worker() {
        host_sdk_source::currentTask = reinterpret_cast<void *>(3);
        owner.service();
        ++f->now;
    }
    GattsServiceCopy copy(esp_gatts_cb_event_t kind, esp_ble_gatts_cb_param_t parameter) {
        host_sdk_source::currentTask = reinterpret_cast<void *>(2);
        const auto event = fromSdkGattsService(kind, 1, parameter);
        const auto result = owner.copyEvent(event);
        std::memset(&parameter, 0, sizeof(parameter));
        return result;
    }
    void registered() {
        esp_ble_gatts_cb_param_t p = {};
        p.reg.app_id = GattsServiceDefinition::Application;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Copied),
                          static_cast<unsigned>(copy(ESP_GATTS_REG_EVT, p)));
        worker();
    }
    esp_ble_gatts_cb_param_t createdParameter() {
        esp_ble_gatts_cb_param_t p = {};
        p.create.service_handle = 0x20;
        p.create.service_id.is_primary = true;
        p.create.service_id.id.uuid.len = 16;
        const auto uuid = GattsServiceDefinition::service();
        std::memcpy(p.create.service_id.id.uuid.uuid.uuid128, uuid.data(), uuid.size());
        return p;
    }
    void created() {
        copy(ESP_GATTS_CREATE_EVT, createdParameter());
        worker();
    }
    esp_ble_gatts_cb_param_t attributeParameter(bool profile = false) {
        esp_ble_gatts_cb_param_t p = {};
        p.add_char.service_handle = 0x20;
        p.add_char.attr_handle = profile ? 0x24 : 0x22;
        p.add_char.char_uuid.len = 16;
        const auto uuid =
            profile ? GattsServiceDefinition::profile() : GattsServiceDefinition::command();
        std::memcpy(p.add_char.char_uuid.uuid.uuid128, uuid.data(), uuid.size());
        return p;
    }
    void command() {
        copy(ESP_GATTS_ADD_CHAR_EVT, attributeParameter());
        worker();
    }
    void profile() {
        copy(ESP_GATTS_ADD_CHAR_EVT, attributeParameter(true));
        worker();
    }
    void pending() {
        registered();
        created();
        command();
        profile();
    }
    void install() {
        host_sdk_source::currentTask = reinterpret_cast<void *>(1);
        host_sdk_source::drainOne();
    }
    void started() {
        esp_ble_gatts_cb_param_t p = {};
        p.start.service_handle = 0x20;
        copy(ESP_GATTS_START_EVT, p);
        worker();
    }
    void ready() {
        pending();
        install();
        worker();
        started();
        TEST_ASSERT_TRUE(owner.ready());
    }
    void phase(GattsServicePhase expected) {
        TEST_ASSERT_EQUAL(static_cast<unsigned>(expected),
                          static_cast<unsigned>(owner.snapshot().phase));
    }
    GattsEndpointSetup job() {
        GattsEndpointSetup value;
        TEST_ASSERT_TRUE(source.readRegistration(value.original));
        value.receivedAt = f->now;
        value.service = 0x20;
        value.command = 0x22;
        value.profile = 0x24;
        return value;
    }
};
void test_service_actual_sdk_callbacks_original_installation_start_and_game() {
    GattsServiceFixture sdk;
    sdk.pending();
    sdk.phase(GattsServicePhase::Installing);
    TEST_ASSERT_FALSE(sdk.owner.ready());
    TEST_ASSERT_EQUAL(3, host_service_sdk::calls.size());
    TEST_ASSERT_EQUAL(1, host_sdk_source::work.size());
    const auto setup = sdk.owner.snapshot().setup;
    TEST_ASSERT_EQUAL_HEX16(0x20, setup.service);
    TEST_ASSERT_EQUAL_HEX16(0x22, setup.command);
    TEST_ASSERT_EQUAL_HEX16(0x24, setup.profile);
    sdk.install();
    sdk.worker();
    sdk.phase(GattsServicePhase::Starting);
    TEST_ASSERT_FALSE(sdk.owner.ready());
    sdk.started();
    TEST_ASSERT_TRUE(sdk.owner.ready());
    TEST_ASSERT_EQUAL(4, host_service_sdk::calls.size());
    const auto &create = host_service_sdk::calls[0];
    TEST_ASSERT_EQUAL(1, create.kind);
    TEST_ASSERT_EQUAL(1, create.interface);
    TEST_ASSERT_EQUAL(5, create.handles);
    TEST_ASSERT_TRUE(create.uuid == GattsServiceDefinition::service());
    for (unsigned i = 1; i < 3; ++i) {
        const auto &call = host_service_sdk::calls[i];
        TEST_ASSERT_EQUAL(2, call.kind);
        TEST_ASSERT_EQUAL_HEX16(0x20, call.service);
        TEST_ASSERT_EQUAL(128, call.maximum);
        TEST_ASSERT_EQUAL(64, call.permission);
        TEST_ASSERT_EQUAL(8, call.property);
        TEST_ASSERT_EQUAL(0, call.response);
    }
    TEST_ASSERT_TRUE(host_service_sdk::calls[1].uuid == GattsServiceDefinition::command());
    TEST_ASSERT_TRUE(host_service_sdk::calls[2].uuid == GattsServiceDefinition::profile());
    TrustedAuthority closed;
    closed.principal = 99;
    TEST_ASSERT_FALSE(f->channel.actual.readAuthority(closed));
    TEST_ASSERT_EQUAL_UINT32(99, closed.principal);
    host_sdk_source::currentTask = reinterpret_cast<void *>(1);
    const auto connectionEvent = lifecycleEvent(0x101);
    lifecycleOutcome(GattsLifecycleOutcome::Opened,
                     sdk.source.connectionNow(1, connectionEvent.peerAddress.data(), 0x101, true));
    BoundConnection connection;
    TEST_ASSERT_TRUE(sdk.source.originalConnectionNow(connection));
    TEST_ASSERT_TRUE(f->channel.actual.readAuthority(closed));
    TEST_ASSERT_FALSE(closed.gameMaster);
    TEST_ASSERT_EQUAL_UINT32(0, closed.principal);
    accessOutcome(IdentityAccessOutcome::Assigned,
                  sdk.identity.assignGameMaster(testIdentity(2), 7));
    accessOutcome(IdentityAccessOutcome::Verified,
                  sdk.identity.authenticate(connection, testVerifiedPeer()));
    f->control.service(++f->now);
    CommandStream stream(f->channel.actual);
    GattsResponseDispatcher worker(sdk.responses, sdk.source);
    const auto document = json(f->command());
    unsigned copies = 0;
    for (std::size_t offset = 0; offset < document.size();) {
        const auto count = std::min<std::size_t>(12, document.size() - offset);
        const auto bytes = frameBytes(document, offset, count);
        sdk446::Request request = {};
        request.write.handle = setup.command;
        request.write.needResponse = true;
        request.write.length = static_cast<std::uint16_t>(bytes.size());
        std::memcpy(request.write.value, bytes.data(), bytes.size());
        TEST_ASSERT_TRUE(sdk.source.requestNow(0x101, copies + 1, 2, &request));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsResponseDispatch::Submitted),
                          static_cast<unsigned>(worker.service()));
        host_sdk_source::drainOne();
        TEST_ASSERT_TRUE(dispatchCommandFrame(sdk.commands, stream, f->now).received);
        offset += count;
        ++copies;
    }
    f->control.service(++f->now);
    TEST_ASSERT_TRUE(f->application.gameRunning());
    TEST_ASSERT_TRUE(sdk.owner.ready());
    char observation[512];
    const int size = std::snprintf(
        observation, sizeof(observation),
        "{\"scope\":\"host_service_sdk_model\",\"sdkRequests\":%u,\"sourcePosts\":%u,"
        "\"service\":32,\"command\":34,\"profile\":36,\"ready\":true,"
        "\"frameCopies\":%u,\"running\":true,\"rfInitialized\":false,\"deviceMeasured\":false}\n",
        unsigned(host_service_sdk::calls.size()), host_sdk_source::posts, copies);
    TEST_ASSERT_TRUE(size > 0 && std::size_t(size) < sizeof(observation));
    if (std::getenv("LARP_EXPORT_GATT_SERVICE_FIXTURE")) {
        auto *output =
            std::fopen("docs/companion/fixtures/gatts/service-setup-observation.json", "wb");
        TEST_ASSERT_NOT_NULL(output);
        TEST_ASSERT_EQUAL(size, std::fwrite(observation, 1, size, output));
        TEST_ASSERT_EQUAL(0, std::fclose(output));
    }
    sdkFixture("service-setup-observation.json", std::string(observation, size));
    lifecycleOutcome(GattsLifecycleOutcome::Closed,
                     sdk.source.connectionNow(1, connectionEvent.peerAddress.data(), 0x101, false));
}
void test_service_sdk_api_post_is_not_completion_and_duplicates_never_repost() {
    GattsServiceFixture sdk;
    sdk.registered();
    sdk.phase(GattsServicePhase::Creating);
    TEST_ASSERT_FALSE(sdk.owner.ready());
    sdk.registered();
    TEST_ASSERT_EQUAL(1, host_service_sdk::calls.size());
    sdk.created();
    sdk.created();
    TEST_ASSERT_EQUAL(2, host_service_sdk::calls.size());
    sdk.command();
    sdk.command();
    TEST_ASSERT_EQUAL(3, host_service_sdk::calls.size());
    sdk.profile();
    sdk.profile();
    TEST_ASSERT_EQUAL(1, host_sdk_source::posts);
    sdk.install();
    sdk.worker();
    sdk.started();
    sdk.started();
    TEST_ASSERT_TRUE(sdk.owner.ready());
    TEST_ASSERT_EQUAL(4, host_service_sdk::calls.size());
}
void test_service_wrong_uuid_handle_order_instance_primary_and_failed_events() {
    for (unsigned variant = 0; variant < 7; ++variant) {
        GattsServiceFixture sdk;
        sdk.registered();
        auto p = sdk.createdParameter();
        if (variant == 0)
            p.create.service_id.id.uuid.uuid.uuid128[0] ^= 1;
        if (variant == 1)
            p.create.service_id.id.inst_id = 1;
        if (variant == 2)
            p.create.service_id.is_primary = false;
        if (variant == 3)
            p.create.status = 1;
        if (variant == 4)
            p.create.service_handle = 0xfffe;
        if (variant < 5)
            sdk.copy(ESP_GATTS_CREATE_EVT, p);
        else {
            sdk.created();
            auto attr = sdk.attributeParameter();
            if (variant == 5)
                attr.add_char.attr_handle = 0x20;
            if (variant == 6)
                attr.add_char.char_uuid.len = 2;
            sdk.copy(ESP_GATTS_ADD_CHAR_EVT, attr);
        }
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
        TEST_ASSERT_FALSE(sdk.owner.ready());
    }
}
void test_service_local_sdk_post_failures_each_close_without_retry() {
    for (unsigned call = 1; call <= 4; ++call) {
        GattsServiceFixture sdk;
        host_service_sdk::failAt = call;
        sdk.registered();
        if (call > 1)
            sdk.created();
        if (call > 2)
            sdk.command();
        if (call > 3) {
            sdk.profile();
            sdk.install();
            sdk.worker();
        }
        sdk.phase(GattsServicePhase::Failed);
        const auto count = host_service_sdk::calls.size();
        sdk.worker();
        sdk.worker();
        TEST_ASSERT_EQUAL(count, host_service_sdk::calls.size());
        TEST_ASSERT_FALSE(sdk.owner.ready());
    }
}
void test_service_full_callback_queue_and_wrong_producer_close_without_sdk() {
    {
        GattsServiceFixture sdk;
        esp_ble_gatts_cb_param_t p = {};
        p.reg.app_id = GattsServiceDefinition::Application;
        for (unsigned i = 0; i < 4; ++i)
            TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Copied),
                              static_cast<unsigned>(sdk.copy(ESP_GATTS_REG_EVT, p)));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Overflow),
                          static_cast<unsigned>(sdk.copy(ESP_GATTS_REG_EVT, p)));
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
        TEST_ASSERT_TRUE(host_service_sdk::calls.empty());
    }
    {
        GattsServiceFixture sdk;
        esp_ble_gatts_cb_param_t p = {};
        p.reg.app_id = GattsServiceDefinition::Application;
        sdk.copy(ESP_GATTS_REG_EVT, p);
        host_sdk_source::currentTask = reinterpret_cast<void *>(4);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Unavailable),
                          static_cast<unsigned>(
                              sdk.owner.copyEvent(fromSdkGattsService(ESP_GATTS_REG_EVT, 1, p))));
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
        TEST_ASSERT_TRUE(host_service_sdk::calls.empty());
    }
}
void test_service_missing_registration_wrong_interface_and_order_never_start() {
    {
        GattsServiceFixture sdk;
        auto event = GattsServiceEvent{};
        event.kind = GattsServiceEventKind::Registered;
        event.success = true;
        event.application = 7;
        event.interface = 1;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Ignored),
                          static_cast<unsigned>(sdk.owner.copyEvent(event)));
        event.application = GattsServiceDefinition::Application;
        event.interface = 2;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Ignored),
                          static_cast<unsigned>(sdk.owner.copyEvent(event)));
        sdk.worker();
        sdk.phase(GattsServicePhase::Idle);
        TEST_ASSERT_TRUE(host_service_sdk::calls.empty());
        sdk.created();
        sdk.phase(GattsServicePhase::Failed);
    }
    {
        GattsServiceFixture sdk;
        btu_thread = nullptr;
        esp_ble_gatts_cb_param_t p = {};
        p.reg.app_id = GattsServiceDefinition::Application;
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Unavailable),
                          static_cast<unsigned>(sdk.copy(ESP_GATTS_REG_EVT, p)));
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
        TEST_ASSERT_TRUE(host_service_sdk::calls.empty());
    }
}
void test_service_first_deadline_not_refreshed_and_clock_wrap() {
    {
        GattsServiceFixture sdk;
        sdk.registered();
        f->now += 29998;
        sdk.registered();
        sdk.phase(GattsServicePhase::Creating);
        ++f->now;
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
    }
    {
        f->now = UINT32_MAX - 3;
        GattsServiceFixture sdk;
        f->now = 29994;
        sdk.worker();
        sdk.phase(GattsServicePhase::Idle);
        f->now = 29996;
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
    }
}
void test_service_endpoint_original_queue_age_failure_and_connect_before_install() {
    {
        GattsServiceFixture sdk;
        host_sdk_source::rejectPost = true;
        sdk.pending();
        sdk.phase(GattsServicePhase::Failed);
        TEST_ASSERT_EQUAL(3, host_service_sdk::calls.size());
    }
    {
        GattsServiceFixture sdk;
        sdk.pending();
        const auto setup = sdk.owner.snapshot().setup;
        f->now = setup.receivedAt + 5000;
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
        sdk.install();
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupState::Rejected),
                          static_cast<unsigned>(sdk.source.endpointSetupState(setup)));
    }
    {
        GattsServiceFixture sdk;
        sdk.pending();
        host_sdk_source::currentTask = reinterpret_cast<void *>(1);
        const auto event = lifecycleEvent(0x101);
        lifecycleOutcome(GattsLifecycleOutcome::Unavailable,
                         sdk.source.connectionNow(1, event.peerAddress.data(), 0x101, true));
        sdk.install();
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
        TEST_ASSERT_EQUAL(3, host_service_sdk::calls.size());
    }
}
void test_service_callbacks_and_endpoint_job_freeze_before_observation() {
    GattsServiceFixture sdk;
    static GattsServiceEvent event;
    event = {};
    event.kind = GattsServiceEventKind::Registered;
    event.interface = 1;
    event.application = GattsServiceDefinition::Application;
    event.success = true;
    host_sdk_source::currentTask = reinterpret_cast<void *>(2);
    sdk.clock.before = []() { event = {}; };
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Copied),
                      static_cast<unsigned>(sdk.owner.copyEvent(event)));
    sdk.worker();
    TEST_ASSERT_EQUAL(1, host_service_sdk::calls.size());
    static GattsEndpointSetup job;
    job = sdk.job();
    const auto original = job;
    sdk.clock.before = []() {
        job.command = 0x99;
        job.original.token = 0;
    };
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupPost::Posted),
                      static_cast<unsigned>(sdk.source.postEndpointSetup(job, sdk.responses)));
    sdk.install();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupState::Installed),
                      static_cast<unsigned>(sdk.source.endpointSetupState(original)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupState::Unavailable),
                      static_cast<unsigned>(sdk.source.endpointSetupState(job)));
}
void test_service_wrong_worker_reentrant_api_and_current_ready_sdk_instance() {
    GattsServiceFixture sdk;
    esp_ble_gatts_cb_param_t p = {};
    p.reg.app_id = GattsServiceDefinition::Application;
    sdk.copy(ESP_GATTS_REG_EVT, p);
    host_sdk_source::currentTask = reinterpret_cast<void *>(4);
    sdk.owner.service();
    TEST_ASSERT_TRUE(host_service_sdk::calls.empty());
    static Esp32GattsService *reentrant = nullptr;
    reentrant = &sdk.owner;
    host_service_sdk::beforeApi = []() { reentrant->service(); };
    sdk.worker();
    TEST_ASSERT_EQUAL(1, host_service_sdk::calls.size());
    sdk.created();
    sdk.command();
    sdk.profile();
    sdk.install();
    sdk.worker();
    sdk.started();
    TEST_ASSERT_TRUE(sdk.owner.ready());
    btu_thread = reinterpret_cast<void *>(4);
    TEST_ASSERT_FALSE(sdk.owner.ready());
}
void test_endpoint_registration_tokens_cannot_rebind_other_instances() {
    GattsServiceFixture first;
    const auto job = first.job();
    IdentityAccess identity(f->channel.actual);
    Esp32GattsSource sibling(identity, first.clock, f->channel.actual, first.commands,
                             first.profiles);
    host_sdk_source::currentTask = reinterpret_cast<void *>(1);
    TEST_ASSERT_TRUE(sibling.registeredSourceNow(1));
    GattsSourceRegistration reg;
    TEST_ASSERT_TRUE(sibling.readRegistration(reg));
    TEST_ASSERT_TRUE(reg.token != job.original.token);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupPost::Rejected),
                      static_cast<unsigned>(sibling.postEndpointSetup(job, first.responses)));
    TEST_ASSERT_TRUE(host_sdk_source::work.empty());
}
void test_endpoint_fixed_once_slot_duplicate_rejected_retry_and_snapshot_scope() {
    GattsServiceFixture sdk;
    const auto job = sdk.job();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupPost::Posted),
                      static_cast<unsigned>(sdk.source.postEndpointSetup(job, sdk.responses)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupPost::Duplicate),
                      static_cast<unsigned>(sdk.source.postEndpointSetup(job, sdk.responses)));
    auto wrong = job;
    wrong.command = 0x21;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupPost::Rejected),
                      static_cast<unsigned>(sdk.source.postEndpointSetup(wrong, sdk.responses)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupState::Unavailable),
                      static_cast<unsigned>(sdk.source.endpointSetupState(wrong)));
    TEST_ASSERT_EQUAL(1, host_sdk_source::posts);
    sdk.install();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupState::Installed),
                      static_cast<unsigned>(sdk.source.endpointSetupState(job)));
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupPost::Duplicate),
                      static_cast<unsigned>(sdk.source.postEndpointSetup(job, sdk.responses)));
    f->now += 5000;
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupState::Installed),
                      static_cast<unsigned>(sdk.source.endpointSetupState(job)));
}

void test_service_start_confirmation_wrong_failed_late_and_premature() {
    for (unsigned variant = 0; variant < 4; ++variant) {
        GattsServiceFixture sdk;
        sdk.pending();
        if (variant != 3) {
            sdk.install();
            sdk.worker();
        }
        auto p = esp_ble_gatts_cb_param_t{};
        p.start.service_handle = variant == 0 ? 0x21 : 0x20;
        if (variant == 1)
            p.start.status = 1;
        if (variant == 2)
            f->now = sdk.startedAt + 30000;
        sdk.copy(ESP_GATTS_START_EVT, p);
        sdk.worker();
        sdk.phase(GattsServicePhase::Failed);
        TEST_ASSERT_FALSE(sdk.owner.ready());
        TEST_ASSERT_EQUAL(variant == 3 ? 3 : 4, host_service_sdk::calls.size());
    }
}
void test_service_lost_start_confirmation_expires_without_retry() {
    GattsServiceFixture sdk;
    sdk.pending();
    sdk.install();
    sdk.worker();
    sdk.phase(GattsServicePhase::Starting);
    f->now = sdk.startedAt + 29999;
    sdk.worker();
    sdk.phase(GattsServicePhase::Starting);
    f->now = sdk.startedAt + 30000;
    sdk.worker();
    sdk.phase(GattsServicePhase::Failed);
    sdk.started();
    TEST_ASSERT_FALSE(sdk.owner.ready());
    TEST_ASSERT_EQUAL(4, host_service_sdk::calls.size());
}
void test_service_callback_loss_after_ready_immediately_closes_readiness() {
    GattsServiceFixture sdk;
    sdk.ready();
    auto p = esp_ble_gatts_cb_param_t{};
    p.start.service_handle = 0x20;
    for (unsigned i = 0; i < 4; ++i)
        sdk.copy(ESP_GATTS_START_EVT, p);
    TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsServiceCopy::Overflow),
                      static_cast<unsigned>(sdk.copy(ESP_GATTS_START_EVT, p)));
    TEST_ASSERT_FALSE(sdk.owner.ready());
    sdk.worker();
    sdk.phase(GattsServicePhase::Failed);
    TEST_ASSERT_EQUAL(4, host_service_sdk::calls.size());
}
void test_endpoint_failed_post_wrong_task_and_replaced_sdk_never_retry() {
    for (unsigned variant = 0; variant < 3; ++variant) {
        GattsServiceFixture sdk;
        if (!variant)
            host_sdk_source::rejectPost = true;
        const auto job = sdk.job();
        const auto posted = sdk.source.postEndpointSetup(job, sdk.responses);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(variant ? GattsEndpointSetupPost::Posted
                                                        : GattsEndpointSetupPost::Rejected),
                          static_cast<unsigned>(posted));
        if (variant) {
            host_sdk_source::currentTask = reinterpret_cast<void *>(variant == 1 ? 4 : 1);
            if (variant == 2)
                btu_thread = reinterpret_cast<void *>(4);
            host_sdk_source::drainOne();
        }
        btu_thread = reinterpret_cast<void *>(2);
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupState::Rejected),
                          static_cast<unsigned>(sdk.source.endpointSetupState(job)));
        TEST_ASSERT_EQUAL(static_cast<unsigned>(GattsEndpointSetupPost::Rejected),
                          static_cast<unsigned>(sdk.source.postEndpointSetup(job, sdk.responses)));
        TEST_ASSERT_TRUE(host_sdk_source::work.empty());
        TEST_ASSERT_EQUAL(1, host_sdk_source::posts);
    }
}
