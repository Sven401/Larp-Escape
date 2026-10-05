#pragma once
#include "GamePorts/ICardReader.h"
#include <Adafruit_PN532.h>
#include <array>
#include <cstdint>

// Single application-loop owner; the referenced driver lives until restart.
class NFCReader : public larp::ports::ICardReader {
  public:
    explicit NFCReader(Adafruit_PN532 &nfc) : nfc(nfc) {}
    NFCReader(const NFCReader &) = delete;
    NFCReader &operator=(const NFCReader &) = delete;
    bool begin();
    bool disableReader();
    bool enableReader();
    void resetStoredUids();
    bool isCurrentUid(const std::uint8_t *queryUid, std::uint8_t queryUidLength) const;
    larp::domain::CardUid readCard(std::uint16_t timeoutMilliseconds) override;
    std::uint8_t uid[7] = {};
    bool isNewUid = false;
    bool isStoredUid = false;
    bool allUidsStored = false;

  private:
    bool initialized = false;
    bool readerDisabled = true;
    Adafruit_PN532 &nfc;
    larp::domain::CardUid lastUid{};
    std::array<larp::domain::CardUid, 6> storedUids{};
    std::size_t storedUidCount = 0U;
    std::uint8_t uidLength = 0U;
    // Driver 1.3.4 copies the returned uint8 length without a destination-capacity
    // parameter. Reserve its full destination range before validating seven-byte
    // game UIDs. This does not validate the driver's internal response buffer.
    std::array<std::uint8_t, 256> driverUid{};
};
