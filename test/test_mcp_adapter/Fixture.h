#pragma once
#include "GameFakes.h"
#include "Keymatrix.h"

inline MCPConfig wakeConfig() {
    MCPConfig config = {};
    config.i2c_addr = 0x20;
    const uint8_t pins[] = {MCP_B0, MCP_B1, MCP_B3, MCP_B4, MCP_B6, MCP_B7};
    for (const uint8_t pin : pins)
        config.pinConfigs[config.numPins++] = {pin, INPUT_PULLUP_MODE};
    return config;
}
inline MCPConfig columnConfig() {
    MCPConfig config = {};
    config.i2c_addr = 0x21;
    config.pinConfigs[config.numPins++] = {MCP_A0, OUTPUT_MODE};
    config.pinConfigs[config.numPins++] = {MCP_A1, OUTPUT_MODE};
    return config;
}

struct McpRig {
    TwoWire &bus;
    Adafruit_MCP23X17 wakeChip, columnChip;
    MCPConfig wakes = wakeConfig(), columns = columnConfig();
    MCPHandler wakeHandler, columnHandler;
    std::map<int, std::pair<int, MCP_Pins>> rows{{1, {0, MCP_B0}}, {2, {0, MCP_B1}}};
    std::map<larp::domain::GridColumn, std::pair<int, MCP_Pins>> cols{
        {larp::domain::GridColumn::A, {1, MCP_A0}}, {larp::domain::GridColumn::B, {1, MCP_A1}}};
    KeyMatrix matrix;
    game_test::FakeCardReader cards;
    game_test::FakeAudioPlayer audio;
    game_test::FakeClock clock;
    game_test::FakeLightEffects lights;
    game_test::FakeCommunicator communicator;
    game_test::FakeDiagnostics diagnostics;
    ReichstagGame game;
    bool ready = false;
    std::vector<TwoWire::WriteOperation> startupWrites;

    explicit McpRig(TwoWire &wire = Wire)
        : bus(wire),
#ifdef MCP_LEGACY_PROBE
          wakeHandler(wakeChip, wakes, "wake"), columnHandler(columnChip, columns, "columns"),
#else
          wakeHandler(wakeChip, wakes, "wake", &wire),
          columnHandler(columnChip, columns, "columns", &wire),
#endif
          matrix({{&wakeHandler, &columnHandler}}, rows, cols),
          game(cards, matrix, matrix, audio, clock, lights, communicator, &diagnostics) {
        bus = TwoWire();
        bus.devices[0x20].present = bus.devices[0x21].present = true;
        // Test precondition: an already idle matrix. begin must preserve latches,
        // not introduce a new hardware startup-level policy.
        bus.devices[0x21].registers[0x14] = 0xff;
        ready = matrix.begin();
        startupWrites = bus.writes;
        bus.writes.clear();
    }
    void tick() {
        game.machine.run();
    }
    bool standby() const {
        return game.snapshot().phase == larp::application::GamePhase::Standby;
    }
    void failOutput(uint8_t pinMask, bool high, bool persistent = false,
                    bool physicalEffect = false) {
        bus.writeFault.address = 0x21;
#ifdef MCP_LEGACY_PROBE
        bus.writeFault.reg = 0x12;
#else
        bus.writeFault.reg = 0x14;
#endif
        bus.writeFault.mask = pinMask;
        bus.writeFault.value = high ? pinMask : 0;
        bus.writeFault.remaining = persistent ? -1 : 1;
        bus.writeFault.applyBeforeNack = physicalEffect;
    }
};
