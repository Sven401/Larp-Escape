#ifndef KEYMATRIX_H
#define KEYMATRIX_H
#pragma once

#include <vector>
#include "MCPHandler.h"
#include "optionConfig.h"
#include <map>

class KeyMatrix {
public:
    KeyMatrix(std::vector<MCPHandler>& handlers, 
        const std::map<int, std::pair<int, MCP_Pins>>& rowMap, 
        const std::map<ColLetter, std::pair<int, MCP_Pins>>& colMap);
    
    void printMatrixState();
    bool getKeyState(int row, ColLetter col);
    void begin();
    std::vector<std::pair<int, ColLetter>> getLowKeys(); // New method declaration

private:
    std::vector<MCPHandler>& handlers;
    const std::map<int, std::pair<int, MCP_Pins>>& rowMapping;
    const std::map<ColLetter, std::pair<int, MCP_Pins>>& colMapping;
    std::vector<int> rows;
    std::vector<ColLetter> cols;
    bool once;
    static std::map<std::pair<int, ColLetter>, bool> lastState;
};;

#endif // KEYMATRIX_H