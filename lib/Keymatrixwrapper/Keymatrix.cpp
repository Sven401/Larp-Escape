#include "Keymatrix.h"

using larp::domain::GridColumn;
using larp::domain::GridPosition;
using larp::runtime::MatrixConfiguration;
using larp::runtime::MatrixConfigurationError;
using larp::runtime::MatrixLine;

KeyMatrix::KeyMatrix(HandlerReferences handlers,
                     const std::map<int, std::pair<int, MCP_Pins>> &rowMap,
                     const std::map<GridColumn, std::pair<int, MCP_Pins>> &colMap)
    : handlers(handlers), rowMapping(rowMap), colMapping(colMap), configuration(), lastState(),
      initialized(false) {
    lastState.fill(true);
}

bool KeyMatrix::begin() {
    initialized = false;
    configuration = MatrixConfiguration();
    configuration.rowCount = rowMapping.size();
    configuration.columnCount = colMapping.size();
    if (configuration.rowCount > configuration.rows.size() ||
        configuration.columnCount > configuration.columns.size())
        return false;

    for (std::size_t index = 0; index < handlers.size(); ++index)
        configuration.availableHandlers[index] = handlers[index] != nullptr;
    std::size_t rowIndex = 0;
    for (const auto &row : rowMapping)
        configuration.rows[rowIndex++] = {row.first,
                                          {row.second.first, static_cast<int>(row.second.second)}};
    std::size_t columnIndex = 0;
    for (const auto &column : colMapping)
        configuration.columns[columnIndex++] = {
            static_cast<int>(column.first),
            {column.second.first, static_cast<int>(column.second.second)}};
    configuration.reservedInputCount = 4;
    configuration.reservedInputs = {{{0, MCP_B7}, {0, MCP_B6}, {0, MCP_B4}, {0, MCP_B3}}};
    if (larp::runtime::validateMatrixConfiguration(configuration) != MatrixConfigurationError::None)
        return false;
    if (handlers[0] == handlers[1] || handlers[0]->sharesDeviceWith(*handlers[1]) ||
        handlers[0]->i2cAddress() == handlers[1]->i2cAddress())
        return false;

    // Validate every adapter first so a malformed second config cannot leave
    // the first device partially initialized through this startup path.
    for (MCPHandler *handler : handlers) {
        if (!handler->configurationValid())
            return false;
    }
    for (MCPHandler *handler : handlers) {
        if (!handler->begin())
            return false;
    }
    lastState.fill(true);
    initialized = true;
    return true;
}

const MatrixLine *KeyMatrix::findRow(int coordinate) const {
    for (std::size_t index = 0; index < configuration.rowCount; ++index) {
        if (configuration.rows[index].coordinate == coordinate)
            return &configuration.rows[index];
    }
    return nullptr;
}

const MatrixLine *KeyMatrix::findColumn(GridColumn coordinate) const {
    for (std::size_t index = 0; index < configuration.columnCount; ++index) {
        if (configuration.columns[index].coordinate == static_cast<int>(coordinate))
            return &configuration.columns[index];
    }
    return nullptr;
}

bool KeyMatrix::getKeyState(const GridPosition &position) {
    if (!initialized)
        return HIGH;
    const MatrixLine *row = findRow(position.row);
    const MatrixLine *column = findColumn(position.column);
    if (row == nullptr || column == nullptr)
        return HIGH;
    MCPHandler *columnHandler = handlers[column->endpoint.handlerIndex];
    MCPHandler *rowHandler = handlers[row->endpoint.handlerIndex];
    if (!columnHandler->writeGPIO(static_cast<MCP_Pins>(column->endpoint.pin), LOW))
        return HIGH;
    const bool state = rowHandler->readGPIO(static_cast<MCP_Pins>(row->endpoint.pin));
    if (!columnHandler->writeGPIO(static_cast<MCP_Pins>(column->endpoint.pin), HIGH))
        return HIGH;
    return state;
}

