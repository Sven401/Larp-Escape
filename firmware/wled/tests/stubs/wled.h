#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#define PROGMEM
#define OUTPUT_MAX_PINS 5
#define TYPE_WS2812_RGB 22
#define COL_ORDER_GRB 0
#define RGBW_MODE_MANUAL_ONLY 0
#define REALTIME_MODE_INACTIVE 0
#define REALTIME_OVERRIDE_NONE 0
#define SEG_OPTION_FREEZE 4
#define SEG_OPTION_ON 2
#define FX_MODE_STATIC 0
#define FX_MODE_FADE 12
#define FX_MODE_TWINKLEFOX 80
#define ERR_NORAM 8
#define ERR_NORAM_PX 7
#define BLACK 0

extern std::uint32_t fakeNow;
inline std::uint32_t millis() { return fakeNow; }
extern bool turnOnAtBoot, transitionActive, nightlightActive, gammaCorrectCol, gammaCorrectBri, offMode;
extern std::uint8_t bootPreset, bri, briT, briOld, briS, briLast, briMultiplier, realtimeMode, realtimeOverride, errorFlag;
extern float gammaCorrectVal;
struct NeoGammaWLEDMethod { static void calcGammaTable(float) {} };
struct BusConfig {
    std::uint8_t type, pin, order;
    std::uint16_t start, count;
    BusConfig(std::uint8_t t, std::uint8_t *p, std::uint16_t s, std::uint16_t n,
              std::uint8_t o, bool, std::uint8_t, std::uint8_t, std::uint16_t,
              std::uint8_t, std::uint16_t, std::uint8_t) : type(t), pin(p[0]), order(o), start(s), count(n) {}
};
extern std::vector<BusConfig> busConfigs;
struct Bus {
    BusConfig config;
    bool valid = true;
    explicit Bus(const BusConfig &c) : config(c) {}
    bool isOk() const { return valid; }
    bool isPlaceholder() const { return false; }
    std::uint8_t getType() const { return config.type; }
    std::uint16_t getStart() const { return config.start; }
    std::uint16_t getLength() const { return config.count; }
    std::uint8_t getColorOrder() const { return config.order; }
    unsigned getPins(std::uint8_t *pins) const { pins[0] = config.pin; return 1; }
};
namespace BusManager {
extern std::vector<Bus> buses;
inline Bus *getBus(unsigned i) { return i < buses.size() ? &buses[i] : nullptr; }
inline unsigned getNumBusses() { return buses.size(); }
inline void setMilliampsMax(std::uint16_t) {}
}
struct Segment {
    bool on = true, active = true, reset = false, check1 = false, check2 = false, check3 = false;
    std::uint8_t speed = 0, intensity = 0, custom1 = 0, custom2 = 0, custom3 = 0, mode = 0;
    unsigned call = 0;
    std::array<std::uint8_t,24> storage{};
    std::uint8_t *data = nullptr;
    bool isActive() const { return active; }
    bool is2D() const { return false; }
    void refreshLightCapabilities() {}
    void setOption(unsigned o, bool value) { if (o == SEG_OPTION_ON) on = value; }
    void setOpacity(unsigned) {}
    void setColor(unsigned, std::uint32_t) {}
    void setPalette(unsigned) {}
    void setMode(std::uint8_t m) { if (mode != m) reset = true; mode = m; }
    void markForReset() { reset = true; }
    void fill(std::uint32_t) {}
};
struct FakeStrip {
    Segment segment;
    bool isMatrix = false, updating = false, servicing = false, triggered = false, corruptBlack = false;
    unsigned segmentCount = 0, restarts = 0;
    std::uint8_t brightness = 0;
    std::uint32_t lastShow = 0, timebase = 0;
    std::array<std::uint32_t,23> pixels{};
    void (*ripple)() = nullptr;
    bool isServicing() const { return servicing; }
    bool isUpdating() const { return updating; }
    unsigned getLengthTotal() const { return 23; }
    unsigned getSegmentsNum() const { return segmentCount; }
    Segment &getSegment(unsigned) { return segment; }
    void suspend() {}
    void setTransition(unsigned) {}
    void setBrightness(std::uint8_t v) { brightness = v; }
    std::uint8_t getBrightness() const { return brightness; }
    void setTargetFps(unsigned) {}
    void finalizeInit() { for (auto &c : busConfigs) BusManager::buses.emplace_back(c); busConfigs.clear(); }
    void resetSegments() { segmentCount = 1; segment = Segment(); }
    std::uint8_t addEffect(std::uint8_t, void (*cb)(), const char *) { ripple = cb; return 240; }
    void setTransitionMode(bool) {}
    void restartRuntime() { ++restarts; segment.reset = true; }
    void resetTimebase() { timebase = fakeNow; }
    std::uint32_t getLastShow() const { return lastShow; }
    void trigger() { triggered = true; }
    std::uint32_t getPixelColorNoMap(unsigned i) const { return pixels[i]; }
    void frame() {
        if (segment.reset) { segment.call = 0; segment.storage.fill(0); segment.reset = false; }
        if (segment.mode == 240) ripple();
        ++segment.call;
        pixels.fill(segment.on && brightness ? 0xFFFFFF : (corruptBlack ? 1 : 0));
        lastShow = ++fakeNow;
        updating = true;
        triggered = false;
    }
};
extern FakeStrip strip;
#define SEGMENT strip.segment
#define SEGENV strip.segment
#define SEGLEN 23U
