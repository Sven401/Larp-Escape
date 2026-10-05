#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace runtime {
// The installed board maps logical rows 1..9 and columns A..I to two 16-pin
// expanders. These bounds describe 81 matrix positions, not the LED count.
struct MatrixEndpoint {
    int handlerIndex;
    int pin;
};

struct MatrixLine {
    int coordinate;
    MatrixEndpoint endpoint;
};

struct MatrixConfiguration {
    static const std::size_t HandlerCount = 2;
    static const std::size_t PinsPerHandler = 16;
    static const std::size_t MaximumRows = 9;
    static const std::size_t MaximumColumns = 9;
    static const std::size_t MaximumCells = MaximumRows * MaximumColumns;
    static const std::size_t MaximumReservedInputs = 4;

    std::array<MatrixLine, MaximumRows> rows;
    std::array<MatrixLine, MaximumColumns> columns;
    std::array<MatrixEndpoint, MaximumReservedInputs> reservedInputs;
    std::array<bool, HandlerCount> availableHandlers;
    std::size_t rowCount;
    std::size_t columnCount;
    std::size_t reservedInputCount;

    MatrixConfiguration()
        : rows(), columns(), reservedInputs(), availableHandlers(), rowCount(0), columnCount(0),
          reservedInputCount(0) {}
};

enum class MatrixConfigurationError : std::uint8_t {
    None,
    MissingHandler,
    InvalidCount,
    InvalidCoordinate,
    InvalidEndpoint,
    DuplicateCoordinate,
    DuplicatePin
};

inline bool isValidMatrixPin(int pin) {
    return pin >= 0 && pin < static_cast<int>(MatrixConfiguration::PinsPerHandler);
}

inline bool isValidMatrixEndpoint(const MatrixEndpoint &endpoint) {
    return endpoint.handlerIndex >= 0 &&
           endpoint.handlerIndex < static_cast<int>(MatrixConfiguration::HandlerCount) &&
           isValidMatrixPin(endpoint.pin);
}

inline bool sameMatrixEndpoint(const MatrixEndpoint &first, const MatrixEndpoint &second) {
    return first.handlerIndex == second.handlerIndex && first.pin == second.pin;
}

inline MatrixConfigurationError
validateMatrixConfiguration(const MatrixConfiguration &configuration) {
    for (bool available : configuration.availableHandlers) {
        if (!available)
            return MatrixConfigurationError::MissingHandler;
    }
    if (configuration.rowCount == 0 || configuration.columnCount == 0 ||
        configuration.rowCount > configuration.rows.size() ||
        configuration.columnCount > configuration.columns.size() ||
        configuration.reservedInputCount > configuration.reservedInputs.size())
        return MatrixConfigurationError::InvalidCount;

    std::array<MatrixEndpoint, MatrixConfiguration::MaximumRows +
                                   MatrixConfiguration::MaximumColumns +
                                   MatrixConfiguration::MaximumReservedInputs>
        usedPins = {};
    std::size_t usedPinCount = 0;
    for (std::size_t group = 0; group < 3; ++group) {
        const std::size_t count = group == 0   ? configuration.rowCount
                                  : group == 1 ? configuration.columnCount
                                               : configuration.reservedInputCount;
        for (std::size_t position = 0; position < count; ++position) {
            MatrixEndpoint endpoint = {};
            if (group < 2) {
                const MatrixLine &line =
                    group == 0 ? configuration.rows[position] : configuration.columns[position];
                const bool validCoordinate = group == 0
                                                 ? line.coordinate >= 1 && line.coordinate <= 9
                                                 : line.coordinate >= 0 && line.coordinate < 9;
                if (!validCoordinate)
                    return MatrixConfigurationError::InvalidCoordinate;
                for (std::size_t previous = 0; previous < position; ++previous) {
                    const MatrixLine &other =
                        group == 0 ? configuration.rows[previous] : configuration.columns[previous];
                    if (line.coordinate == other.coordinate)
                        return MatrixConfigurationError::DuplicateCoordinate;
                }
                endpoint = line.endpoint;
            } else {
                endpoint = configuration.reservedInputs[position];
            }
            if (!isValidMatrixEndpoint(endpoint))
                return MatrixConfigurationError::InvalidEndpoint;
            for (std::size_t previous = 0; previous < usedPinCount; ++previous) {
                if (sameMatrixEndpoint(usedPins[previous], endpoint))
                    return MatrixConfigurationError::DuplicatePin;
            }
            usedPins[usedPinCount++] = endpoint;
        }
    }
    return MatrixConfigurationError::None;
}

struct ExpanderPinConfiguration {
    int pin;
    // Normalized adapter modes: input=0, input pullup=1, output=2.
    int mode;
};

inline bool isValidExpanderConfiguration(
    int address,
    const std::array<ExpanderPinConfiguration, MatrixConfiguration::PinsPerHandler> &pins,
    std::size_t count) {
    if (address < 0x20 || address > 0x27 || count == 0 || count > pins.size())
        return false;
    for (std::size_t position = 0; position < count; ++position) {
        if (!isValidMatrixPin(pins[position].pin) || pins[position].mode < 0 ||
            pins[position].mode > 2)
            return false;
        for (std::size_t previous = 0; previous < position; ++previous) {
            if (pins[position].pin == pins[previous].pin)
                return false;
        }
    }
    return true;
}
} // namespace runtime
} // namespace larp