void KeyMatrix::debug(const GridPosition &position) {
    if (!initialized || findRow(position.row) == nullptr ||
        findColumn(position.column) == nullptr) {
        Serial.println("Invalid matrix position or matrix not initialized");
        return;
    }
    // Reuse the normal scan, including its final HIGH write. Debug output must
    // not leave a column selected while the rest of the application continues.
    const bool state = getKeyState(position);
    Serial.print("Scanned column: ");
    Serial.print(static_cast<int>(position.column));
    Serial.print(", row: ");
    Serial.print(static_cast<int>(position.row));
    Serial.print(", input pullup: ");
    Serial.println(state);
}

void KeyMatrix::printMatrixState() {
    if (!initialized)
        return;
    Serial.println("\nKey Matrix State:");
    Serial.print("   ");
    for (std::size_t column = 0; column < configuration.columnCount; ++column) {
        Serial.print(" ");
        Serial.print(configuration.columns[column].coordinate);
    }
    Serial.println();
    for (std::size_t row = 0; row < configuration.rowCount; ++row) {
        Serial.print(configuration.rows[row].coordinate);
        Serial.print(" |");
        for (std::size_t column = 0; column < configuration.columnCount; ++column) {
            const GridPosition position{
                static_cast<std::uint8_t>(configuration.rows[row].coordinate),
                static_cast<GridColumn>(configuration.columns[column].coordinate)};
            const bool currentState = getKeyState(position);
            Serial.print(" ");
            Serial.print(currentState ? "1" : "0");
            lastState[row * MatrixConfiguration::MaximumColumns + column] = currentState;
        }
        Serial.println();
    }
    Serial.println();
}

bool KeyMatrix::printMatrixChanges() {
    if (!initialized)
        return false;
    bool hasChanges = false;
    for (std::size_t row = 0; row < configuration.rowCount; ++row) {
        for (std::size_t column = 0; column < configuration.columnCount; ++column) {
            const GridPosition position{
                static_cast<std::uint8_t>(configuration.rows[row].coordinate),
                static_cast<GridColumn>(configuration.columns[column].coordinate)};
            const bool currentState = getKeyState(position);
            bool &previous = lastState[row * MatrixConfiguration::MaximumColumns + column];
            if (previous != currentState) {
                Serial.print("Change detected - Row: ");
                Serial.print(static_cast<int>(position.row));
                Serial.print(", Col: ");
                Serial.print(static_cast<int>(position.column));
                Serial.print(", New State: ");
                Serial.println(currentState ? "1" : "0");
                previous = currentState;
                hasChanges = true;
            }
        }
    }
    return hasChanges;
}

larp::ports::CrystalInputBatch KeyMatrix::readPressedPositions() {
    larp::ports::CrystalInputBatch pressed;
    if (!initialized)
        return pressed;
    for (std::size_t row = 0; row < configuration.rowCount; ++row) {
        for (std::size_t column = 0; column < configuration.columnCount; ++column) {
            const GridPosition position{
                static_cast<std::uint8_t>(configuration.rows[row].coordinate),
                static_cast<GridColumn>(configuration.columns[column].coordinate)};
            if (!getKeyState(position))
                pressed.push_back(position);
        }
    }
    return pressed;
}

std::array<bool, 4> KeyMatrix::readWakeSignals() {
    // Wake signals are interpreted as active booleans by GameState, unlike
    // the matrix's LOW-is-pressed convention. Startup failure raises no wake.
    if (!initialized || !handlers[0]->isReady())
        return {{false, false, false, false}};
    return {{handlers[0]->readGPIO(MCP_B7) == HIGH, handlers[0]->readGPIO(MCP_B6) == HIGH,
             handlers[0]->readGPIO(MCP_B4) == HIGH, handlers[0]->readGPIO(MCP_B3) == HIGH}};
}
