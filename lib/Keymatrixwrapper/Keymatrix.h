#ifndef KEYMATRIX_H
#define KEYMATRIX_H
#pragma once

#include "GamePorts/ICrystalInput.h"
#include "GamePorts/IWakeInput.h"
#include "GameRuntime/MatrixConfiguration.h"

#include <array>
#include <map>
#include <utility>
#include <vector>

#include "GameDomain/GridPosition.h"
#include "MCPHandler.h"

class KeyMatrix : public larp::ports::ICrystalInput, public larp::ports::IWakeInput {
  public:
    using HandlerReferences =
        std::array<MCPHandler *, larp::runtime::MatrixConfiguration::HandlerCount>;
    // Borrows the device handlers and mapping sources. The application keeps
    // them alive; begin validates and snapshots the maps before touching I2C.
    KeyMatrix(HandlerReferences handlers, const std::map<int, std::pair<int, MCP_Pins>> &rowMap,
              const std::map<larp::domain::GridColumn, std::pair<int, MCP_Pins>> &colMap);
    KeyMatrix(const KeyMatrix &) = delete;
    KeyMatrix &operator=(const KeyMatrix &) = delete;
    KeyMatrix(KeyMatrix &&) = delete;
    KeyMatrix &operator=(KeyMatrix &&) = delete;

    void printMatrixState();
    bool printMatrixChanges();
    // A failed scan is explicit; high is only meaningful when true is returned.
    bool getKeyState(const larp::domain::GridPosition &position, bool &high);
    void debug(const larp::domain::GridPosition &position);
    bool begin();
    larp::ports::CrystalInputSample readPressedPositions() override;
    larp::ports::WakeInputSample readWakeSignals() override;

  private:
    bool releaseColumns();
    const larp::runtime::MatrixLine *findRow(int coordinate) const;
    const larp::runtime::MatrixLine *findColumn(larp::domain::GridColumn coordinate) const;
    const HandlerReferences handlers;
    const std::map<int, std::pair<int, MCP_Pins>> &rowMapping;
    const std::map<larp::domain::GridColumn, std::pair<int, MCP_Pins>> &colMapping;
    larp::runtime::MatrixConfiguration configuration;
    std::array<bool, larp::runtime::MatrixConfiguration::MaximumCells> lastState;
    bool initialized;
    bool recoveryNeeded;
};

#endif // KEYMATRIX_H
