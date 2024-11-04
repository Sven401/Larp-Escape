#include "NFCReader.h"

void NFCReader::begin()
{
    nfc.begin();      // Initialize the PN532
    startListening(); // Start listening
}

void NFCReader::loop()
{
    if (readerDisabled)
    {
        if (millis() - lastCardReadTime > DELAY_BETWEEN_CARDS)
        {
            readerDisabled = false;
            startListening();
        }
    }
    else
    {
        checkForCard(); // Check for IRQ and handle card detection
    }
}

void NFCReader::checkForCard()
{
    irqCurr = digitalRead(PN532_IRQ);

    if (irqCurr == LOW && irqPrev == HIGH)
    {
        Serial.println("Got NFC IRQ");
        if (handleCardDetected())
        {
            Serial.println("Card detected and processed.");
        }
        else
        {
            Serial.println("Failed to read card.");
        }
    }
    irqPrev = irqCurr; // Update the previous IRQ state
}

bool NFCReader::startListening()
{
    if (!readerDisabled && millis() - lastCardReadTime > DELAY_BETWEEN_CARDS)
    {
        readerDisabled = !nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
        return !readerDisabled; // Returns true if listening is active
    }
    return false; // Not starting listening
}

bool NFCReader::handleCardDetected()
{
    uint8_t uid[7], uidLength;
    bool success = nfc.readDetectedPassiveTargetID(uid, &uidLength);

    if (success)
    {
        Serial.println("Found an ISO14443A card");
        Serial.print("  UID Length: ");
        Serial.print(uidLength, DEC);
        Serial.println(" bytes");
        Serial.print("  UID Value: ");
        nfc.PrintHex(uid, uidLength); // Print UID

        if (uidLength == 4)
        {
            uint32_t cardid = (uid[0] << 24) | (uid[1] << 16) | (uid[2] << 8) | uid[3];
            Serial.print("Seems to be a Mifare Classic card #");
            Serial.println(cardid);
        }
        lastCardReadTime = millis(); // Update last read time
        readerDisabled = true;       // Disable reader for a certain time
    }
    return success; // Return success status
}
