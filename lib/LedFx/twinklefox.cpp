#include "FastLED.h"
#include "fx/fx1d.h"
#include "fl/str.h"
#include "fl/namespace.h"
#include "pallets.h"
#include "twinklefox.h"

namespace fl
{
    MyTwinkleFox::MyTwinkleFox(uint16_t num_leds)
        : Fx1d(num_leds), backgroundColor(CRGB::Black),
          twinkleSpeed(1), twinkleDensity(1),
          coolLikeIncandescent(false),
          autoSelectBackgroundColor(true) {};

    void MyTwinkleFox::draw(DrawContext context)
    {
        EVERY_N_MILLISECONDS(10)
        {
            nblendPaletteTowardPalette(currentPalette, targetPalette, 12);
        }
        // Smooth transition using easing that starts slow and speeds up
        if (targetSpeed != twinkleSpeed) {
            EVERY_N_MILLISECONDS(1000) {
                twinkleSpeed += (twinkleSpeed < targetSpeed) ? 1 : -1;
                twinkleSpeed > 8 ? twinkleSpeed = 8 : twinkleSpeed;
            }
        }
        
        if (targetDensity != twinkleDensity) {
            EVERY_N_MILLISECONDS(1000) {
                    twinkleDensity += (twinkleDensity < targetDensity) ? 1 : -1;
                    twinkleDensity > 8 ? twinkleDensity = 8 : twinkleDensity;
            }
        }
        
        drawTwinkleFox(context.leds);
    }

    void MyTwinkleFox::setTwinkleSpeed(uint8_t speed) {
        Serial.print("Setting Twinkle Speed: ");
        Serial.println(speed);
        targetSpeed = speed; 
        Serial.print("Target Speed: ");
        Serial.println(targetSpeed);
    };
    void MyTwinkleFox::setTwinkleDensity(uint8_t density) {
        Serial.print("Setting Twinkle Density: ");
        Serial.println(density);
        targetDensity = density; 
        Serial.print("Target Density: ");
        Serial.println(targetDensity);    
    };

    void MyTwinkleFox::drawTwinkleFox(CRGB *leds)
    {
        uint16_t PRNG16 = 11337;
        uint32_t clock32 = millis();

        CRGB bg = backgroundColor;
        if (autoSelectBackgroundColor &&
            currentPalette[0] == currentPalette[1])
        {
            bg = currentPalette[0];
            uint8_t bglight = bg.getAverageLight();
            if (bglight > 64)
            {
                bg.nscale8_video(16);
            }
            else if (bglight > 16)
            {
                bg.nscale8_video(64);
            }
            else
            {
                bg.nscale8_video(86);
            }
        }

        uint8_t backgroundBrightness = bg.getAverageLight();

        for (uint16_t i = 0; i < mNumLeds; i++)
        {
            PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384;
            uint16_t myclockoffset16 = PRNG16;
            PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384;
            uint8_t myspeedmultiplierQ5_3 =
                ((((PRNG16 & 0xFF) >> 4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
            uint32_t myclock30 =
                (uint32_t)((clock32 * myspeedmultiplierQ5_3) >> 3) +
                myclockoffset16;
            uint8_t myunique8 = PRNG16 >> 8;

            CRGB c = computeOneTwinkle(myclock30, myunique8);

            uint8_t cbright = c.getAverageLight();
            int16_t deltabright = cbright - backgroundBrightness;
            if (deltabright >= 32 || (!bg))
            {
                leds[i] = c;
            }
            else if (deltabright > 0)
            {
                leds[i] = blend(bg, c, deltabright * 8);
            }
            else
            {
                leds[i] = bg;
            }
        }
    }

    CRGB MyTwinkleFox::computeOneTwinkle(uint32_t ms, uint8_t salt)
    {
        uint16_t ticks = ms >> (8 - twinkleSpeed);
        uint8_t fastcycle8 = ticks;
        uint16_t slowcycle16 = (ticks >> 8) + salt;
        slowcycle16 += sin8(slowcycle16);
        slowcycle16 = (slowcycle16 * 2053) + 1384;
        uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);

        uint8_t bright = 0;
        if (((slowcycle8 & 0x0E) / 2) < twinkleDensity)
        {
            bright = attackDecayWave8(fastcycle8);
        }

        uint8_t hue = slowcycle8 - salt;
        CRGB c;
        if (bright > 0)
        {
            c = ColorFromPalette(currentPalette, hue, bright, NOBLEND);
            if (coolLikeIncandescent)
            {
                coolLikeIncandescentFunction(c, fastcycle8);
            }
        }
        else
        {
            c = CRGB::Black;
        }
        return c;
    }

    uint8_t MyTwinkleFox::attackDecayWave8(uint8_t i)
    {
        if (i < 86)
        {
            return i * 3;
        }
        else
        {
            i -= 86;
            return 255 - (i + (i / 2));
        }
    }

    void MyTwinkleFox::coolLikeIncandescentFunction(CRGB &c, uint8_t phase)
    {
        if (phase < 128)
            return;

        uint8_t cooling = (phase - 128) >> 4;
        c.g = qsub8(c.g, cooling);
        c.b = qsub8(c.b, cooling * 2);
    }
}