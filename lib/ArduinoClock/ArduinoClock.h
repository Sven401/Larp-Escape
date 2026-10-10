#pragma once

#include "GamePorts/IClock.h"

class ArduinoClock : public larp::ports::IClock
{
public:
    std::uint32_t nowMilliseconds() const override;
    void delayMilliseconds(std::uint32_t duration) override;
};
