#include <Arduino.h>
#include <new>
#include <type_traits>

#include "Esp32CompanionIdentityOwner.h"
#include "ProductGattsOwner.h"
#include "CompanionIntegration/DiagnosticDevelopmentAdmission.h"

#if !defined(LARP_DIAGNOSTIC_TEST_FIRMWARE) || !defined(LARP_PRODUCT_GATTS_CLOSED)
#error "This entry point requires the isolated diagnostic test-firmware build"
#endif

// Development image: the proven BLE/GATT owner retains its original device
// identity and pairing path. No game, component driver, or diagnostic write
// owner is constructed here. A BLE notification or OS bond is not a hardware
// action or diagnostic authority.
Esp32CompanionIdentityOwner identity;
ProductGattsOwner gatts(identity);
static std::aligned_storage<sizeof(larp::diagnostic::DevelopmentAdmission),
                            alignof(larp::diagnostic::DevelopmentAdmission)>::type admissionStorage;
static larp::diagnostic::DevelopmentAdmission *admission = nullptr;

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
    const auto state = gatts.begin();
    Serial.printf("TEST_BLE startup=%u diagnostic_writes=closed\n",
                  static_cast<unsigned>(state));
}

void loop() {
    static unsigned long lastReport = 0;
    const auto now = millis();
    if (now - lastReport >= 10000UL) {
        lastReport = now;
        Serial.printf("TEST_STATUS ble_service=%u diagnostic_service=%u registry=%u "
                      "advertising=%u game=disabled actions=closed\n",
                      gatts.serviceReady(), gatts.diagnosticServiceReady(),
                      static_cast<unsigned>(gatts.registryState()),
                      static_cast<unsigned>(gatts.advertisingState()));
        if (admission)
            Serial.printf("TEST_ADMISSION boot=%08lX%08lX active=%u path=closed\n",
                          static_cast<unsigned long>(admission->boot() >> 32),
                          static_cast<unsigned long>(admission->boot()), admission->active());
    }
    delay(10);
}
