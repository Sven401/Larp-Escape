#include "Keymatrix.h"

#include "MCPHandler.h"

using larp::domain::GridColumn;
using larp::domain::GridPosition;

std::map<GridPosition, bool> KeyMatrix::lastState;

KeyMatrix::KeyMatrix(std::vector<MCPHandler> &handlers,
                     const std::map<int, std::pair<int, MCP_Pins>> &rowMap,
                     const std::map<GridColumn, std::pair<int, MCP_Pins>> &colMap)
    : handlers(handlers), rowMapping(rowMap), colMapping(colMap), once(true)
{
    for (const auto &row : rowMap)
    {
        rows.push_back(row.first);
    }
    for (const auto &column : colMap)
    {
        columns.push_back(column.first);
    }
}

void KeyMatrix::printMatrixState()
{
    Serial.println("\nKey Matrix State:");
    Serial.print("   ");
    for (GridColumn column : columns)
    {
        Serial.print(" ");
        Serial.print(static_cast<int>(column));
    }
    Serial.println();

    for (int row : rows)
    {
        Serial.print(row);
        Serial.print(" |");

        for (GridColumn column : columns)
        {
            const GridPosition position{static_cast<std::uint8_t>(row), column};
            const bool currentState = getKeyState(position);
            Serial.print(" ");
            Serial.print(currentState ? "1" : "0");
            lastState[position] = currentState;
        }
        Serial.println();
    }
    Serial.println();
    once = false;
}

bool KeyMatrix::printMatrixChanges()
{
    bool hasChanges = false;

    for (int row : rows)
    {
        for (GridColumn column : columns)
        {
            const GridPosition position{static_cast<std::uint8_t>(row), column};
            const bool currentState = getKeyState(position);

            if (lastState[position] != currentState)
            {
                Serial.print("Change detected - Row: ");
                Serial.print(row);
                Serial.print(", Col: ");
                Serial.print(static_cast<int>(column));
                Serial.print(", New State: ");
                Serial.println(currentState ? "1" : "0");

                lastState[position] = currentState;
                hasChanges = true;
            }
        }
    }
    return hasChanges;
}

std::vector<GridPosition> KeyMatrix::getLowKeys()
{
    std::vector<GridPosition> lowKeys;
    for (int row : rows)
    {
        for (GridColumn column : columns)
        {
            const GridPosition position{static_cast<std::uint8_t>(row), column};
            if (!getKeyState(position))
            {
                lowKeys.push_back(position);
            }
        }
    }
    return lowKeys;
}

bool KeyMatrix::getKeyState(const GridPosition &position)
{
    handlers[colMapping.at(position.column).first].writeGPIO(colMapping.at(position.column).second,
                                                             LOW);
    const bool state = handlers[rowMapping.at(position.row).first].readGPIO(
        rowMapping.at(position.row).second);
    handlers[colMapping.at(position.column).first].writeGPIO(colMapping.at(position.column).second,
                                                             HIGH);
    return state;
}

void KeyMatrix::debug(const GridPosition &position)
{
    handlers[colMapping.at(position.column).first].writeGPIO(colMapping.at(position.column).second,
                                                             LOW);
    const bool state = handlers[rowMapping.at(position.row).first].readGPIO(
        rowMapping.at(position.row).second);

    Serial.print("column: ");
    Serial.print(static_cast<int>(position.column));
    Serial.print("low ");
    Serial.print(" row: ");
    Serial.print(static_cast<int>(position.row));
    Serial.print("input Pullup");
    Serial.println(state);
}

void KeyMatrix::begin()
{
    for (auto &handler : handlers)
    {
        handler.begin();
    }
}
