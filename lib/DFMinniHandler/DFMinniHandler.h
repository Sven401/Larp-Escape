#ifndef DFMINNIHANDLER_H
#define DFMINNIHANDLER_H

#include "GamePorts/IAudioPlayer.h"
#include "GamePorts/IRuntimeService.h"
#include "GameRuntime/WakePulseSchedule.h"
#include <Arduino.h>
#include <DFPlayerMini_Fast.h>

class DFMinniHandler : public larp::ports::IAudioPlayer, public larp::ports::IRuntimeService {
  public:
    DFMinniHandler(HardwareSerial &serial, DFPlayerMini_Fast &myMP3, int busyPin, int wakeUpPin)
        : mySerial(serial), busyPin(busyPin), wakeUpPin(wakeUpPin), myMP3(myMP3),
          initialized(false) {}
    void begin();
    void service() override;
    void playTrack(std::uint16_t trackNumber) override;
    bool isBusy() override;
    void wakeupDFPlayer();
    void wakeup() override;

  private:
    HardwareSerial &mySerial;
    int busyPin;
    int wakeUpPin;
    DFPlayerMini_Fast &myMP3;
    bool initialized;
    larp::runtime::WakePulseSchedule wakeSchedule;

    void writeWakePin();
};

#endif // DFMINNIHANDLER_H
