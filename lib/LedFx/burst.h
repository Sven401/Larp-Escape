#pragma once

#include "FastLED.h"
#include <tuple> // Include for std::tuple

class Burst
{
public:

    Burst(uint8_t frames, CRGB *leds, int NUM_LEDS); // Fixed tuple issue

    void draw();

    void fire(uint8_t inframes  = 30, CRGB color = {255,255,255});

private:
    uint8_t frames; // Stores the LED range for the effect
    CRGB *leds; // Pointer to the LED array
    uint8_t step;
    int NUM_LEDS;
    CRGB color;
};