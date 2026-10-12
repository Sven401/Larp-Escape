#include "DiagnosticGattsService.h"
#include <cstring>

namespace larp {
namespace diagnostic {
namespace {
esp_bt_uuid_t sdkUuid(const std::array<std::uint8_t, 16> &bytes) noexcept {
    esp_bt_uuid_t result = {};
    result.len = ESP_UUID_LEN_128;
    std::memcpy(result.uuid.uuid128, bytes.data(), bytes.size());
    return result;
}
bool create(std::uint8_t interface) noexcept {
    esp_gatt_srvc_id_t id = {};
    id.id.uuid = sdkUuid(DiagnosticGattsDefinition::service());
    id.id.inst_id = 0;
    id.is_primary = true;
    return esp_ble_gatts_create_service(interface, &id,
                                        DiagnosticGattsDefinition::Handles) == ESP_OK;
}
bool addCharacteristic(std::uint16_t service, const std::array<std::uint8_t, 16> &bytes,
                       bool request) noexcept {
    auto uuid = sdkUuid(bytes);
    esp_attr_value_t value = {};
    value.attr_max_len = 128;
    esp_attr_control_t control = {};
    control.auto_rsp = ESP_GATT_RSP_BY_APP;
    return esp_ble_gatts_add_char(service, &uuid,
                                  request ? ESP_GATT_PERM_WRITE_ENC_MITM : 0,
                                  request ? ESP_GATT_CHAR_PROP_BIT_WRITE
                                          : ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                  &value, &control) == ESP_OK;
}
bool addCccd(std::uint16_t service) noexcept {
    esp_bt_uuid_t uuid = {};
    uuid.len = ESP_UUID_LEN_16;
    uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
    esp_attr_value_t value = {};
    value.attr_max_len = 2;
    esp_attr_control_t control = {};
    control.auto_rsp = ESP_GATT_RSP_BY_APP;
    // A CCCD only prepares the reply transport. It conveys no diagnostic
    // authority; request characteristics retain their independent MITM gate.
    return esp_ble_gatts_add_char_descr(service, &uuid, ESP_GATT_PERM_WRITE_ENCRYPTED,
                                        &value, &control) == ESP_OK;
}
#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
bool addDevelopmentInfo(std::uint16_t service) noexcept {
    auto uuid = sdkUuid(DiagnosticGattsDefinition::developmentInfo());
    esp_attr_value_t value = {};
    value.attr_max_len = 27;
    esp_attr_control_t control = {};
    control.auto_rsp = ESP_GATT_RSP_BY_APP;
    return esp_ble_gatts_add_char(service, &uuid, ESP_GATT_PERM_READ_ENCRYPTED,
                                  ESP_GATT_CHAR_PROP_BIT_READ, &value, &control) == ESP_OK;
}
bool addDevelopmentCharacteristic(std::uint16_t service,
                                  const std::array<std::uint8_t, 16> &bytes,
                                  bool request) noexcept {
    auto uuid = sdkUuid(bytes);
    esp_attr_value_t value = {};
    value.attr_max_len = 128;
    esp_attr_control_t control = {};
    control.auto_rsp = ESP_GATT_RSP_BY_APP;
    return esp_ble_gatts_add_char(service, &uuid,
                                  request ? ESP_GATT_PERM_WRITE_ENCRYPTED : 0,
                                  request ? ESP_GATT_CHAR_PROP_BIT_WRITE
                                          : ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                  &value, &control) == ESP_OK;
}
#endif
} // namespace
bool DiagnosticGattsService::begin() noexcept {
    const auto task = xTaskGetCurrentTaskHandle();
    if (!task || (worker_ && worker_ != task))
        return false;
    if (worker_)
        return queue_ && phase() != DiagnosticGattsPhase::Failed;
    worker_ = task;
    queue_ = xQueueCreateStatic(Depth, sizeof(DiagnosticGattsEvent), storage_, &control_);
    return queue_ != nullptr;
}
DiagnosticGattsPhase DiagnosticGattsService::phase() const noexcept {
    return phase_.load(std::memory_order_acquire);
}
DiagnosticClosedHandles DiagnosticGattsService::handles() const noexcept {
    if (phase() != DiagnosticGattsPhase::Ready)
        return {};
    portENTER_CRITICAL(&mutex_);
    const auto value = handles_;
    portEXIT_CRITICAL(&mutex_);
    return value;
}
bool DiagnosticGattsService::ready() const noexcept {
    if (phase() != DiagnosticGattsPhase::Ready || broken_.load(std::memory_order_acquire))
        return false;
    const auto value = handles();
    return value.complete() &&
           source_.diagnosticClosedSetupInstalled(value);
}
void DiagnosticGattsService::fail() noexcept {
    phase_.store(DiagnosticGattsPhase::Failed, std::memory_order_release);
}
bool DiagnosticGattsService::owns(esp_gatts_cb_event_t kind,
                                  const esp_ble_gatts_cb_param_t &parameter) const noexcept {
    if (kind == ESP_GATTS_CREATE_EVT) {
        const auto &uuid = parameter.create.service_id.id.uuid;
        const auto expected = DiagnosticGattsDefinition::service();
        return uuid.len == ESP_UUID_LEN_128 &&
               std::memcmp(uuid.uuid.uuid128, expected.data(), expected.size()) == 0;
    }
    const auto current = publishedService_.load(std::memory_order_acquire);
    if (!current)
        return false;
    if (kind == ESP_GATTS_ADD_CHAR_EVT)
        return parameter.add_char.service_handle == current;
    if (kind == ESP_GATTS_ADD_CHAR_DESCR_EVT)
        return parameter.add_char_descr.service_handle == current;
    if (kind == ESP_GATTS_START_EVT)
        return parameter.start.service_handle == current;
    return false;
}
bool DiagnosticGattsService::copyEvent(esp_gatts_cb_event_t kind, esp_gatt_if_t interface,
                                       const esp_ble_gatts_cb_param_t &parameter) noexcept {
    if (!queue_ || !owns(kind, parameter))
        return false;
    companion::GattsSourceRegistration original;
    if (!source_.readRegistration(original) || original.interface != interface) {
        broken_ = true;
        return false;
    }
    const auto task = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&mutex_);
    if (!producer_ && task)
        producer_ = task;
    const bool producer = task && producer_ == task;
    portEXIT_CRITICAL(&mutex_);
    if (!producer) {
        broken_ = true;
        return false;
    }
    DiagnosticGattsEvent event;
    event.original = original;
    event.receivedAt = clock_.nowMilliseconds();
    if (kind == ESP_GATTS_CREATE_EVT) {
        event.kind = DiagnosticGattsEventKind::Created;
        event.success = parameter.create.status == ESP_GATT_OK;
        event.service = parameter.create.service_handle;
        event.primary = parameter.create.service_id.is_primary;
        event.instance = parameter.create.service_id.id.inst_id;
        event.uuid128 = true;
        std::memcpy(event.uuid.data(), parameter.create.service_id.id.uuid.uuid.uuid128,
                    event.uuid.size());
    } else if (kind == ESP_GATTS_ADD_CHAR_EVT) {
        event.kind = DiagnosticGattsEventKind::Characteristic;
        event.success = parameter.add_char.status == ESP_GATT_OK;
        event.service = parameter.add_char.service_handle;
        event.attribute = parameter.add_char.attr_handle;
        event.uuid128 = parameter.add_char.char_uuid.len == ESP_UUID_LEN_128;
        if (event.uuid128)
            std::memcpy(event.uuid.data(), parameter.add_char.char_uuid.uuid.uuid128,
                        event.uuid.size());
    } else if (kind == ESP_GATTS_ADD_CHAR_DESCR_EVT) {
        event.kind = DiagnosticGattsEventKind::Descriptor;
        event.success = parameter.add_char_descr.status == ESP_GATT_OK;
        event.service = parameter.add_char_descr.service_handle;
        event.attribute = parameter.add_char_descr.attr_handle;
        if (parameter.add_char_descr.descr_uuid.len == ESP_UUID_LEN_16)
            event.uuid16 = parameter.add_char_descr.descr_uuid.uuid.uuid16;
    } else if (kind == ESP_GATTS_START_EVT) {
        event.kind = DiagnosticGattsEventKind::Started;
        event.success = parameter.start.status == ESP_GATT_OK;
        event.service = parameter.start.service_handle;
    }
    if (xQueueSendToBack(queue_, &event, 0) == pdTRUE)
        return true;
    broken_ = true;
    return false;
}
bool DiagnosticGattsService::postNext() noexcept {
    switch (phase_) {
    case DiagnosticGattsPhase::ControlWrite:
        return addCharacteristic(handles_.service, DiagnosticGattsDefinition::controlWrite(), true);
    case DiagnosticGattsPhase::ControlReply:
        return addCharacteristic(handles_.service, DiagnosticGattsDefinition::controlReply(), false);
    case DiagnosticGattsPhase::ControlCccd:
        return addCccd(handles_.service);
    case DiagnosticGattsPhase::StepWrite:
        return addCharacteristic(handles_.service, DiagnosticGattsDefinition::stepWrite(), true);
    case DiagnosticGattsPhase::StepReply:
        return addCharacteristic(handles_.service, DiagnosticGattsDefinition::stepReply(), false);
    case DiagnosticGattsPhase::StepCccd:
        return addCccd(handles_.service);
#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
    case DiagnosticGattsPhase::DevelopmentInfo:
        return addDevelopmentInfo(handles_.service);
    case DiagnosticGattsPhase::DevelopmentWrite:
        return addDevelopmentCharacteristic(handles_.service,
                                            DiagnosticGattsDefinition::developmentWrite(), true);
    case DiagnosticGattsPhase::DevelopmentReply:
        return addDevelopmentCharacteristic(handles_.service,
                                            DiagnosticGattsDefinition::developmentReply(), false);
    case DiagnosticGattsPhase::DevelopmentCccd:
        return addCccd(handles_.service);
#endif
    default:
        return false;
    }
}
void DiagnosticGattsService::service(bool gameServiceReady) noexcept {
    if (busy_ || !queue_ || worker_ != xTaskGetCurrentTaskHandle() ||
        phase_ == DiagnosticGattsPhase::Failed || phase_ == DiagnosticGattsPhase::Ready)
        return;
    busy_ = true;
    if (broken_ || (phase_ != DiagnosticGattsPhase::Idle &&
                    clock_.nowMilliseconds() - startedAt_ >=
                        DiagnosticGattsDefinition::SetupDeadlineMilliseconds)) {
        fail();
        busy_ = false;
        return;
    }
    if (phase_ == DiagnosticGattsPhase::Idle) {
        if (gameServiceReady) {
            companion::GattsSourceRegistration original;
            if (!source_.readRegistration(original) || !create(original.interface))
                fail();
            else {
                handles_.original = original;
                startedAt_ = clock_.nowMilliseconds();
                phase_ = DiagnosticGattsPhase::Creating;
            }
        }
        busy_ = false;
        return;
    }
    DiagnosticGattsEvent event;
    if (xQueueReceive(queue_, &event, 0) == pdTRUE) {
        companion::GattsSourceRegistration current;
        if (!source_.readRegistration(current) || !(current == event.original) ||
            !(current == handles_.original) || !event.success ||
            clock_.nowMilliseconds() - event.receivedAt >=
                DiagnosticGattsDefinition::SetupDeadlineMilliseconds) {
            fail();
            busy_ = false;
            return;
        }
        if (phase_ == DiagnosticGattsPhase::Creating &&
            event.kind == DiagnosticGattsEventKind::Created && event.uuid128 &&
            event.uuid == DiagnosticGattsDefinition::service() && event.primary &&
            !event.instance && event.service &&
            static_cast<std::uint32_t>(event.service) + DiagnosticGattsDefinition::Handles - 1 <=
                UINT16_MAX) {
            handles_.service = event.service;
            publishedService_.store(event.service, std::memory_order_release);
            phase_ = DiagnosticGattsPhase::ControlWrite;
        } else if (event.service == handles_.service && event.attribute > handles_.service &&
                   static_cast<std::uint32_t>(event.attribute) <
                       static_cast<std::uint32_t>(handles_.service) +
                           DiagnosticGattsDefinition::Handles) {
            std::uint16_t *destination = nullptr;
            std::uint16_t previous = 0;
            bool expected = false;
            switch (phase_) {
            case DiagnosticGattsPhase::ControlWrite:
                destination = &handles_.controlWrite; previous = handles_.service;
                expected = event.kind == DiagnosticGattsEventKind::Characteristic &&
                           event.uuid128 && event.uuid == DiagnosticGattsDefinition::controlWrite();
                phase_ = DiagnosticGattsPhase::ControlReply; break;
            case DiagnosticGattsPhase::ControlReply:
                destination = &handles_.controlReply; previous = handles_.controlWrite;
                expected = event.kind == DiagnosticGattsEventKind::Characteristic &&
                           event.uuid128 && event.uuid == DiagnosticGattsDefinition::controlReply();
                phase_ = DiagnosticGattsPhase::ControlCccd; break;
            case DiagnosticGattsPhase::ControlCccd:
                destination = &handles_.controlCccd; previous = handles_.controlReply;
                expected = event.kind == DiagnosticGattsEventKind::Descriptor &&
                           event.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                phase_ = DiagnosticGattsPhase::StepWrite; break;
            case DiagnosticGattsPhase::StepWrite:
                destination = &handles_.stepWrite; previous = handles_.controlCccd;
                expected = event.kind == DiagnosticGattsEventKind::Characteristic &&
                           event.uuid128 && event.uuid == DiagnosticGattsDefinition::stepWrite();
                phase_ = DiagnosticGattsPhase::StepReply; break;
            case DiagnosticGattsPhase::StepReply:
                destination = &handles_.stepReply; previous = handles_.stepWrite;
                expected = event.kind == DiagnosticGattsEventKind::Characteristic &&
                           event.uuid128 && event.uuid == DiagnosticGattsDefinition::stepReply();
                phase_ = DiagnosticGattsPhase::StepCccd; break;
            case DiagnosticGattsPhase::StepCccd:
                destination = &handles_.stepCccd; previous = handles_.stepReply;
                expected = event.kind == DiagnosticGattsEventKind::Descriptor &&
                           event.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
                phase_ = DiagnosticGattsPhase::DevelopmentInfo; break;
            case DiagnosticGattsPhase::DevelopmentInfo:
                destination = &handles_.developmentInfo; previous = handles_.stepCccd;
                expected = event.kind == DiagnosticGattsEventKind::Characteristic &&
                           event.uuid128 && event.uuid == DiagnosticGattsDefinition::developmentInfo();
                phase_ = DiagnosticGattsPhase::DevelopmentWrite; break;
            case DiagnosticGattsPhase::DevelopmentWrite:
                destination = &handles_.developmentWrite; previous = handles_.developmentInfo;
                expected = event.kind == DiagnosticGattsEventKind::Characteristic &&
                           event.uuid128 && event.uuid == DiagnosticGattsDefinition::developmentWrite();
                phase_ = DiagnosticGattsPhase::DevelopmentReply; break;
            case DiagnosticGattsPhase::DevelopmentReply:
                destination = &handles_.developmentReply; previous = handles_.developmentWrite;
                expected = event.kind == DiagnosticGattsEventKind::Characteristic &&
                           event.uuid128 && event.uuid == DiagnosticGattsDefinition::developmentReply();
                phase_ = DiagnosticGattsPhase::DevelopmentCccd; break;
            case DiagnosticGattsPhase::DevelopmentCccd:
                destination = &handles_.developmentCccd; previous = handles_.developmentReply;
                expected = event.kind == DiagnosticGattsEventKind::Descriptor &&
                           event.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
#endif
                phase_ = DiagnosticGattsPhase::Installing; break;
            default: break;
            }
            if (!destination || !expected || event.attribute <= previous) {
                fail(); busy_ = false; return;
            }
            *destination = event.attribute;
        } else if (phase_ == DiagnosticGattsPhase::Starting &&
                   event.kind == DiagnosticGattsEventKind::Started &&
                   event.service == handles_.service &&
                   source_.diagnosticClosedSetupInstalled(handles_)) {
            phase_ = DiagnosticGattsPhase::Ready;
            busy_ = false;
            return;
        } else {
            fail(); busy_ = false; return;
        }
        if (phase_ == DiagnosticGattsPhase::Installing) {
            if (!handles_.complete() || !source_.postDiagnosticClosedSetup(handles_))
                fail();
        } else if (!postNext())
            fail();
        busy_ = false;
        return;
    }
    if (phase_ == DiagnosticGattsPhase::Installing &&
        source_.diagnosticClosedSetupInstalled(handles_)) {
        phase_ = DiagnosticGattsPhase::Starting;
        if (esp_ble_gatts_start_service(handles_.service) != ESP_OK)
            fail();
    }
    busy_ = false;
}
} // namespace diagnostic
} // namespace larp
