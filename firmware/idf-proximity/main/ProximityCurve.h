#pragma once
#include <array>
#include <cstdint>
namespace larp { namespace proximity {
struct CurvePoint { std::int8_t rssi; std::uint8_t percent; };
struct ProximityCurve {
    std::array<CurvePoint,4> points{{{-90,0},{-75,30},{-60,60},{-40,100}}};
    bool valid() const {
        for(unsigned i=0;i<4;++i) {
            if(points[i].rssi < -127 || points[i].rssi >= 0 || points[i].percent>100)return false;
            if(i&&(points[i].rssi<=points[i-1].rssi||points[i].percent<points[i-1].percent))return false;
        }
        return true;
    }
    std::uint16_t level(int rssi) const {
        if(rssi<=points[0].rssi)return points[0].percent*10U;
        for(unsigned i=1;i<4;++i)if(rssi<=points[i].rssi) {
            const auto &a=points[i-1],&b=points[i];
            return static_cast<std::uint16_t>(a.percent*10+(rssi-a.rssi)*10*(b.percent-a.percent)/(b.rssi-a.rssi));
        }
        return points[3].percent*10U;
    }
    std::array<std::uint8_t,8> encode() const {
        std::array<std::uint8_t,8> out{};
        for(unsigned i=0;i<4;++i){out[i*2]=static_cast<std::uint8_t>(points[i].rssi);out[i*2+1]=points[i].percent;}
        return out;
    }
    static bool decode(const std::uint8_t *p,ProximityCurve &out) {
        ProximityCurve c;
        for(unsigned i=0;i<4;++i){c.points[i].rssi=static_cast<std::int8_t>(p[i*2]);c.points[i].percent=p[i*2+1];}
        if(!c.valid())return false;
        out=c;return true;
    }
};
}}
