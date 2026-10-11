#include "Config.h"
#include <array>
#include <cstdint>

#include "ArduinoClock.h"
#include "CompanionStatusStore.h"
#if defined(LARP_PRODUCT_GATTS_CLOSED)
#include "ProductGattsOwner.h"
#endif
#include "DFMinniHandler.h"
#include "Esp32CompanionIdentityOwner.h"
#include "EspNowCommunicator.h"
#include "EspNowRuntime.h"
#include "FastLED.h"
#include "GameLightEffects.h"
#include "Keymatrix.h"
#include "LarpEscapeApp.h"
#include "ClosedDiagnosticArbiter.h"
#include "Esp32LedParkDeadline.h"
#include "MCPHandler.h"
#include "NFCReader.h"
#include "SerialGameDiagnostics.h"
#include "burst.h"
#include "ripple.h"
#include "twinklefox.h"

#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define DATA_PIN 15
#define VOLTS 5
#define MAX_MA 1000
#define NUM_LEDS 23
CRGBArray<NUM_LEDS> leds;

const std::array<std::uint8_t, 6> receiverMAC = {{0xB0, 0xA7, 0x32, 0xF1, 0x86, 0x54}};
using fl::MyTwinkleFox;
MyTwinkleFox twinkleFox(NUM_LEDS);
Burst burst(30, leds, NUM_LEDS);
Ripple ripples[4] = {Ripple(5, leds, 30, NUM_LEDS), Ripple(11, leds, 30, NUM_LEDS),
                     Ripple(17, leds, 30, NUM_LEDS), Ripple(22, leds, 30, NUM_LEDS)};

// Hardware adapters and services have process lifetime. setup initializes them;
// only app.update owns game ticks, audio service and communication status.
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
NFCReader nfcReader(nfc);
Adafruit_MCP23X17 mcp1;
Adafruit_MCP23X17 mcp2;
MCPHandler mcpHandler1(mcp1, mcpConfigs[0], "MCP1");
MCPHandler mcpHandler2(mcp2, mcpConfigs[1], "MCP2");
std::array<MCPHandler *, 2> handlers = {{&mcpHandler1, &mcpHandler2}};
KeyMatrix keymatrix(handlers, ROWMAP, COLMAP);
HardwareSerial mySoftwareSerial(1);
DFPlayerMini_Fast myMP3;
DFMinniHandler dfmHandler(mySoftwareSerial, myMP3, DFBUSY, DFWAKEUP);
ArduinoClock gameClock;
GameLightEffects gameLightEffects(twinkleFox, burst, ripples, gameClock, leds, NUM_LEDS);
EspNowCommunicator communicator(receiverMAC);
EspNowRuntime communicationRuntime(receiverMAC);
SerialGameDiagnostics gameDiagnostics;
CompanionStatusStore companionStatus;
Esp32CompanionIdentityOwner companionIdentity;
#if defined(LARP_PRODUCT_GATTS_CLOSED)
ProductGattsOwner closedGatts(companionIdentity);
#endif
ReichstagGame game(nfcReader, keymatrix, keymatrix, dfmHandler, gameClock, gameLightEffects,
                   communicator, &gameDiagnostics);
larp::application::LarpEscapeApp app(game, dfmHandler, communicationRuntime, companionStatus);
Esp32LedParkDeadline ledParkDeadline;
larp::application::ClosedDiagnosticArbiter diagnosticArbiter(app, gameLightEffects,
                                                             ledParkDeadline);

std::uint32_t maximumMainLoopDurationMicroseconds = 0U;
std::uint32_t lastRuntimeReportMilliseconds = 0U;

