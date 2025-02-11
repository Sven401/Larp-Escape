#pragma once

#include "FastLED.h"
#include "fx/fx1d.h"
#include "fl/str.h"
#include "fl/namespace.h"

namespace fl
{

/// @file    TwinkleFox.hpp
/// @brief   Twinkling "holiday" lights that fade in and out.
/// @example TwinkleFox.ino

//  TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
//  Colors are chosen from a palette; a few palettes are provided.
//
//  This December 2015 implementation improves on the December 2014 version
//  in several ways:
//  - smoother fading, compatible with any colors and any palettes
//  - easier control of twinkle speed and twinkle density
//  - supports an optional 'background color'
//  - takes even less RAM: zero RAM overhead per pixel
//  - illustrates a couple of interesting techniques (uh oh...)
//
//  The idea behind this (new) implementation is that there's one
//  basic, repeating pattern that each pixel follows like a waveform:
//  The brightness rises from 0..255 and then falls back down to 0.
//  The brightness at any given point in time can be determined as
//  as a function of time, for example:
//    brightness = sine( time ); // a sine wave of brightness over time
//
//  So the way this implementation works is that every pixel follows
//  the exact same wave function over time.  In this particular case,
//  I chose a sawtooth triangle wave (triwave8) rather than a sine wave,
//  but the idea is the same: brightness = triwave8( time ).
//
//  The triangle wave function is a piecewise linear function that looks like:
//
//     / \\ 
//    /     \\ 
//   /         \\ 
//  /             \\ 
//
//  Of course, if all the pixels used the exact same wave form, and
//  if they all used the exact same 'clock' for their 'time base', all
//  the pixels would brighten and dim at once -- which does not look
//  like twinkling at all.
//
//  So to achieve random-looking twinkling, each pixel is given a
//  slightly different 'clock' signal.  Some of the clocks run faster,
//  some run slower, and each 'clock' also has a random offset from zero.
//  The net result is that the 'clocks' for all the pixels are always out
//  of sync from each other, producing a nice random distribution
//  of twinkles.
//
//  The 'clock speed adjustment' and 'time offset' for each pixel
//  are generated randomly.  One (normal) approach to implementing that
//  would be to randomly generate the clock parameters for each pixel
//  at startup, and store them in some arrays.  However, that consumes
//  a great deal of precious RAM, and it turns out to be totally
//  unnessary!  If the random number generate is 'seeded' with the
//  same starting value every time, it will generate the same sequence
//  of values every time.  So the clock adjustment parameters for each
//  pixel are 'stored' in a pseudo-random number generator!  The PRNG
//  is reset, and then the first numbers out of it are the clock
//  adjustment parameters for the first pixel, the second numbers out
//  of it are the parameters for the second pixel, and so on.
//  In this way, we can 'store' a stable sequence of thousands of
//  random clock adjustment parameters in literally two bytes of RAM.
//
//  There's a little bit of fixed-point math involved in applying the
//  clock speed adjustments, which are expressed in eighths.  Each pixel's
//  clock speed ranges from 8/8ths of the system clock (i.e. 1x) to
//  23/8ths of the system clock (i.e. nearly 3x).
//
//  On a basic Arduino Uno or Leonardo, this code can twinkle 300+ pixels
//  smoothly at over 50 updates per seond.
//
//  -Mark Kriegsman, December 2015

//  Adapted for FastLED 3.x in August 2023 by Marlin Unruh

// TwinkleFox effect parameters
// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).
// 4, 5, and 6 are recommended, default is 4.
#define TWINKLE_SPEED 4

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).
// Default is 5.
#define TWINKLE_DENSITY 5

// How often to change color palettes.
#define SECONDS_PER_PALETTE 30

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 0

