// Larp-Escape replacement for the pinned Adafruit PN532 1.3.4 UID read only.
// See docs/nfc-packet-contract.md for provenance, limits and rollback.
#include "GameRuntime/Pn532PassiveFrame.h"
#include <cstring>
bool Adafruit_PN532::readDetectedPassiveTargetID(uint8_t *uid, uint8_t *uidLength) {
    if (!uidLength)
        return false;
    *uidLength = 0;
    if (!uid)
        return false;
    // Read checksum/postamble as well as the UID. Never parse shared old bytes.
    uint8_t frame[PN532_PACKBUFFSIZ] = {};
    std::size_t received = sizeof(frame);
    if (spi_dev) {
        const uint8_t command = PN532_SPI_DATAREAD;
        if (!spi_dev->write_then_read(&command, 1, frame, sizeof(frame)))
            return false;
    } else if (i2c_dev) {
        uint8_t response[PN532_PACKBUFFSIZ + 1] = {};
        if (!i2c_dev->read(response, sizeof(response)) || response[0] != PN532_I2C_READY)
            return false;
        std::memcpy(frame, response + 1, sizeof(frame));
    } else if (ser_dev) {
        // HSU packets are not padded to 64 bytes; consume their exact bounded
        // header-declared length. Existing Stream timeouts still apply.
        if (ser_dev->readBytes(frame, 5) != 5 || frame[3] < 3 || frame[3] > 57)
            return false;
        received = static_cast<std::size_t>(frame[3]) + 7;
        if (ser_dev->readBytes(frame + 5, received - 5) != received - 5)
            return false;
    } else
        return false;
    larp::runtime::PassiveTarget target;
    if (larp::runtime::parsePassiveTarget(frame, received, target) !=
        larp::runtime::PassiveTargetOutcome::Card)
        return false;
    // Upstream public API promises a caller buffer of up to seven UID bytes.
    std::memcpy(uid, target.uid.data(), target.length);
    *uidLength = target.length;
    return true;
}
