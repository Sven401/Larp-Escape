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
    return busyState == LOW;
}

void DFMinniHandler::wakeupDFPlayer() {
    // Wake up the DFPlayer
    digitalWrite(wakeUpPin, HIGH);
    Serial.print("DFPlayer busy state: ");
    Serial.print(isBusy());
    delay(100); // Wait for the DFPlayer to wake up
    digitalWrite(wakeUpPin, LOW);
    delay(100); // Wait for the DFPlayer to wake up
    digitalWrite(wakeUpPin, HIGH);
    Serial.print("DFPlayer busy state: ");
    Serial.print(isBusy());
    delay(100); // Wait for the DFPlayer to wake up
    digitalWrite(wakeUpPin, LOW);
}