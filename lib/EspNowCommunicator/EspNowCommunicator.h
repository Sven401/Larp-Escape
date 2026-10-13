#pragma once

#include "GamePorts/ICommunicator.h"

#include <array>
#include <cstdint>

class EspNowCommunicator : public larp::ports::ICommunicator
{
public:
    explicit EspNowCommunicator(const std::array<std::uint8_t, 6> &peerAddress);

    bool send(larp::ports::GameMessage message) override;

private:
    bool sendPayload(const char *payload);

    std::array<std::uint8_t, 6> peerAddress;
};
