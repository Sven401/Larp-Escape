#pragma once
#include "DiagnosticGattsDefinition.h"
#include "../GattsWriteAdmission/Esp32GattsSource.h"
#include <atomic>
#include <esp_gatts_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace larp {
namespace diagnostic {
enum class DiagnosticGattsPhase : std::uint8_t {
    Idle, Creating, ControlWrite, ControlReply, ControlCccd, StepWrite,
    StepReply, StepCccd,
#if defined(LARP_DIAGNOSTIC_TEST_FIRMWARE)
    DevelopmentInfo, DevelopmentWrite, DevelopmentReply, DevelopmentCccd,
#endif
    Installing, Starting, Ready, Failed
};
enum class DiagnosticGattsEventKind : std::uint8_t {
    Other, Created, Characteristic, Descriptor, Started
};
struct DiagnosticGattsEvent {
    DiagnosticGattsEventKind kind = DiagnosticGattsEventKind::Other;
    companion::GattsSourceRegistration original;
    std::array<std::uint8_t, 16> uuid = {};
    std::uint16_t service = 0, attribute = 0, uuid16 = 0;
    std::uint32_t receivedAt = 0;
    bool success = false, primary = false, uuid128 = false;
    std::uint8_t instance = 0;
};
// All SDK callback data is copied before leaving the callback. The service is
// created only after the legacy game service is ready and before advertising.
class DiagnosticGattsService {
  public:
    DiagnosticGattsService(companion::Esp32GattsSource &source,
                           const companion::IProfileImportClock &clock)
        : source_(source), clock_(clock) {}
    bool begin() noexcept;
    void service(bool gameServiceReady) noexcept;
    bool owns(esp_gatts_cb_event_t, const esp_ble_gatts_cb_param_t &) const noexcept;
    bool copyEvent(esp_gatts_cb_event_t, esp_gatt_if_t,
                   const esp_ble_gatts_cb_param_t &) noexcept;
    bool ready() const noexcept;
    DiagnosticGattsPhase phase() const noexcept;
    DiagnosticClosedHandles handles() const noexcept;

  private:
    static const std::size_t Depth = 8;
    companion::Esp32GattsSource &source_;
    const companion::IProfileImportClock &clock_;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t control_ = {};
    alignas(DiagnosticGattsEvent) std::uint8_t storage_[Depth * sizeof(DiagnosticGattsEvent)] = {};
    mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    DiagnosticClosedHandles handles_;
    std::atomic<DiagnosticGattsPhase> phase_{DiagnosticGattsPhase::Idle};
    std::atomic<std::uint16_t> publishedService_{0};
    TaskHandle_t worker_ = nullptr, producer_ = nullptr;
    std::uint32_t startedAt_ = 0;
    std::atomic<bool> broken_{false};
    bool busy_ = false;
    void fail() noexcept;
    bool postNext() noexcept;
};
} // namespace diagnostic
} // namespace larp
