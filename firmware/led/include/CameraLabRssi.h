#pragma once
#include "LedWire.h"

namespace larp { namespace led { namespace camera {
// Laboratory-only request owner. Radio callbacks remain behind the existing
// EspNowRadio queue; only the main loop services this controller.
class RssiRadio {
public:
    virtual ~RssiRadio() = default;
    virtual void service(std::uint32_t now) = 0;
    virtual ledwire::EspNowSnapshot snapshot() = 0;
    virtual bool submit(const std::array<std::uint8_t, 6> &peer, std::uint32_t now) = 0;
    virtual void cancel() = 0;
};
struct RssiResult {
    char token[17] = {};
    std::uint32_t sample = 0, now = 0;
    ledwire::EspNowSnapshot radio;
};
class RssiObserver {
public:
    virtual ~RssiObserver() = default;
    virtual void measuring(const char *token, std::uint32_t sample, std::uint32_t now) = 0;
    virtual void rejected(const char *token, std::uint32_t sample, const char *reason, std::uint32_t now) = 0;
    virtual void measured(const RssiResult &result) = 0;
};
class RssiController {
public:
    RssiController(RssiRadio &radio, RssiObserver &observer) : radio_(radio), observer_(observer) {}
    void begin(const char *token);
    void command(const char *line, std::uint32_t now);
    // Called on every loop, including idle and LED stop-drain paths. Results
    // carry exactly the time passed to the radio's service, not a later clock.
    void service(std::uint32_t now);
    // Emits a pending cancellation (outcome 14) before cancelling the radio.
    // The radio keeps its outstanding send-callback fence until drained.
    void end(std::uint32_t now);
private:
    RssiResult result(std::uint32_t now);
    RssiRadio &radio_;
    RssiObserver &observer_;
    char token_[17] = "0000000000000000";
    std::uint32_t lastSample_ = 0, pendingSample_ = 0;
    bool active_ = false, pending_ = false;
};
}}}
