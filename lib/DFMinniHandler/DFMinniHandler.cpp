#include "DFMinniHandler.h"
#include <Arduino.h>
#include <DFPlayerMini_Fast.h>
#include "Config.h"

void DFMinniHandler::begin() {
    pinMode(busyPin, INPUT);
    pinMode(wakeUpPin, OUTPUT);
    if (mySerial.available() == 0){
        mySerial.begin(9600, SERIAL_8N1, 25, 26);
    }
    wakeupDFPlayer();
    myMP3.begin(mySerial);
    myMP3.volume(VOL);
}

void DFMinniHandler::playTrack(uint16_t trackNumber) {
    if (myMP3.currentVersion() == -1) {
        begin();
        delay(300); // Wait for the DFPlayer to reset
        myMP3.begin(mySerial);
        myMP3.volume(VOL);
    }
    myMP3.playFromMP3Folder(trackNumber);
}

bool DFMinniHandler::isBusy() {
    int busyState = digitalRead(busyPin);
    Serial.print("Busy pin state: ");
    Serial.print(busyState);
    Serial.println(busyState == LOW ? "LOW" : "HIGH");
    return busyState == LOW;
}

void DFMinniHandler::wakeupDFPlayer() {
    // Wake up the DFPlayer
    digitalWrite(wakeUpPin, HIGH);
    delay(100); // Wait for the DFPlayer to wake up
    digitalWrite(wakeUpPin, LOW);
}