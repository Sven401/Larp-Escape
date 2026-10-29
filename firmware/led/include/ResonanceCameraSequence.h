#pragma once
#include "LedOutput.h"
#include "CameraSequence.h"

namespace larp { namespace led { namespace camera {
inline Step resonanceColorStep(unsigned number) {
    constexpr std::uint8_t levels[] = {1,2,4,8,16,24,32};
    if (number >= 14) return {-3, {}, 0, Effect::Off};
    if (number % 2) return dark(200);
    const auto value = levels[number/2];
    return {-1, rgb(value/6, value*2/3, value), 400, Effect::Off};
}
// Synthetic observations at the same boundary used by ProductMain. No MCP or
// BLE is present in the isolated optical lab image.
struct ResonanceStep {
    std::uint32_t duration;
    bool valid;
    std::uint8_t mask;
    Effect effect;
    bool restart;
};
inline ResonanceStep resonanceStep(unsigned number, bool transitions) {
    if (!transitions) {
        // Zero, each single input, every pair, every triple, then all four.
        constexpr std::uint8_t masks[] = {0,1,2,4,8,3,5,9,6,10,12,7,11,13,14,15};
        if (number < 16) return {3200, true, masks[number], Effect::Resonance, number == 0};
    } else {
        static const ResonanceStep steps[] = {
            {1600,true,15,Effect::Resonance,true},
            {1400,true,14,Effect::Resonance,false}, {1400,true,15,Effect::Resonance,false},
            {1400,true,13,Effect::Resonance,false}, {1400,true,15,Effect::Resonance,false},
            {1400,true,11,Effect::Resonance,false}, {1400,true,15,Effect::Resonance,false},
            {1400,true,7,Effect::Resonance,false}, {1400,true,15,Effect::Resonance,false},
            {1400,false,15,Effect::Resonance,false}, {1400,true,15,Effect::Resonance,false},
            {1400,true,255,Effect::Resonance,false}, {1400,true,15,Effect::Resonance,false},
            {1400,true,15,Effect::Resonance,true},
            {1000,true,15,Effect::Off,true},
            {1000,true,1,Effect::Off,false}, // Wake alone must not restart.
            {3200,true,3,Effect::Resonance,true},
            {1000,true,3,Effect::Off,true},
            {1200,true,15,Effect::Resonance,true},
            {1800,true,15,Effect::Ripple,true},
            {1200,true,15,Effect::Resonance,true},
            {1200,true,15,Effect::Twinkle,true},
        };
        if (number < sizeof(steps)/sizeof(steps[0])) return steps[number];
    }
    return {0, true, 0, Effect::Off, false};
}
}}}