void setup() {
    Serial.begin(115200);
    // Startup entropy runs before radio and device workers. An unavailable
    // companion identity leaves standalone play running and never erases NVS.
    const auto companionIdentityState = companionIdentity.begin();
    Serial.print("Companion identity state (diagnosis closed): ");
    Serial.println(static_cast<unsigned>(companionIdentityState));
#if defined(LARP_PRODUCT_GATTS_CLOSED)
    // The GATT owner advertises only after its service and closed registry ACK.
    // A discoverable link never admits a peer role or diagnosis here.
    if (companionIdentityState == Esp32CompanionIdentityOwner::State::ReadyClosed) {
        const auto bleState = closedGatts.begin();
        Serial.print("Closed GATT registration state: ");
        Serial.println(static_cast<unsigned>(bleState));
        if (bleState == ProductGattsOwner::State::StackFailed) {
            const auto &report = closedGatts.stackReport();
            Serial.print("BLE stack failed step/error: ");
            Serial.print(static_cast<unsigned>(report.state));
            Serial.print(" / ");
            Serial.println(static_cast<long>(report.sdkError));
            Serial.print("BLE stack controller/bluedroid before->after: ");
            Serial.print(report.controllerBefore); Serial.print("->");
            Serial.print(report.controllerAfter); Serial.print(" / ");
            Serial.print(report.bluedroidBefore); Serial.print("->");
            Serial.println(report.bluedroidAfter);
            Serial.print("BLE stack internal free/largest before->after: ");
            Serial.print(report.internalFreeBefore); Serial.print("/");
            Serial.print(report.internalLargestBefore); Serial.print(" -> ");
            Serial.print(report.internalFreeAfter); Serial.print("/");
            Serial.println(report.internalLargestAfter);
        }
    }
#endif
    Wire.begin();
    Serial.println("Beginning");
    if (!keymatrix.begin()) {
        Serial.println("Failed to initialize key matrix");
        ESP.restart();
        return;
    }
    if (!nfcReader.begin()) {
        Serial.println("Failed to initialize NFC reader");
        ESP.restart();
        return;
    }
    dfmHandler.begin();
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    keymatrix.printMatrixState();
    twinkleFox.currentPalette = MutedAllColors_p;
    twinkleFox.setTwinkleDensity(1);
    twinkleFox.setTwinkleSpeed(1);
    FastLED.setBrightness(100);
    if (!companionStatus.begin() || !communicationRuntime.begin() || !gameLightEffects.begin()) {
        Serial.println("Failed to initialize runtime services");
        ESP.restart();
        return;
    }
    if (!ledParkDeadline.begin()) {
        // Standalone game remains available. Any later diagnostic handoff
        // faults closed when its unavailable timer cannot be armed.
        Serial.println("LED diagnostic deadline unavailable; diagnostics closed");
    }
    companionStatus.publish(game.snapshot());
    Serial.println("Runtime ready");
}

void loop() {
    const std::uint32_t loopStarted = micros();
    diagnosticArbiter.update();
    const std::uint32_t now = millis();
    if ((now - lastRuntimeReportMilliseconds) >= 10000U) {
        lastRuntimeReportMilliseconds = now;
        Serial.print("Runtime max loop us: ");
        Serial.println(maximumMainLoopDurationMicroseconds);
        Serial.print("LED min free stack bytes: ");
        Serial.println(gameLightEffects.minimumStackFreeBytes());
        Serial.print("LED max observed queue depth: ");
        Serial.println(gameLightEffects.maximumQueueDepth());
        Serial.print("LED max render us: ");
        Serial.println(gameLightEffects.maximumRenderDurationMicroseconds());
        Serial.print("LED missed frame deadlines: ");
        Serial.println(gameLightEffects.frameDeadlineMissCount());
        Serial.print("ESP-NOW max observed queue depth: ");
        Serial.println(communicationRuntime.maximumObservedQueueDepth());
        Serial.print("LED command drops: ");
        Serial.println(gameLightEffects.droppedCommandCount());
        Serial.print("ESP-NOW status drops: ");
        Serial.println(communicationRuntime.droppedStatusCount());
        Serial.print("ESP-NOW delivery success/fail: ");
        Serial.print(communicationRuntime.successfulDeliveryCount());
        Serial.print('/');
        Serial.println(communicationRuntime.failedDeliveryCount());
#if defined(LARP_PRODUCT_GATTS_CLOSED)
        larp::companion::PeerTransportBinding originalLink;
        Serial.print("BLE discovery phase (no role): ");
        Serial.println(static_cast<unsigned>(closedGatts.advertisingState()));
        Serial.print("Original BLE link current (no bond claim): ");
        Serial.println(closedGatts.originalLink(originalLink) ? 1 : 0);
#endif
    }
    // Include the diagnostic report in this measurement. Its duration is visible
    // in the next report, because this iteration has not ended while printing.
    const std::uint32_t loopDuration = micros() - loopStarted;
    if (loopDuration > maximumMainLoopDurationMicroseconds)
        maximumMainLoopDurationMicroseconds = loopDuration;
}
