#ifndef NFCREADER_H
#define NFCREADER_H
#include "Config.h"
#include <Adafruit_PN532.h> // Include the necessary library for PN532
#include <Arduino.h>        // Include Arduino libraries

class NFCReader
{
public:
    NFCReader(Adafruit_PN532 &nfc)
        : nfc(nfc){}
    bool begin();              // Initializes the NFC reader
    bool disableReader();      // Disable the NFC reader
    bool enableReader();
    void resetStoredUids();       // Enable the NFC reader
    bool isCurrentUid(uint8_t* queryUid, uint8_t queryUidLength);
    uint8_t uid[7];  // Buffer to store the returned UID
    uint8_t* getCard();
    bool isNewUid;
    bool isStoredUid;
    bool allUidsStored;

private:
    bool readerDisabled = true;
    //unsigned long lastCardReadTime;
    Adafruit_PN532 &nfc; // Assuming you have initialized the PN532 object
    //int irqPrev = HIGH; // Previous state of the IRQ pin
    //int irqCurr = HIGH; // Current state of the IRQ pin
    uint8_t lastUid[7] = {0};
    uint8_t storedUids[6][7] = {0};
    uint8_t storedUidLengths[6] = {0};
    uint8_t success;
    uint8_t uidLength;   
    void checkForCard(); // Check if a card has been detected
    bool handleCardDetected(); // Handle detected card
    bool startListening();     // Start listening for NFC cards
};

#endif // NFCREADER_H
