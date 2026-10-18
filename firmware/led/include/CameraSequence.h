#pragma once
#include "LedOutput.h"

namespace larp { namespace led { namespace camera {
struct Step {
    int index; // -1 all pixels, -2 effect, -3 finished
    Pixel pixel;
    std::uint32_t duration;
    Effect effect;
};
inline Pixel rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    Pixel p; p.red = r; p.green = g; p.blue = b; return p;
}
inline Step dark(std::uint32_t duration) { return {-1, {}, duration, Effect::Off}; }
inline Step step(unsigned number) {
    // Optical sync: three separate markers, before any mapping/effect fit.
    // Different durations sweep the camera sampling phase instead of repeating
    // an almost exact multiple of its 30-Hz frame interval.
    constexpr std::uint32_t markers[] = {419,457,491,523,557,593};
    if (number < 6) return number % 2 ? dark(markers[number]) :
        Step{-1, rgb(16,16,16), markers[number], Effect::Off};
    number -= 6;
    // Training, then held-out reversed order at a different brightness.
    if (number < 92) {
        const bool verification = number >= 46;
        const unsigned local = number % 46;
        if (local % 2) return dark(200);
        const int index = verification ? 22 - int(local / 2) : int(local / 2);
        const std::uint8_t value = verification ? 8 : 16;
        return {index, rgb(value,value,value), 400, Effect::Off};
    }
    number -= 92;
    // Empirical camera response for both actual product color families.
    constexpr std::uint8_t levels[] = {1,2,4,8,16,24,32};
    if (number < 28) {
        if (number % 2) return dark(200);
        const auto value = levels[(number / 2) % 7];
        const bool cool = number >= 14;
        return {-1, rgb(std::uint8_t(cool ? value/4 : value),
                        std::uint8_t(cool ? value/4 : value), value), 400, Effect::Off};
    }
    number -= 28;
    if (number < 6) {
        if (number % 2) return dark(200);
        Pixel color{};
        if (number == 0) color.red = 32;
        if (number == 2) color.green = 32;
        if (number == 4) color.blue = 32;
        return {-1, color, 400, Effect::Off};
    }
    if (number == 6) return dark(1000);
    return {-3, {}, 0, Effect::Off};
}
inline bool validToken(const char *token) {
    unsigned count = 0;
    while (*token) {
        const char c = *token++;
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) || ++count > 16) return false;
    }
    return count == 16;
}
}}}
