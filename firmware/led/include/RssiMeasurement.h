#pragma once
#include "EspNowTest.h"
namespace larp { namespace rssi {
struct Observation { espnowtest::Peer source{}, destination{}; espnowtest::Packet packet{}; int strength = 0; };
// IDF 4.4 sniffer: 24-byte management header, action header, vendor IE, FCS.
inline bool decode(const std::uint8_t *p, std::size_t size, int strength, Observation &out) {
    if (!p || size != 107 || strength < -127 || strength >= 0 || p[0] != 0xd0 || (p[1] & 0x43) ||
        p[24] != 127 || p[25] != 0x18 || p[26] != 0xfe || p[27] != 0x34 ||
        p[32] != 221 || p[33] != 69 || p[34] != 0x18 || p[35] != 0xfe || p[36] != 0x34 ||
        p[37] != 4 || p[38] != 1 || !espnowtest::validPacket(p + 39, 64, true)) return false;
    std::copy(p + 4, p + 10, out.destination.begin());
    std::copy(p + 10, p + 16, out.source.begin());
    std::copy(p + 39, p + 103, out.packet.begin()); out.strength = strength;
    return espnowtest::validPeer(out.source);
}
class Filter {
    int q_ = 0; bool valid_ = false; std::uint32_t at_ = 0;
public:
    void clear() { valid_ = false; q_ = 0; }
    void sample(int value, std::uint32_t now) {
        if (value < -127 || value >= 0) return;
        if (!valid(now)) q_ = value * 256;
        else q_ += (value * 256 - q_) / 4;
        valid_ = true; at_ = now;
    }
    bool valid(std::uint32_t now) const { return valid_ && now - at_ < 3000U; }
    int value() const { return q_ / 256; }
    unsigned target(std::uint32_t now) const {
        if (!valid(now)) return 20;
        const int v = value(); return v <= -90 ? 20 : v >= -40 ? 100 : 20 + (v + 90) * 80 / 50;
    }
};
}}
