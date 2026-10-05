#include "NFCReader.h"
#include "GameRuntime/CardUidCopy.h"
#include <algorithm>
#include <cstring>

bool NFCReader::begin() {
    if (initialized)
        return true;
    const std::uint32_t started = millis();
    while (!nfc.begin()) {
        if (static_cast<std::uint32_t>(millis() - started) >= 10000U) {
            Serial.println("NFC initialization timed out");
            return false;
        }
        nfc.reset();
        delay(20U); // Startup only; the game is not running yet.
    }
    const std::uint32_t version = nfc.getFirmwareVersion();
    if (version == 0U) {
        Serial.println("NFC firmware query failed");
        return false;
    }
    initialized = true;
    readerDisabled = false;
    Serial.print("NFC firmware: ");
    Serial.println(version, HEX);
    return true;
}
bool NFCReader::disableReader() {
    if (!initialized)
        return false;
    readerDisabled = !nfc.AsTarget();
    return readerDisabled;
}
bool NFCReader::enableReader() {
    if (!initialized)
        return false;
    nfc.wakeup();
    readerDisabled = !nfc.SAMConfig();
    return !readerDisabled;
}
larp::domain::CardUid NFCReader::readCard(std::uint16_t timeoutMilliseconds) {
    isNewUid = false;
    isStoredUid = false;
    uidLength = 0U;
    std::fill(std::begin(uid), std::end(uid), 0U);
    if (!enableReader())
        return {};

    std::uint8_t readLength = 0U;
    const bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, driverUid.data(),
                                                 &readLength, timeoutMilliseconds);
    larp::domain::CardUid card{};
    const bool accepted =
        success && larp::runtime::tryCopyCardUid(driverUid.data(), readLength, card);
    // Retain the established target-mode lifecycle for now. Its hardware latency
    // and necessity are a separate device check, not inferred from host tests.
    disableReader();
    if (!accepted)
        return {};
    uidLength = static_cast<std::uint8_t>(card.size());
    std::copy(card.begin(), card.end(), std::begin(uid));
    isNewUid = card != lastUid;
    lastUid = card;
    for (std::size_t index = 0U; index < storedUidCount; ++index) {
        if (storedUids[index] == card) {
            isStoredUid = true;
            break;
        }
    }
    if (!isStoredUid && storedUidCount < storedUids.size()) {
        storedUids[storedUidCount++] = card;
        isStoredUid = true;
    }
    allUidsStored = storedUidCount == storedUids.size();
    return card;
}
void NFCReader::resetStoredUids() {
    storedUids = {};
    storedUidCount = 0U;
    isStoredUid = false;
    allUidsStored = false;
}
bool NFCReader::isCurrentUid(const std::uint8_t *queryUid, std::uint8_t queryUidLength) const {
    return queryUid != nullptr && uidLength == sizeof(uid) && queryUidLength == sizeof(uid) &&
           std::memcmp(uid, queryUid, sizeof(uid)) == 0;
}
