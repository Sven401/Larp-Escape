#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "Gamestate.h"

namespace game_test {
class FakeClock : public larp::ports::IClock {
  public:
    explicit FakeClock(std::uint32_t initial = 0U) : now(initial) {}
    std::uint32_t nowMilliseconds() const override {
        ++nowReads;
        return now;
    }
    void delayMilliseconds(std::uint32_t) override {
        ++delayCalls;
    }
    std::uint32_t now;
    mutable unsigned int nowReads = 0U;
    unsigned int delayCalls = 0U;
};

class FakeCardReader : public larp::ports::ICardReader {
  public:
    larp::domain::CardUid readCard(std::uint16_t timeout) override {
        timeouts.push_back(timeout);
        return present;
    }
    larp::domain::CardUid present{};
    std::vector<std::uint16_t> timeouts;
};

class FakeCrystalInput : public larp::ports::ICrystalInput {
  public:
    larp::ports::CrystalInputBatch readPressedPositions() override {
        ++reads;
        return positions;
    }
    larp::ports::CrystalInputBatch positions;
    unsigned int reads = 0U;
};

class FakeWakeInput : public larp::ports::IWakeInput {
  public:
    std::array<bool, 4> readWakeSignals() override {
        ++reads;
        return signals;
    }
    std::array<bool, 4> signals{{false, false, false, false}};
    unsigned int reads = 0U;
};

class FakeAudioPlayer : public larp::ports::IAudioPlayer {
  public:
    void playTrack(std::uint16_t track) override {
        tracks.push_back(track);
        busy = playSetsBusy;
    }
    bool isBusy() override {
        ++busyReads;
        return busy;
    }
    void wakeup() override {
        ++wakeups;
    }
    void finish() {
        busy = false;
    }
    std::vector<std::uint16_t> tracks;
    unsigned int wakeups = 0U;
    unsigned int busyReads = 0U;
    bool busy = false;
    bool playSetsBusy = true;
};

struct RippleRequest {
    std::uint8_t index;
    std::uint8_t frames;
    std::uint8_t origin;
};

struct BrightnessRequest {
    larp::ports::BrightnessRamp ramp;
    std::uint8_t target;
    std::uint16_t interval;
};

struct PaletteRequest {
    larp::domain::RoleId role;
    larp::domain::GridPosition position;
};

class FakeLightEffects : public larp::ports::ILightEffects {
  public:
    void setTwinkleDensity(std::uint8_t density) override {
        densities.push_back(density);
    }
    void setTwinkleSpeed(std::uint8_t speed) override {
        speeds.push_back(speed);
    }
    void setMutedPalette() override {
        ++mutedPalettes;
    }
    void setCrystalPalette(larp::domain::RoleId role,
                           const larp::domain::GridPosition &position) override {
        palettes.push_back({role, position});
    }
    void fireBurst(std::uint8_t frames) override {
        bursts.push_back(frames);
    }
    void fireBurst(std::uint8_t frames, larp::ports::RgbColor color) override {
        bursts.push_back(frames);
        burstColors.push_back(color);
    }
    void fireRipple(std::uint8_t index, std::uint8_t frames, std::uint8_t origin) override {
        ripples.push_back({index, frames, origin});
    }
    void stepBrightnessToward(larp::ports::BrightnessRamp ramp, std::uint8_t target,
                              std::uint16_t interval) override {
        brightness.push_back({ramp, target, interval});
    }
    std::vector<std::uint8_t> densities;
    std::vector<std::uint8_t> speeds;
    unsigned int mutedPalettes = 0U;
    std::vector<PaletteRequest> palettes;
    std::vector<std::uint8_t> bursts;
    std::vector<larp::ports::RgbColor> burstColors;
    std::vector<RippleRequest> ripples;
    std::vector<BrightnessRequest> brightness;
};

class FakeCommunicator : public larp::ports::ICommunicator {
  public:
    bool send(larp::ports::GameMessage message) override {
        messages.push_back(message);
        return succeeds;
    }
    std::vector<larp::ports::GameMessage> messages;
    bool succeeds = true;
};

struct DiagnosticRecord {
    larp::ports::GameDiagnosticEvent event;
    std::uint32_t value;
    std::uint32_t auxiliary;
};

class FakeDiagnostics : public larp::ports::IGameDiagnostics {
  public:
    void report(larp::ports::GameDiagnosticEvent event, std::uint32_t value,
                std::uint32_t auxiliary) override {
        records.push_back({event, value, auxiliary});
    }
    bool contains(larp::ports::GameDiagnosticEvent event) const {
        return std::find_if(records.begin(), records.end(), [event](const DiagnosticRecord &item) {
                   return item.event == event;
               }) != records.end();
    }
    std::vector<DiagnosticRecord> records;
};

struct GameRig {
    explicit GameRig(std::uint32_t initialTime = 0U, bool diagnosticsEnabled = true)
        : clock(initialTime), game(cards, crystals, wake, audio, clock, lights, communicator,
                                   diagnosticsEnabled ? &diagnostics : nullptr) {}
    void tick(std::uint32_t elapsed = 0U) {
        clock.now += elapsed;
        game.machine.run();
    }
    larp::application::GamePhase phase() const {
        return game.snapshot().phase;
    }
    void wakeToIdle() {
        wake.signals = {{true, true, true, true}};
        tick();
        wake.signals = {{false, false, false, false}};
        tick();
    }
    void selectRole(const larp::domain::RoleDefinition &role) {
        cards.present = role.uid;
        tick(1000U);
        tick();
    }
    void completeRound(const larp::domain::RoleDefinition &role) {
        selectRole(role);
        for (const auto &crystal : role.crystals) {
            audio.finish();
            crystals.positions = {crystal.position};
            tick(100U);
            tick();
            crystals.positions.clear();
        }
        audio.finish();
        tick(500U);
        tick();
    }
    void returnToIdle() {
        audio.finish();
        tick(500U);
        tick();
    }
    // Existing controller behavior: first five reads clear current UID, the next
    // five clear previous UID. Keep this visible until the board-clear rule is decided.
    void clearRoleCardForNextRound() {
        cards.present = {};
        for (unsigned int attempt = 0U; attempt < 10U; ++attempt)
            tick(1000U);
    }
    FakeCardReader cards;
    FakeCrystalInput crystals;
    FakeWakeInput wake;
    FakeAudioPlayer audio;
    FakeClock clock;
    FakeLightEffects lights;
    FakeCommunicator communicator;
    FakeDiagnostics diagnostics;
    ReichstagGame game;
};
} // namespace game_test