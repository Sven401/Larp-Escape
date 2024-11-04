#ifndef NFCREADER_H
#define NFCREADER_H
#include "Config.h"
#include <Adafruit_PN532.h> // Include the necessary library for PN532
#include <Arduino.h>        // Include Arduino libraries

class NFCReader
{
public:
    NFCReader(uint8_t irqPin)
        : nfc(irqPin), readerDisabled(false), lastCardReadTime(0) {}
    void begin();              // Initializes the NFC reader
    void loop();               // Should be called in the main loop
    bool startListening();     // Start listening for NFC cards
    bool handleCardDetected(); // Handle detected card

private:
    bool readerDisabled;
    unsigned long lastCardReadTime;
    Adafruit_PN532 nfc; // Assuming you have initialized the PN532 object
    int irqPrev = HIGH; // Previous state of the IRQ pin
    int irqCurr = HIGH; // Current state of the IRQ pin

    void checkForCard(); // Check if a card has been detected
};

#endif // NFCREADER_H
