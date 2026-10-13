#include "EspNowCommunicator.h"

#include <cstring>
#include <esp_now.h>

EspNowCommunicator::EspNowCommunicator(const std::array<std::uint8_t, 6> &peerAddress)
    : peerAddress(peerAddress)
{
}

bool EspNowCommunicator::send(larp::ports::GameMessage message)
{
    switch (message)
    {
    case larp::ports::GameMessage::Activate:
        return sendPayload("ACTIVATE");
    }
    return false;
}

bool EspNowCommunicator::sendPayload(const char *payload)
{
    struct Message
    {
        char text[32];
    } packet{};

    const std::size_t payloadLength = std::strlen(payload);
    if (payloadLength >= sizeof(packet.text))
    {
        return false;
    }
    std::memcpy(packet.text, payload, payloadLength);

    return esp_now_send(peerAddress.data(), reinterpret_cast<const std::uint8_t *>(&packet),
                        sizeof(packet)) == ESP_OK;
}
