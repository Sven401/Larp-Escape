#ifndef DFMINNIHANDLER_H
#define DFMINNIHANDLER_H

#include <Arduino.h>
#include <DFPlayerMini_Fast.h>

class DFMinniHandler {
public:
    DFMinniHandler(HardwareSerial &serial, DFPlayerMini_Fast &myMP3, int busyPin, int wakeUpPin)
        : mySerial(serial), myMP3(myMP3), busyPin(busyPin), wakeUpPin(wakeUpPin) {}
    void begin();
    void playTrack(uint16_t trackNumber);
    bool isBusy();

private:
    void wakeupDFPlayer();
    HardwareSerial& mySerial;
    int busyPin;
    int wakeUpPin;
    DFPlayerMini_Fast& myMP3;
};

#endif // DFMINNIHANDLER_H