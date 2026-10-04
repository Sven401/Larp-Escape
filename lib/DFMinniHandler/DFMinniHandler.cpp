#include "DFMinniHandler.h"
#include <Arduino.h>
#include <DFPlayerMini_Fast.h>
#include "Config.h"

void DFMinniHandler::begin() {
    pinMode(busyPin, INPUT);
    pinMode(wakeUpPin, OUTPUT);
    setWakePin(true);
    delay(100);
    setWakePin(false);
    delay(100);
    scheduleNextPeriodicTransition();
    if (mySerial.available() == 0){
        mySerial.begin(9600, SERIAL_8N1, 25, 26);
    }
    myMP3.begin(mySerial);
    myMP3.volume(VOL);
}

void DFMinniHandler::service() {
    const std::uint32_t now = millis();

    if (sequenceTransitionsRemaining > 0U &&
        static_cast<std::int32_t>(now - nextSequenceTransition) >= 0) {
        setWakePin(!wakePinHigh);
        --sequenceTransitionsRemaining;
        nextSequenceTransition = now + 100U;
    }

    if (sequenceTransitionsRemaining == 0U &&
        static_cast<std::int32_t>(now - nextPeriodicTransition) >= 0) {
        setWakePin(!wakePinHigh);
        scheduleNextPeriodicTransition();
    }
}

void DFMinniHandler::playTrack(uint16_t trackNumber) {
    if (!myMP3.parseFeedback()) {
        Serial.println("DFPlayer not found");
        begin();
    } else {
        Serial.println("DFPlayer found");
    }
    if (!myMP3.parseFeedback()) {
        Serial.println("DFPlayer not found");
        begin();
    }
    else{
        Serial.println("DFPlayer found");
    }
    Serial.print("Playing track: ");
    Serial.println(trackNumber);
    myMP3.playFromMP3Folder(trackNumber);
    Serial.print("DFPlayer busy state: ");
    Serial.print(isBusy());
}

bool DFMinniHandler::isBusy() {
    int busyState = digitalRead(busyPin);
    return busyState == LOW || sequenceTransitionsRemaining > 0U;
}

void DFMinniHandler::wakeupDFPlayer() {
    wakeup();
}

void DFMinniHandler::wakeup() {
    sequenceTransitionsRemaining = 8U;
    nextSequenceTransition = millis() + 100U;
    nextPeriodicTransition = nextSequenceTransition + 800U;
}

void DFMinniHandler::setWakePin(bool high) {
    wakePinHigh = high;
    digitalWrite(wakeUpPin, high ? HIGH : LOW);
}

void DFMinniHandler::scheduleNextPeriodicTransition() {
    const std::uint32_t duration = wakePinHigh
        ? static_cast<std::uint32_t>(random(200, 500))
        : static_cast<std::uint32_t>(random(100, 400));
    nextPeriodicTransition = millis() + duration;
}
