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
bool wake2InputReady = false;

// The proven game image configured MCP1/GPB6 as INPUT_PULLUP. Keep every
// other expander pin untouched in this isolated image and read the mode back.
static bool readMcpRegister(std::uint8_t reg, std::uint8_t &value) {
    Wire.beginTransmission(0x20);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(0x20, 1) != 1)
        return false;
    value = static_cast<std::uint8_t>(Wire.read());
    return true;
}

static bool writeMcpRegister(std::uint8_t reg, std::uint8_t value) {
    Wire.beginTransmission(0x20);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool configureWake2Input() {
    std::uint8_t iocon = 0, mirroredIocon = 0, direction = 0, pullups = 0;
    if (!readMcpRegister(0x0a, iocon) ||
        !readMcpRegister(0x0b, mirroredIocon) ||
        iocon != mirroredIocon || (iocon & 0x80) ||
        !readMcpRegister(0x01, direction) || !readMcpRegister(0x0d, pullups))
        return false;
    if (!writeMcpRegister(0x01, direction | 0x40) ||
        !writeMcpRegister(0x0d, pullups | 0x40))
        return false;
    return readMcpRegister(0x01, direction) && (direction & 0x40) &&
           readMcpRegister(0x0d, pullups) && (pullups & 0x40);
}

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
    if (Wire.begin()) {
        Wire.setTimeOut(25);
        wake2InputReady = configureWake2Input();
    }
    Serial.printf("TEST_WAKE2 input_pullup_ready=%u address=0x20 pin=GPB6\n",
                  wake2InputReady);
}

void loop() {
    larp::companion::Esp32GattsSource::DevelopmentRawWork work;
    if (gatts.takeDevelopmentRawWork(work)) {
        larp::companion::Esp32GattsSource::DevelopmentRawResult result;
        result.work = work;
        result.startedAtMs = millis();
        if (wake2InputReady) {
            static RawExpanderWireReader reader(Wire);
            static larp::diagnostic::RawExpanderProbe probe(reader);
            result.observation = probe.sample();
            result.observation.physicalChannelQualified =
                result.observation.readable();
        } else {
            result.observation.address = 0x20;
            result.observation.reg = 0x13;
            result.observation.bitIndex = 6;
            result.observation.status =
                larp::diagnostic::RawRegisterStatus::RegisterSelectFailed;
            result.observation.busCode = 0xfe; // Input mode could not be verified.
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