// If COOL_LIKE_INCANDESCENT is set to 1, colors will
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 1

    FASTLED_SMART_PTR(TwinkleFox);

    class TwinkleFox : public Fx1d
    {
    public:
        CRGBPalette16 targetPalette;
        CRGBPalette16 currentPalette;

        TwinkleFox(uint16_t num_leds)
            : Fx1d(num_leds), backgroundColor(CRGB::Black),
              twinkleSpeed(TWINKLE_SPEED), twinkleDensity(TWINKLE_DENSITY),
              coolLikeIncandescent(COOL_LIKE_INCANDESCENT),
              autoSelectBackgroundColor(AUTO_SELECT_BACKGROUND_COLOR)
        {
            chooseNextColorPalette(targetPalette);
        }

        void draw(DrawContext context) override
        {
            EVERY_N_MILLISECONDS(10)
            {
                nblendPaletteTowardPalette(currentPalette, targetPalette, 12);
            }
            drawTwinkleFox(context.leds);
            if (targetSpeed != twinkleSpeed || targetDensity != twinkleDensity)
            {
                EVERY_N_MILLISECONDS(500)
                {
                    twinkleSpeed = qadd8(twinkleSpeed, targetSpeed - twinkleSpeed);
                    twinkleDensity = qadd8(twinkleDensity, targetDensity - twinkleDensity);
                }
            };
        }

        void setTwinkleSpeed(uint8_t speed) { targetSpeed = speed; };
        void setTwinkleDensity(uint8_t density) { targetDensity = density; };

        void chooseNextColorPalette(CRGBPalette16 &pal);
        fl::Str fxName() const override { return "TwinkleFox"; }

    private:
        uint8_t targetSpeed;
        uint8_t targetDensity;
        CRGB backgroundColor;
        uint8_t twinkleSpeed;
        uint8_t twinkleDensity;
        bool coolLikeIncandescent;
        bool autoSelectBackgroundColor;

        void drawTwinkleFox(CRGB *leds)
        {
            // "PRNG16" is the pseudorandom number generator
            // It MUST be reset to the same starting value each time
            // this function is called, so that the sequence of 'random'
            // numbers that it generates is (paradoxically) stable.
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

        CRGB computeOneTwinkle(uint32_t ms, uint8_t salt)
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

        uint8_t attackDecayWave8(uint8_t i)
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

        void coolLikeIncandescentFunction(CRGB &c, uint8_t phase)
        {
            if (phase < 128)
                return;

            uint8_t cooling = (phase - 128) >> 4;
            c.g = qsub8(c.g, cooling);
            c.b = qsub8(c.b, cooling * 2);
        }
    };

    // Color palettes
    // Color palette definitions
    // A mostly red palette with green accents and white trim.
    // "CRGB::Gray" is used as white to keep the brightness more uniform.
    const TProgmemRGBPalette16 RedGreenWhite_p FL_PROGMEM = {
        CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
        CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray,
        CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green};

    const TProgmemRGBPalette16 Holly_p FL_PROGMEM = {
        0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c,
        0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c,
        0x00580c, 0x00580c, 0x00580c, 0xB00402};

    const TProgmemRGBPalette16 RedWhite_p FL_PROGMEM = {
        CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray,
        CRGB::Gray, CRGB::Gray, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
        CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray};

    const TProgmemRGBPalette16 BlueWhite_p FL_PROGMEM = {
        CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
        CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
        CRGB::Blue, CRGB::Gray, CRGB::Gray, CRGB::Gray};

    const TProgmemRGBPalette16 FairyLight_p = {
        CRGB::FairyLight,
        CRGB::FairyLight,
        CRGB::FairyLight,
        CRGB::FairyLight,
        CRGB(CRGB::FairyLight).nscale8_constexpr(uint8_t(128)).as_uint32_t(),
        CRGB(CRGB::FairyLight).nscale8_constexpr(uint8_t(128)).as_uint32_t(),
        CRGB::FairyLight,
        CRGB::FairyLight,
        CRGB(CRGB::FairyLight).nscale8_constexpr(64).as_uint32_t(),
        CRGB(CRGB::FairyLight).nscale8_constexpr(64).as_uint32_t(),
        CRGB::FairyLight,
        CRGB::FairyLight,
        CRGB::FairyLight,
        CRGB::FairyLight,
        CRGB::FairyLight,
        CRGB::FairyLight};

    const TProgmemRGBPalette16 Snow_p FL_PROGMEM = {
        0x304048, 0x304048, 0x304048, 0x304048, 0x304048, 0x304048,
        0x304048, 0x304048, 0x304048, 0x304048, 0x304048, 0x304048,
        0x304048, 0x304048, 0x304048, 0xE0F0FF};

    const TProgmemRGBPalette16 RetroC9_p FL_PROGMEM = {
        0xB80400, 0x902C02, 0xB80400, 0x902C02, 0x902C02, 0xB80400,
        0x902C02, 0xB80400, 0x046002, 0x046002, 0x046002, 0x046002,
        0x070758, 0x070758, 0x070758, 0x606820};

    const TProgmemRGBPalette16 Ice_p FL_PROGMEM = {
        0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
        0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
        0x182080, 0x182080, 0x182080, 0x5080C0};
        
    const TProgmemRGBPalette16 Blue_p FL_PROGMEM = {
        0x0000FF, 0x0000CC, 0x000099, 0x000066, 0x000033, 0x0033FF,
        0x0066FF, 0x0099FF, 0x00CCFF, 0x00FFFF, 0x3399FF, 0x3366FF,
        0x3333FF, 0x3300FF, 0x6600FF, 0x9900FF};

    const TProgmemRGBPalette16 Yellow_p FL_PROGMEM = {
        0xFFFF00, 0xFFCC00, 0xFF9900, 0xFF6600, 0xFF3300, 0xFFCC33,
        0xFFCC66, 0xFFCC99, 0xFFCCFF, 0xFFFF33, 0xFFFF66, 0xFFFF99,
        0xFFFFCC, 0xFFFF99, 0xFFFF66, 0xFFFF33};

    const TProgmemRGBPalette16 Green_p FL_PROGMEM = {
        0x00FF00, 0x00CC00, 0x009900, 0x006600, 0x003300, 0x33FF33,
        0x66FF66, 0x99FF99, 0xCCFFCC, 0x00FF33, 0x00FF66, 0x00FF99,
        0x00FFCC, 0x00FF99, 0x00FF66, 0x00FF33};

    const TProgmemRGBPalette16 Purple_p FL_PROGMEM = {
        0x800080, 0x993399, 0xB266B2, 0xCC99CC, 0xE5CCE5, 0x660066,
        0x4D004D, 0x330033, 0x1A001A, 0x990099, 0xB200B2, 0xCC00CC,
        0xE500E5, 0xFF00FF, 0xFF33FF, 0xFF66FF};

    const TProgmemRGBPalette16 Orange_p FL_PROGMEM = {
        0xFFA500, 0xFF8C00, 0xFF7F50, 0xFF6347, 0xFF4500, 0xFF4500,
        0xFF6347, 0xFF7F50, 0xFF8C00, 0xFFA500, 0xFFB347, 0xFFCC66,
        0xFFD700, 0xFFE4B5, 0xFFEFD5, 0xFFF8DC};

    const TProgmemRGBPalette16 Pink_p FL_PROGMEM = {
        0xFFC0CB, 0xFFB6C1, 0xFF69B4, 0xFF1493, 0xDB7093, 0xFF69B4,
        0xFFB6C1, 0xFFC0CB, 0xFF69B4, 0xFF1493, 0xDB7093, 0xFF69B4,
        0xFFB6C1, 0xFFC0CB, 0xFF69B4, 0xFF1493};

    const TProgmemRGBPalette16 Red_p FL_PROGMEM = {
        0xFF0000, 0xCC0000, 0x990000, 0x660000, 0x330000, 0xFF3333,
        0xFF6666, 0xFF9999, 0xFFCCCC, 0xFF0000, 0xCC0000, 0x990000,
        0x660000, 0x330000, 0xFF3333, 0xFF6666};

    const TProgmemRGBPalette16 White_p FL_PROGMEM = {
        0xFFFFFF, 0xF0F0F0, 0xE0E0E0, 0xD0D0D0, 0xC0C0C0, 0xB0B0B0,
        0xA0A0A0, 0x909090, 0x808080, 0x707070, 0x606060, 0x505050,
        0x404040, 0x303030, 0x202020, 0x101010};

    const TProgmemRGBPalette16 Turquoise_p FL_PROGMEM = {
        0x40E0D0, 0x48D1CC, 0x00CED1, 0x20B2AA, 0x5F9EA0, 0x008B8B,
        0x008080, 0x2E8B57, 0x3CB371, 0x66CDAA, 0x8FBC8F, 0x20B2AA,
        0x48D1CC, 0x40E0D0, 0x00CED1, 0x5F9EA0};

    const TProgmemRGBPalette16 MutedAllColors_p FL_PROGMEM = {
        0x404040, 0x606060, 0x808080, 0xA0A0A0, 0xC0C0C0, 0xE0E0E0,
        0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF,
        0xFFA500, 0xFFC0CB, 0x800080, 0x40E0D0};

    // Add or remove palette names from this list to control which color
    // palettes are used, and in what order.
    const TProgmemRGBPalette16 *ActivePaletteList[] = {
        &RetroC9_p, &BlueWhite_p, &RainbowColors_p, &FairyLight_p,
        &RedGreenWhite_p, &PartyColors_p, &RedWhite_p, &Snow_p,
        &Holly_p, &Ice_p};

    void TwinkleFox::chooseNextColorPalette(CRGBPalette16 &pal)
    {
        const uint8_t numberOfPalettes =
            sizeof(ActivePaletteList) / sizeof(ActivePaletteList[0]);
        static uint8_t whichPalette = -1;
        whichPalette = addmod8(whichPalette, 1, numberOfPalettes);
        pal = *(ActivePaletteList[whichPalette]);
    }

} // namespace fl
