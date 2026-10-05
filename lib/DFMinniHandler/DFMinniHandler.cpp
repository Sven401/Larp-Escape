#include "DFMinniHandler.h"
#include "Config.h"

void DFMinniHandler::begin() {
    if (initialized)
        return;

    pinMode(busyPin, INPUT);
    pinMode(wakeUpPin, OUTPUT);
    mySerial.begin(9600, SERIAL_8N1, 25, 26);

    // Preserve the original HIGH/LOW wake sequence before playback is enabled.
    // This startup-only wait cannot overlap a game tick or another pin owner.
    wakeSchedule.requestWake(millis());
    writeWakePin();
    while (wakeSchedule.isWaking()) {
        delay(1);
        service();
    }

    myMP3.begin(mySerial);
    myMP3.volume(VOL);
    initialized = true;
}

void DFMinniHandler::service() {
    const std::uint32_t now = millis();
    if (!wakeSchedule.transitionDue(now))
        return;

    // Wake completion retains LOW after its eighth 100ms hold; only a
    // periodic transition toggles the level used for the following hold.
    const bool followingHigh = !wakeSchedule.isWaking() && !wakeSchedule.isHigh();
    const std::uint32_t nextHold = followingHigh ? static_cast<std::uint32_t>(random(200, 500))
                                                 : static_cast<std::uint32_t>(random(100, 400));
    if (wakeSchedule.advance(now, nextHold))
        writeWakePin();
}

void DFMinniHandler::playTrack(std::uint16_t trackNumber) {
    if (!initialized) {
        Serial.println("Audio command ignored before initialization");
        return;
    }
    // parseFeedback() parses a query response; absence of feedback is not
    // evidence of a disconnected player and must not trigger reinitialization.
    myMP3.playFromMP3Folder(trackNumber);
}

bool DFMinniHandler::isBusy() {
    return wakeSchedule.isWaking() || digitalRead(busyPin) == LOW;
}

void DFMinniHandler::wakeupDFPlayer() {
    wakeup();
}

void DFMinniHandler::wakeup() {
    if (initialized && wakeSchedule.requestWake(millis()))
        writeWakePin();
}

void DFMinniHandler::writeWakePin() {
    digitalWrite(wakeUpPin, wakeSchedule.isHigh() ? HIGH : LOW);
}
