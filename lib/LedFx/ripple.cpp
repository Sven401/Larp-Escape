#include "FastLED.h"
#include "ripple.h"

Ripple::Ripple(uint8_t centerLED, CRGB *leds, uint8_t frames, int NUM_LEDS)
    : centerLED(centerLED), leds(leds), step(frames), frames(frames), NUM_LEDS(NUM_LEDS) {} // Default effect duration

void Ripple::draw()
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
        if (loggingEnabled){
        Serial.println("~~");
        Serial.print("Center");
        Serial.println(centerLED);
        }
        for (int i = 0; i < (NUM_LEDS / 2) - 7; i++)
        {

            if (i == 0)
            {
                leds[centerLED] += CRGB(brightness/2, brightness/2, brightness); // Add brightness instead of setting
                continue;
            }
            if (loggingEnabled){
            Serial.print(i);
            Serial.print("  ");
            Serial.print((centerLED + i) % NUM_LEDS);
            Serial.print("  ");
            Serial.print((centerLED - i) % NUM_LEDS);
            Serial.print("  ");
            Serial.print((centerLED - i + NUM_LEDS) % NUM_LEDS);
            Serial.print("  ");
            Serial.print(brightness);
            Serial.print(" ");
            Serial.println(brightness / (i*2));
            }
            int j = i*2;
            leds[(centerLED + i) % NUM_LEDS] += CRGB(brightness / j, brightness / j, brightness / j);
            leds[(centerLED - i) % NUM_LEDS] += CRGB(brightness / j, brightness / j, brightness / j);
        }
        step++; // Move to the next frame
    }
}

void Ripple::fire(uint8_t inframes, uint8_t incenterLED)
{
    if (step >= frames)
    {
        centerLED = incenterLED;
        frames = inframes;
        step = 0;
    }
};