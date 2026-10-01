#ifndef KEYMATRIX_H
#define KEYMATRIX_H
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "GameDomain/GridPosition.h"
#include "MCPHandler.h"

class KeyMatrix
{
public:
    KeyMatrix(std::vector<MCPHandler> &handlers,
              const std::map<int, std::pair<int, MCP_Pins>> &rowMap,
              const std::map<larp::domain::GridColumn, std::pair<int, MCP_Pins>> &colMap);

    void printMatrixState();
    bool printMatrixChanges();
    bool getKeyState(const larp::domain::GridPosition &position);
    void debug(const larp::domain::GridPosition &position);
    void begin();
    std::vector<larp::domain::GridPosition> getLowKeys();
    std::vector<MCPHandler> &handlers;

private:
    const std::map<int, std::pair<int, MCP_Pins>> &rowMapping;
    const std::map<larp::domain::GridColumn, std::pair<int, MCP_Pins>> &colMapping;
    std::vector<int> rows;
    std::vector<larp::domain::GridColumn> columns;
    bool once;
    static std::map<larp::domain::GridPosition, bool> lastState;
};

#endif // KEYMATRIX_H
