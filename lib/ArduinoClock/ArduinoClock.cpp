#include "ArduinoClock.h"

#include <Arduino.h>

std::uint32_t ArduinoClock::nowMilliseconds() const
{
    return millis();
}

void ArduinoClock::delayMilliseconds(std::uint32_t duration)
{
    delay(duration);
}
