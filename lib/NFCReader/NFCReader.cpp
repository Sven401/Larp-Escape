#include "NFCReader.h"

bool NFCReader::begin()
{
    bool setup = nfc.begin();      // Initialize the PN532
    uint32_t versiondata = nfc.getFirmwareVersion();
    unsigned long startTime = millis();
    const unsigned long timeout = 10000; // 10 seconds timeout
    Serial.println("Initializing NFC reader...");
    Serial.print(setup);
    Serial.println(setup ? "Success" : "Failed");
    readerDisabled = !setup;
    while (!setup) {
        Serial.println("Didn't find PN53x board, retrying...");
        setup = nfc.begin();

        nfc.reset();

        readerDisabled = !setup;

        if (millis() - startTime >= timeout) {
            Serial.println("Timeout reached, restarting ESP...");
            ESP.restart();
        }
    }
    // Got ok data, print it out!
    Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
    Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
    Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
    return setup;
}

bool NFCReader::disableReader()
{
    readerDisabled = !nfc.AsTarget(); 
    return readerDisabled;
}
bool NFCReader::enableReader()
{
    nfc.wakeup();
    bool awake = nfc.SAMConfig();
    readerDisabled = !awake;
    if (!awake)
    {
        readerDisabled = !begin();
    }
    return !readerDisabled;
} 

uint8_t* NFCReader::getCard()
{
    Serial.print("Attempting to enable NFC reader. Result: ");
    bool result = enableReader();
    Serial.println(result ? "Success" : "Failure");
    if(!result){
        return nullptr;
    }
    Serial.println("Waiting for an ISO14443A Card ...");
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
    Serial.print("Read passive target ID success: ");
    Serial.println(success);
    if (!success) {
        return nullptr;
    }

    isNewUid = (uidLength != sizeof(lastUid) || memcmp(uid, lastUid, uidLength) != 0);
    isStoredUid = false;

    if (isNewUid) {
        Serial.print("New UID: ");
        for (uint8_t i = 0; i < uidLength; i++) {
            Serial.print(uid[i], HEX); Serial.print(" ");
        }
        Serial.println();

        // Store the UID in the stored UIDs array
        for (int i = 0; i < 6; i++) {
            if (storedUidLengths[i] == 0) {
                memcpy(storedUids[i], uid, uidLength);
                storedUidLengths[i] = uidLength;
                break;
            }
        }

        // Store the UID as the last UID
        memset(lastUid, 0, sizeof(lastUid));
        memcpy(lastUid, uid, uidLength);
    } else {
        Serial.println("Same UID as last time");
    }

    // Check if the UID is in the stored UIDs
    for (int i = 0; i < 6; i++) {
        if (storedUidLengths[i] == uidLength && memcmp(uid, storedUids[i], uidLength) == 0) {
            isStoredUid = true;
            Serial.print("This UID matches stored UID #"); Serial.println(i + 1);
            break;
        }
    }

    // Check if all 6 UIDs are stored
    allUidsStored = true;
    for (int i = 0; i < 6; i++) {
        if (storedUidLengths[i] == 0) {
            allUidsStored = false;
            break;
        }
    }
    disableReader();
    return uid;
}

void NFCReader::resetStoredUids()
{
    memset(storedUids, 0, sizeof(storedUids));
    memset(storedUidLengths, 0, sizeof(storedUidLengths));
}

bool NFCReader::isCurrentUid(uint8_t* queryUid, uint8_t queryUidLength)
{
    return (uidLength == queryUidLength && memcmp(uid, queryUid, queryUidLength) == 0);
}
