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
    Serial.println("\nKey Matrix State:");
    
    // Print column headers
    Serial.print("   "); // Space for row labels
    for (ColLetter col : cols)
    {
        Serial.print(" ");
        Serial.print(col);
    }
    Serial.println(); // Newline after headers

    for (int row : rows)
    {
        Serial.print(row); // Row label
        Serial.print(" |");

        for (ColLetter col : cols)
        {
            bool currentState = getKeyState(row, col);
            std::pair<int, ColLetter> key = std::make_pair(row, col);

            Serial.print(" ");
            Serial.print(currentState ? "1" : "0");

            lastState[key] = currentState;
        }
        Serial.println(); // Newline after each row
    }
    Serial.println(); // Extra newline for spacing
    once = false;
}

bool KeyMatrix::printMatrixChanges()
{
    bool hasChanges = false;
    
    for (int row : rows)
    {
        for (ColLetter col : cols)
        {
            bool currentState = getKeyState(row, col);
            std::pair<int, ColLetter> key = std::make_pair(row, col);

            if (lastState[key] != currentState)
            {
                Serial.print("Change detected - Row: ");
                Serial.print(row);
                Serial.print(", Col: ");
                Serial.print(col);
                Serial.print(", New State: ");
                Serial.println(currentState ? "1" : "0");
                
                lastState[key] = currentState; // Update stored state
                hasChanges = true;
            }
        }
    }
    return hasChanges;
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
    handlers[colMapping.at(col).first].writeGPIO(colMapping.at(col).second, LOW);
    bool state = handlers[rowMapping.at(row).first].readGPIO(rowMapping.at(row).second);
    handlers[colMapping.at(col).first].writeGPIO(colMapping.at(col).second, HIGH);

    return state;
}

void KeyMatrix::debug(int row, ColLetter col){

    handlers[colMapping.at(col).first].writeGPIO(colMapping.at(col).second, LOW);
    
    bool state = handlers[rowMapping.at(row).first].readGPIO(rowMapping.at(row).second);

    Serial.print("column: ");
    Serial.print(col);
    Serial.print("low ");
    Serial.print(" row: ");
    Serial.print(row);
    Serial.print("input Pullup");
    Serial.println(state);

}



void KeyMatrix::begin()
{
    for (auto &handler : handlers)
    {
        handler.begin();
    }
    return ;
}   