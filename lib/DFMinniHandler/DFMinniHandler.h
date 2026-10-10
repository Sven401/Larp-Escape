#ifndef DFMINNIHANDLER_H
#define DFMINNIHANDLER_H

#include <Arduino.h>
#include <DFPlayerMini_Fast.h>
#include "GamePorts/IAudioPlayer.h"

class DFMinniHandler : public larp::ports::IAudioPlayer {
public:
    DFMinniHandler(HardwareSerial &serial, DFPlayerMini_Fast &myMP3, int busyPin, int wakeUpPin)
        : mySerial(serial), myMP3(myMP3), busyPin(busyPin), wakeUpPin(wakeUpPin) {}
    void begin();
    void playTrack(std::uint16_t trackNumber) override;
    bool isBusy() override;
    void wakeupDFPlayer();
    void wakeup() override;

private:
    
    HardwareSerial& mySerial;
    int busyPin;
    int wakeUpPin;
    DFPlayerMini_Fast& myMP3;
};

#endif // DFMINNIHANDLER_H
