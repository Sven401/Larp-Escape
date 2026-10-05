#pragma once

#include <array>
#include <cstdint>

using TProgmemRGBPalette16 = std::array<uint8_t, 16>;

class HostSerial
{
public:
    template <typename T>
    void print(const T &) {}

    template <typename T>
    void println(const T &) {}
};

extern HostSerial Serial;
// Minimal value arithmetic for adapter bounds/color contracts. This does not
// emulate FastLED drivers, timing, palettes or physical light output.
struct CRGB {
    std::uint8_t r, g, b;
    CRGB(std::uint8_t red = 0U, std::uint8_t green = 0U, std::uint8_t blue = 0U)
        : r(red), g(green), b(blue) {}
    static std::uint8_t add(std::uint8_t left, std::uint8_t right) {
        const unsigned value = static_cast<unsigned>(left) + right;
        return static_cast<std::uint8_t>(value > 255U ? 255U : value);
    }
    CRGB &operator+=(const CRGB &other) {
        r = add(r, other.r); g = add(g, other.g); b = add(b, other.b); return *this;
    }
    CRGB scale8(const CRGB &scale) const {
        return CRGB(static_cast<std::uint8_t>(static_cast<unsigned>(r) * (scale.r + 1U) >> 8U),
                    static_cast<std::uint8_t>(static_cast<unsigned>(g) * (scale.g + 1U) >> 8U),
                    static_cast<std::uint8_t>(static_cast<unsigned>(b) * (scale.b + 1U) >> 8U));
    }
};
