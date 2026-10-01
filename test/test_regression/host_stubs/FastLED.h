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