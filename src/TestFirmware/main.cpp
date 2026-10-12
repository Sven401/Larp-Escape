#include <Arduino.h>

#include "Esp32CompanionIdentityOwner.h"
#include "ProductGattsOwner.h"

#if !defined(LARP_DIAGNOSTIC_TEST_FIRMWARE) || !defined(LARP_PRODUCT_GATTS_CLOSED)
#error "This entry point requires the isolated diagnostic test-firmware build"
#endif

// Development image: the proven BLE/GATT owner retains its original device
// identity and pairing path. No game, component driver, or diagnostic write
// owner is constructed here. A BLE notification or OS bond is not a hardware
// action or diagnostic authority.
Esp32CompanionIdentityOwner identity;
ProductGattsOwner gatts(identity);

void setup() {
    Serial.begin(115200);
    Serial.println("LARP_DIAGNOSTIC_TEST_FIRMWARE development image; game disabled");
    const auto identityState = identity.begin();
    Serial.printf("TEST_IDENTITY state=%u\n", static_cast<unsigned>(identityState));
    if (identityState != Esp32CompanionIdentityOwner::State::ReadyClosed) {
        Serial.println("TEST_BLE unavailable: identity startup failed");
        return;
    }
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
    }
    delay(10);
}
