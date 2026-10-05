#include "burst.h"
#include "FastLED.h"

Burst::Burst(uint8_t frames, CRGB *leds, int NUM_LEDS)
    : frames(frames), leds(leds), NUM_LEDS(NUM_LEDS), step(frames) {}

void Burst::draw() {
    if (leds != nullptr && NUM_LEDS > 0 && step < frames) {
        uint8_t brightness;
        if (step < frames / 6) {
            // Fast ramp-up over the first fraction of frames
            brightness = 255 * (step + 1) / (frames / 6);
        } else {
            // Smooth fade-out for the remaining frames
            brightness = 255 - ((step - frames / 6) * (255 / (frames - frames / 6)));
        }

        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] += color.scale8(
                CRGB(brightness, brightness, brightness)); // Add brightness instead of setting
        }

        step++; // Move to the next frame
    }
}

void Burst::fire(uint8_t inframes, CRGB incolor) {
    if (step >= frames) {
        color = incolor;
        step = 0;
        frames = inframes;
    }
}