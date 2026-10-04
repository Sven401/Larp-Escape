#ifndef DFMINNIHANDLER_H
#define DFMINNIHANDLER_H

#include <Arduino.h>
#include <DFPlayerMini_Fast.h>
#include "GamePorts/IAudioPlayer.h"

class DFMinniHandler : public larp::ports::IAudioPlayer {
public:
    DFMinniHandler(HardwareSerial &serial, DFPlayerMini_Fast &myMP3, int busyPin, int wakeUpPin)
        : mySerial(serial), myMP3(myMP3), busyPin(busyPin), wakeUpPin(wakeUpPin),
          nextPeriodicTransition(0U), nextSequenceTransition(0U),
          sequenceTransitionsRemaining(0U), wakePinHigh(false) {}
    void begin();
    void service();
    void playTrack(std::uint16_t trackNumber) override;
    bool isBusy() override;
    void wakeupDFPlayer();
    void wakeup() override;

private:
    
    HardwareSerial& mySerial;
    int busyPin;
    int wakeUpPin;
    DFPlayerMini_Fast& myMP3;
    std::uint32_t nextPeriodicTransition;
    std::uint32_t nextSequenceTransition;
    std::uint8_t sequenceTransitionsRemaining;
    bool wakePinHigh;

    void setWakePin(bool high);
    void scheduleNextPeriodicTransition();
};

#endif // DFMINNIHANDLER_H
