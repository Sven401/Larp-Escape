#ifndef PALLETS_H
#define PALLETS_H

#include <FastLED.h>

// Color palettes
// Color palette definitions

// A mostly red palette with green accents and white trim.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 RedGreenWhite_p FL_PROGMEM = {
    CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
    CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray,
    CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green
};

const TProgmemRGBPalette16 Holly_p FL_PROGMEM = {
    0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c,
    0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c,
    0x00580c, 0x00580c, 0x00580c, 0xB00402
};

const TProgmemRGBPalette16 RedWhite_p FL_PROGMEM = {
    CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray,
    CRGB::Gray, CRGB::Gray, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
    CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray
};

const TProgmemRGBPalette16 BlueWhite_p FL_PROGMEM = {
    CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
    CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
    CRGB::Blue, CRGB::Gray, CRGB::Gray, CRGB::Gray
};

const TProgmemRGBPalette16 FairyLight_p = {
    CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight,
    CRGB(CRGB::FairyLight).nscale8_constexpr(uint8_t(128)).as_uint32_t(),
    CRGB(CRGB::FairyLight).nscale8_constexpr(uint8_t(128)).as_uint32_t(),
    CRGB::FairyLight, CRGB::FairyLight,
    CRGB(CRGB::FairyLight).nscale8_constexpr(64).as_uint32_t(),
    CRGB(CRGB::FairyLight).nscale8_constexpr(64).as_uint32_t(),
    CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight,
    CRGB::FairyLight, CRGB::FairyLight
};

const TProgmemRGBPalette16 Snow_p FL_PROGMEM = {
    0x304048, 0x304048, 0x304048, 0x304048, 0x304048, 0x304048,
    0x304048, 0x304048, 0x304048, 0x304048, 0x304048, 0x304048,
    0x304048, 0x304048, 0x304048, 0xE0F0FF
};

const TProgmemRGBPalette16 RetroC9_p FL_PROGMEM = {
    0xB80400, 0x902C02, 0xB80400, 0x902C02, 0x902C02, 0xB80400,
    0x902C02, 0xB80400, 0x046002, 0x046002, 0x046002, 0x046002,
    0x070758, 0x070758, 0x070758, 0x606820
};

const TProgmemRGBPalette16 Ice_p FL_PROGMEM = {
    0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
    0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
    0x182080, 0x182080, 0x182080, 0x5080C0
};

const TProgmemRGBPalette16 Blue_p FL_PROGMEM = {
    0x0000FF, 0x0000CC, 0x000099, 0x000066, 0x000033, 0x0033FF,
    0x0066FF, 0x0099FF, 0x00CCFF, 0x00FFFF, 0x3399FF, 0x3366FF,
    0x3333FF, 0x3300FF, 0x6600FF, 0x9900FF
};

const TProgmemRGBPalette16 Yellow_p FL_PROGMEM = {
    0xFFFF00, 0xFFCC00, 0xFF9900, 0xFF6600, 0xFF3300, 0xFFCC33,
    0xFFCC66, 0xFFCC99, 0xFFCCFF, 0xFFFF33, 0xFFFF66, 0xFFFF99,
    0xFFFFCC, 0xFFFF99, 0xFFFF66, 0xFFFF33
};

const TProgmemRGBPalette16 Green_p FL_PROGMEM = {
    0x00FF00, 0x00CC00, 0x009900, 0x006600, 0x003300, 0x33FF33,
    0x66FF66, 0x99FF99, 0xCCFFCC, 0x00FF33, 0x00FF66, 0x00FF99,
    0x00FFCC, 0x00FF99, 0x00FF66, 0x00FF33
};

const TProgmemRGBPalette16 Purple_p FL_PROGMEM = {
    0x800080, 0x993399, 0xB266B2, 0xCC99CC, 0xE5CCE5, 0x660066,
    0x4D004D, 0x330033, 0x1A001A, 0x990099, 0xB200B2, 0xCC00CC,
    0xE500E5, 0xFF00FF, 0xFF33FF, 0xFF66FF
};

const TProgmemRGBPalette16 Orange_p FL_PROGMEM = {
    0xFFA500, 0xFF8C00, 0xFF7F50, 0xFF6347, 0xFF4500, 0xFF4500,
    0xFF6347, 0xFF7F50, 0xFF8C00, 0xFFA500, 0xFFB347, 0xFFCC66,
    0xFFD700, 0xFFE4B5, 0xFFEFD5, 0xFFF8DC
};

const TProgmemRGBPalette16 Pink_p FL_PROGMEM = {
    0xFFC0CB, 0xFFB6C1, 0xFF69B4, 0xFF1493, 0xDB7093, 0xFF69B4,
    0xFFB6C1, 0xFFC0CB, 0xFF69B4, 0xFF1493, 0xDB7093, 0xFF69B4,
    0xFFB6C1, 0xFFC0CB, 0xFF69B4, 0xFF1493
};

const TProgmemRGBPalette16 Red_p FL_PROGMEM = {
    0xFF0000, 0xCC0000, 0x990000, 0x660000, 0x330000, 0xFF3333,
    0xFF6666, 0xFF9999, 0xFFCCCC, 0xFF0000, 0xCC0000, 0x990000,
    0x660000, 0x330000, 0xFF3333, 0xFF6666
};

const TProgmemRGBPalette16 White_p FL_PROGMEM = {
    0xFFFFFF, 0xF0F0F0, 0xE0E0E0, 0xD0D0D0, 0xC0C0C0, 0xB0B0B0,
    0xA0A0A0, 0x909090, 0x808080, 0x707070, 0x606060, 0x505050,
    0x404040, 0x303030, 0x202020, 0x101010
};

const TProgmemRGBPalette16 Turquoise_p FL_PROGMEM = {
    0x40E0D0, 0x48D1CC, 0x00CED1, 0x20B2AA, 0x5F9EA0, 0x008B8B,
    0x008080, 0x2E8B57, 0x3CB371, 0x66CDAA, 0x8FBC8F, 0x20B2AA,
    0x48D1CC, 0x40E0D0, 0x00CED1, 0x5F9EA0
};

const TProgmemRGBPalette16 MutedAllColors_p FL_PROGMEM = {
    0x404040, 0x606060, 0x808080, 0xA0A0A0, 0xC0C0C0, 0xE0E0E0,
    0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF,
    0xFFA500, 0xFFC0CB, 0x800080, 0x40E0D0
};

#endif // PALLETS_H