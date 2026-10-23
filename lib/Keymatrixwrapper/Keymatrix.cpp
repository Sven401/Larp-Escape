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
      initialized(false), recoveryNeeded(false) {
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

bool KeyMatrix::releaseColumns() {
    bool released = true;
    for (std::size_t index = 0; index < configuration.columnCount; ++index) {
        const auto &endpoint = configuration.columns[index].endpoint;
        // Try every column even when one release fails. No further LOW is
        // allowed until a later attempt confirms the complete idle condition.
        if (!handlers[endpoint.handlerIndex]->writeGPIO(static_cast<MCP_Pins>(endpoint.pin), HIGH))
            released = false;
    }
    recoveryNeeded = !released;
    return released;
}

bool KeyMatrix::getKeyState(const GridPosition &position, bool &high) {
    high = true;
    if (!initialized)
        return false;
    const MatrixLine *row = findRow(position.row);
    const MatrixLine *column = findColumn(position.column);
    if (row == nullptr || column == nullptr)
        return false;
    if (recoveryNeeded && !releaseColumns())
        return false;
    MCPHandler *columnHandler = handlers[column->endpoint.handlerIndex];
    MCPHandler *rowHandler = handlers[row->endpoint.handlerIndex];
    const auto columnPin = static_cast<MCP_Pins>(column->endpoint.pin);
    const bool selected = columnHandler->writeGPIO(columnPin, LOW);
    uint8_t value = LOW;
    const bool read =
        selected && rowHandler->readGPIO(static_cast<MCP_Pins>(row->endpoint.pin), value);
    // Even a failed selection may have had a partial physical effect. Always
    // attempt release, and make any failed transaction invalidate the scan.
    const bool released = columnHandler->writeGPIO(columnPin, HIGH);
    if (!selected || !read || !released) {
        recoveryNeeded = true;
        return false;
    }
    high = value == HIGH;
    return true;
}

void KeyMatrix::debug(const GridPosition &position) {
    if (!initialized || findRow(position.row) == nullptr ||
        findColumn(position.column) == nullptr) {
        Serial.println("Invalid matrix position or matrix not initialized");
        return;
    }
    // Reuse the normal scan, including its final HIGH write. Debug output must
    // not leave a column selected while the rest of the application continues.
    bool state = true;
    if (!getKeyState(position, state)) {
        Serial.println("Matrix sample unavailable; release recovery required");
        return;
    }
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
            bool currentState = true;
            if (!getKeyState(position, currentState)) {
                Serial.println("Matrix sample unavailable");
                return;
            }
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
            bool currentState = true;
            if (!getKeyState(position, currentState)) {
                Serial.println("Matrix sample unavailable");
                return false;
            }
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

larp::ports::CrystalInputSample KeyMatrix::readPressedPositions() {
    larp::ports::CrystalInputSample sample;
    if (!initialized)
        return sample;
    for (std::size_t row = 0; row < configuration.rowCount; ++row) {
        for (std::size_t column = 0; column < configuration.columnCount; ++column) {
            const GridPosition position{
                static_cast<std::uint8_t>(configuration.rows[row].coordinate),
                static_cast<GridColumn>(configuration.columns[column].coordinate)};
            bool high = true;
            if (!getKeyState(position, high))
                return larp::ports::CrystalInputSample();
            if (!high)
                sample.positions.push_back(position);
        }
    }
    sample.valid = true;
    return sample;
}

larp::ports::WakeInputSample KeyMatrix::readWakeSignals() {
    larp::ports::WakeInputSample sample;
    uint16_t levels = 0;
    if (!initialized || !handlers[0]->readGPIOAB(levels))
        return sample;
    // One complete two-port transaction supplies the whole group. No mixture
    // of successful levels and failed reads can become four active inputs.
    const MCP_Pins pins[] = {MCP_B7, MCP_B6, MCP_B4, MCP_B3};
    for (std::size_t index = 0; index < sample.signals.size(); ++index)
        sample.signals[index] = (levels & (1U << pins[index])) != 0;
    sample.valid = true;
    return sample;
}
