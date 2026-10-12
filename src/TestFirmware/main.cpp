#include <Arduino.h>
#include <new>
#include <type_traits>

#include "Esp32CompanionIdentityOwner.h"
#include "ProductGattsOwner.h"
#include "CompanionIntegration/DiagnosticDevelopmentAdmission.h"
#include "CompanionIntegration/DiagnosticDevelopmentControl.h"
#include "CompanionIntegration/DiagnosticRawExpanderProbe.h"
#include "RawExpanderWire.h"
#include "DevelopmentLedDeadline.h"

#if !defined(LARP_DIAGNOSTIC_TEST_FIRMWARE) || !defined(LARP_PRODUCT_GATTS_CLOSED)
#error "This entry point requires the isolated diagnostic test-firmware build"
#endif

// Development image: the proven BLE/GATT owner retains its original device
// identity and pairing path. The separate development control owner can only
// grant a test session; no game or component driver is constructed here.
Esp32CompanionIdentityOwner identity;
ProductGattsOwner gatts(identity);
static std::aligned_storage<sizeof(larp::diagnostic::DevelopmentAdmission),
                            alignof(larp::diagnostic::DevelopmentAdmission)>::type admissionStorage;
static larp::diagnostic::DevelopmentAdmission *admission = nullptr;
static std::aligned_storage<sizeof(larp::diagnostic::DevelopmentControl),
                            alignof(larp::diagnostic::DevelopmentControl)>::type controlStorage;
DevelopmentLedDeadline ledDeadline;
bool ledDeadlineStarted = false;

void setup() {
    Serial.begin(115200);
    Serial.println("LARP_DIAGNOSTIC_TEST_FIRMWARE development image; game disabled");
    const auto identityState = identity.begin();
    Serial.printf("TEST_IDENTITY state=%u\n", static_cast<unsigned>(identityState));
    if (identityState != Esp32CompanionIdentityOwner::State::ReadyClosed) {
        Serial.println("TEST_BLE unavailable: identity startup failed");
        return;
    }
    larp::companion::Identity device;
    larp::companion::BootIdentity boot;
    if (!identity.identity(device) || !identity.bootIdentity(boot) || !boot.valid()) {
        Serial.println("TEST_BLE unavailable: device or boot identity missing");
        return;
    }
    const std::uint64_t bootValue =
        (static_cast<std::uint64_t>(boot.high) << 32) | boot.low;
    admission = new (&admissionStorage)
        larp::diagnostic::DevelopmentAdmission(device, bootValue);
    auto *control = new (&controlStorage)
        larp::diagnostic::DevelopmentControl(*admission, device, bootValue);
    if (!gatts.configureDevelopmentControl(*control)) {
        Serial.println("TEST_BLE unavailable: development control configuration failed");
        return;
    }
    const auto state = gatts.begin();
    Serial.printf("TEST_BLE startup=%u product_writes=closed development_control=configured\n",
                  static_cast<unsigned>(state));
    ledDeadlineStarted = ledDeadline.begin();
    Serial.printf("TEST_LED deadline_ready=%u output=closed\n", ledDeadlineStarted);
}

void loop() {
    larp::companion::Esp32GattsSource::DevelopmentRawWork work;
    if (gatts.takeDevelopmentRawWork(work)) {
        static bool busAttempted = false, busReady = false;
        if (!busAttempted) {
            busAttempted = true;
            busReady = Wire.begin();
            if (busReady)
                Wire.setTimeOut(25);
        }
        larp::companion::Esp32GattsSource::DevelopmentRawResult result;
        result.work = work;
        result.startedAtMs = millis();
        if (busReady) {
            static RawExpanderWireReader reader(Wire);
            static larp::diagnostic::RawExpanderProbe probe(reader);
            result.observation = probe.sample();
        } else {
            result.observation.address = 0x20;
            result.observation.reg = 0x13;
            result.observation.bitIndex = 6;
            result.observation.status =
                larp::diagnostic::RawRegisterStatus::RegisterSelectFailed;
            result.observation.busCode = 0xfe; // I2C controller did not start.
        }
        result.endedAtMs = millis();
        if (!gatts.postDevelopmentRawResult(result))
            Serial.println("TEST_RAW_RESULT delivery_unavailable");
    }
    static unsigned long lastReport = 0;
    const auto now = millis();
    if (now - lastReport >= 10000UL) {
        lastReport = now;
        Serial.printf("TEST_STATUS ble_service=%u diagnostic_service=%u registry=%u "
                      "advertising=%u led_deadline=%u led_fault=%u game=disabled component_actions=closed\n",
                      gatts.serviceReady(), gatts.diagnosticServiceReady(),
                      static_cast<unsigned>(gatts.registryState()),
                      static_cast<unsigned>(gatts.advertisingState()),
                      ledDeadlineStarted, ledDeadline.failed());
        if (admission)
            Serial.printf("TEST_ADMISSION boot=%08lX%08lX path=development-control\n",
                          static_cast<unsigned long>(admission->boot() >> 32),
                          static_cast<unsigned long>(admission->boot()));
    }
    delay(10);
}
