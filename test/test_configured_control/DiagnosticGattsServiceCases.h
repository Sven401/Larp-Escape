#pragma once
struct DiagnosticGattsFixture {
    SourceFixture base;
    larp::diagnostic::DiagnosticGattsService service{base.source, base.clock};
    const std::uint16_t handle = 0x50;
    DiagnosticGattsFixture() {
        host_service_sdk::reset();
        host_sdk_source::currentTask = reinterpret_cast<void *>(3);
        TEST_ASSERT_TRUE(service.begin());
    }
    void worker() {
        host_sdk_source::currentTask = reinterpret_cast<void *>(3);
        service.service(true);
    }
    void callback(esp_gatts_cb_event_t kind, esp_ble_gatts_cb_param_t value,
                  std::uint8_t interface = 1) {
        host_sdk_source::currentTask = reinterpret_cast<void *>(2);
        TEST_ASSERT_TRUE(service.copyEvent(kind, interface, value));
        worker();
    }
    void created() {
        worker();
        TEST_ASSERT_EQUAL(1, host_service_sdk::calls.size());
        esp_ble_gatts_cb_param_t value = {};
        value.create.service_handle = handle;
        value.create.service_id.is_primary = true;
        value.create.service_id.id.uuid.len = ESP_UUID_LEN_128;
        const auto uuid = larp::diagnostic::DiagnosticGattsDefinition::service();
        std::memcpy(value.create.service_id.id.uuid.uuid.uuid128, uuid.data(), uuid.size());
        callback(ESP_GATTS_CREATE_EVT, value);
    }
    void characteristic(std::uint16_t attribute, const std::array<std::uint8_t, 16> &uuid) {
        esp_ble_gatts_cb_param_t value = {};
        value.add_char.service_handle = handle;
        value.add_char.attr_handle = attribute;
        value.add_char.char_uuid.len = ESP_UUID_LEN_128;
        std::memcpy(value.add_char.char_uuid.uuid.uuid128, uuid.data(), uuid.size());
        callback(ESP_GATTS_ADD_CHAR_EVT, value);
    }
    void descriptor(std::uint16_t attribute, std::uint16_t uuid =
                        ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
        esp_ble_gatts_cb_param_t value = {};
        value.add_char_descr.service_handle = handle;
        value.add_char_descr.attr_handle = attribute;
        value.add_char_descr.descr_uuid.len = ESP_UUID_LEN_16;
        value.add_char_descr.descr_uuid.uuid.uuid16 = uuid;
        callback(ESP_GATTS_ADD_CHAR_DESCR_EVT, value);
    }
    void attributes() {
        using larp::diagnostic::DiagnosticGattsDefinition;
        created();
        characteristic(0x52, DiagnosticGattsDefinition::controlWrite());
        characteristic(0x54, DiagnosticGattsDefinition::controlReply());
        descriptor(0x55);
        characteristic(0x57, DiagnosticGattsDefinition::stepWrite());
        characteristic(0x59, DiagnosticGattsDefinition::stepReply());
        descriptor(0x5a);
    }
};
void test_diagnostic_gatts_exact_sdk_shape_and_closed_install_before_start() {
    DiagnosticGattsFixture sdk;
    sdk.attributes();
    using larp::diagnostic::DiagnosticGattsDefinition;
    TEST_ASSERT_EQUAL(7, host_service_sdk::calls.size());
    TEST_ASSERT_EQUAL(11, host_service_sdk::calls[0].handles);
    TEST_ASSERT_EQUAL_MEMORY(DiagnosticGattsDefinition::service().data(),
                             host_service_sdk::calls[0].uuid.data(), 16);
    const unsigned kinds[] = {1, 2, 2, 4, 2, 2, 4};
    for (std::size_t i = 0; i < 7; ++i)
        TEST_ASSERT_EQUAL(kinds[i], host_service_sdk::calls[i].kind);
    const std::size_t writes[] = {1, 3, 4, 6};
    for (const auto i : writes)
        TEST_ASSERT_EQUAL(ESP_GATT_PERM_WRITE_ENC_MITM,
                          host_service_sdk::calls[i].permission);
    TEST_ASSERT_EQUAL(ESP_GATT_CHAR_PROP_BIT_WRITE, host_service_sdk::calls[1].property);
    TEST_ASSERT_EQUAL(ESP_GATT_CHAR_PROP_BIT_WRITE, host_service_sdk::calls[4].property);
    TEST_ASSERT_EQUAL(ESP_GATT_CHAR_PROP_BIT_NOTIFY, host_service_sdk::calls[2].property);
    TEST_ASSERT_EQUAL(ESP_GATT_CHAR_PROP_BIT_NOTIFY, host_service_sdk::calls[5].property);
    TEST_ASSERT_EQUAL(0, host_service_sdk::calls[2].permission);
    TEST_ASSERT_EQUAL(0, host_service_sdk::calls[5].permission);
    TEST_ASSERT_FALSE(sdk.service.ready());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(larp::diagnostic::DiagnosticGattsPhase::Installing),
                      static_cast<unsigned>(sdk.service.phase()));
    host_sdk_source::currentTask = reinterpret_cast<void *>(1);
    host_sdk_source::drainOne();
    sdk.worker();
    TEST_ASSERT_EQUAL(8, host_service_sdk::calls.size());
    TEST_ASSERT_EQUAL(3, host_service_sdk::calls.back().kind);
    TEST_ASSERT_FALSE(sdk.service.ready());
    esp_ble_gatts_cb_param_t started = {};
    started.start.service_handle = sdk.handle;
    sdk.callback(ESP_GATTS_START_EVT, started);
    TEST_ASSERT_TRUE(sdk.service.ready());
    const auto handles = sdk.service.handles();
    TEST_ASSERT_TRUE(handles.complete());
    TEST_ASSERT_EQUAL_HEX16(0x55, handles.controlCccd);
    TEST_ASSERT_EQUAL_HEX16(0x5a, handles.stepCccd);
}
void test_diagnostic_gatts_bad_callback_and_generation_fail_closed() {
    DiagnosticGattsFixture sdk;
    sdk.created();
    sdk.characteristic(0x52, larp::diagnostic::DiagnosticGattsDefinition::stepWrite());
    TEST_ASSERT_EQUAL(static_cast<unsigned>(larp::diagnostic::DiagnosticGattsPhase::Failed),
                      static_cast<unsigned>(sdk.service.phase()));
    TEST_ASSERT_FALSE(sdk.service.ready());
    TEST_ASSERT_EQUAL(2, host_service_sdk::calls.size());
    DiagnosticGattsFixture other;
    other.worker();
    esp_ble_gatts_cb_param_t value = {};
    value.create.service_handle = 0x50;
    value.create.service_id.is_primary = true;
    value.create.service_id.id.uuid.len = ESP_UUID_LEN_128;
    const auto uuid = larp::diagnostic::DiagnosticGattsDefinition::service();
    std::memcpy(value.create.service_id.id.uuid.uuid.uuid128, uuid.data(), uuid.size());
    host_sdk_source::currentTask = reinterpret_cast<void *>(2);
    TEST_ASSERT_FALSE(other.service.copyEvent(ESP_GATTS_CREATE_EVT, 7, value));
    other.worker();
    TEST_ASSERT_EQUAL(static_cast<unsigned>(larp::diagnostic::DiagnosticGattsPhase::Failed),
                      static_cast<unsigned>(other.service.phase()));
    TEST_ASSERT_FALSE(other.service.ready());
}
