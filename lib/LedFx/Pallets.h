#ifndef PALLETS_H
#define PALLETS_H

#include <FastLED.h>

// Color palettes
// Color palette definitions

// A mostly red palette with green accents and white trim.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
/*
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
*/

const TProgmemRGBPalette16 Blue_p FL_PROGMEM = {
    0x0000FF, 0x0000CC, 0x003366, 0x00509E, 0x1E6091, 0x0077B6,
    0x0096C7, 0x00B4D8, 0x48CAE4, 0x90E0EF, 0x3399FF, 0x3366FF,
    0x4A69BD, 0x6A5ACD, 0x5A84E3, 0x8EA8E6};

const TProgmemRGBPalette16 Yellow_p FL_PROGMEM = {
    0xFFFF00, 0xFFCC00, 0xFF9900, 0xFF6600, 0xFF3300, 0xFFCC33,
    0xFFCC66, 0xFFCC99, 0xFFCCFF, 0xFFFF33, 0xFFFF66, 0xFFFF99,
    0xFFFFCC, 0xFFFF99, 0xFFFF66, 0xFFFF33};

const TProgmemRGBPalette16 Green_p FL_PROGMEM = {
    0x4CAF50, 0x388E3C, 0x2E7D32, 0x1B5E20, 0x0D3B15, 0x81C784,
    0xA5D6A7, 0xC8E6C9, 0xE8F5E9, 0x66BB6A, 0x57A773, 0x5FAD56,
    0x8BC34A, 0x9CCC65, 0xB2DFDB, 0xD0ECE7};

const TProgmemRGBPalette16 Purple_p FL_PROGMEM = {
    0x800080, 0x993399, 0xB266B2, 0xCC99CC, 0xE5CCE5, 0x660066,
    0x4D004D, 0xA569BD, 0xD8BFD8, 0x990099, 0xB200B2, 0xCC00CC,
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
    0xCC0000, 0x990000, 0x800020, 0xB22222, 0xFF5A36, 0xFF6666,
    0xFF9999, 0xFFCCCC, 0xFFD700, 0xFFA07A, 0xE34234, 0xD72638,
    0xC21807, 0xFF8C42, 0xFA8072, 0xFFC0CB};

const TProgmemRGBPalette16 White_p FL_PROGMEM = {
    0xFFFFFF, 0xF8F8F0, 0xF0F0E8, 0xE8F0F8, 0xE0E0F0, 0xF0E8F8,
    0xF8E0E0, 0xF8F0D0, 0xE8F8D0, 0xD0F8E8, 0xD0E8F8, 0xF0D0F8,
    0xF8D0E0, 0xE0F8F0, 0xF0F8D0, 0xF8E8D0};

const TProgmemRGBPalette16 Turquoise_p FL_PROGMEM = {
    0x40E0D0, 0x48D1CC, 0x00CED1, 0x20B2AA, 0x5F9EA0, 0x008B8B,
    0x008080, 0x2E8B57, 0x3CB371, 0x66CDAA, 0x8FBC8F, 0x20B2AA,
    0x48D1CC, 0x40E0D0, 0x00CED1, 0x5F9EA0};

const TProgmemRGBPalette16 MutedAllColors_p FL_PROGMEM = {
    0xFFA97F, 0xFFE08C, 0xA8C89E, 0x85A88A, 0xD0E3A8, 0xB7D3C3,
    0x8DA6B1, 0xA0B6D3, 0xB7D3E8, 0xE3D0E3, 0xEFC5DE, 0xD98A9C,
    0xFFA08A, 0xFFB39A, 0xFFD98A, 0xFFC7D1};

#endif // PALLETS_H