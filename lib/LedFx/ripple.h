#pragma once

#include "FastLED.h"

class Ripple
{
public:
 Ripple(uint8_t centerLED, CRGB *leds, uint8_t frames, int NUM_LEDS);

void draw();
void fire(uint8_t inframes, uint8_t centerLED);

private:

    uint8_t frames; // Stores the LED range for the effect
    CRGB *leds; // Pointer to the LED array
    uint8_t step;
    int NUM_LEDS;
    uint8_t centerLED;
    bool loggingEnabled = false;

};