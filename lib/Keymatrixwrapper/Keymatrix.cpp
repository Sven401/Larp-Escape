#include <iostream>
#include <vector>
#include "MCPHandler.h" 
#include <map>
#include "optionConfig.h"
#include "Keymatrix.h"

std::map<std::pair<int, ColLetter>, bool> KeyMatrix::lastState;

KeyMatrix::KeyMatrix(std::vector<MCPHandler> &handlers, const std::map<int, std::pair<int, MCP_Pins>> &rowMap, const std::map<ColLetter, std::pair<int, MCP_Pins>> &colMap)
    : handlers(handlers), rowMapping(rowMap), colMapping(colMap)
{
    for (const auto &row : rowMap)
    {
        rows.push_back(row.first);
    }
    for (const auto &col : colMap)
    {
        cols.push_back(col.first);
    }
    once = true;
}

void KeyMatrix::printMatrixState()
{
    for (int row : rows)
    {
        for (ColLetter col : cols)
        {
            bool currentState = getKeyState(row, col);
            std::pair<int, ColLetter> key = std::make_pair(row, col);

            if (once || lastState[key] != currentState)
            {
                Serial.print("Row: ");
                Serial.print(row);
                Serial.print(" Col: ");
                Serial.print(col);
                Serial.print(" State: ");
                Serial.println(currentState ? "1" : "0");
            }

            lastState[key] = currentState;
        }
    }
    once = false;
}

std::vector<std::pair<int, ColLetter>> KeyMatrix::getLowKeys()
{
    std::vector<std::pair<int, ColLetter>> lowKeys;
    for (int row : rows)
    {
        for (ColLetter col : cols)
        {
            if (!getKeyState(row, col))
            {
                lowKeys.push_back(std::make_pair(row, col));
            }
        }
    }
    return lowKeys;
}

bool KeyMatrix::getKeyState(int row, ColLetter col)
{
    // Set the row pin to LOW
    handlers[colMapping.at(col).first].writeGPIO(colMapping.at(col).second, LOW);
    // Read the column pin state
    bool state = handlers[rowMapping.at(row).first].readGPIO(rowMapping.at(row).second);
    // Reset the row pin to HIGH
    handlers[colMapping.at(col).first].writeGPIO(colMapping.at(col).second, HIGH);

    return state;
}

void KeyMatrix::begin()
{
    for (auto &handler : handlers)
    {
        handler.begin();
    }
    return ;
}   