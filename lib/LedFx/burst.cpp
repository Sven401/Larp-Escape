#include "FastLED.h"
#include "burst.h"

Burst::Burst(uint8_t frames, CRGB *leds, int NUM_LEDS)
    : frames(frames), leds(leds), NUM_LEDS(NUM_LEDS), step(frames) {}

void Burst::draw()
{
    if (step < frames) // Effect lasts for a configurable number of frames
    {
        uint8_t brightness;
        if (step < frames / 6)
        {
            // Fast ramp-up over the first fraction of frames
            brightness = 255 * (step + 1) / (frames / 6);
        }
        else
        {
            // Smooth fade-out for the remaining frames
            brightness = 255 - ((step - frames / 6) * (255 / (frames - frames / 6)));
        }

        for (int i = 0; i < NUM_LEDS; i++)
        {
            leds[i] += color.scale8(CRGB(brightness, brightness, brightness)) ; // Add brightness instead of setting
        }

        step++; // Move to the next frame
    }
}

void Burst::fire(uint8_t inframes, CRGB incolor)
{
    color = incolor;
    if (step >= frames){
    step = 0;
    frames = inframes;}
    else{
        Serial.print("failed to fire Burst. step: ");
        Serial.print(step);
        Serial.print(" frames: ");
        Serial.println(frames);
    }
}