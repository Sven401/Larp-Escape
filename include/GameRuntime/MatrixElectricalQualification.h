#pragma once

#include "GameRuntime/MatrixConfiguration.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace runtime {

// This is an asserted hardware-contract fact, not a value inferred from an I2C ACK.
enum class ExpanderInputVariant : std::uint8_t {
    Unknown = 0,
    Mcp23017,
    VerifiedGpa7Gpb7Bidirectional
};

enum class MatrixElectricalBlocker : std::uint32_t {
    StructuralConfigurationInvalid = 1U << 0,
    UnverifiedExpanderVariant = 1U << 1,
    Wake1ExpanderVariantUnknown = 1U << 2,
    MatrixRow1ExpanderVariantUnknown = 1U << 3,
    Wake1Mcp23017Gpa7OrGpb7Input = 1U << 4,
    MatrixRow1Mcp23017Gpa7OrGpb7Input = 1U << 5,
    OtherMatrixRowMcp23017Gpa7OrGpb7Input = 1U << 6,
    OtherWakeMcp23017Gpa7OrGpb7Input = 1U << 7
};

struct MatrixElectricalQualification {
    std::uint32_t blockers = 0U;

    bool qualified() const { return blockers == 0U; }
    bool has(MatrixElectricalBlocker blocker) const {
        return (blockers & static_cast<std::uint32_t>(blocker)) != 0U;
    }
};

inline const char *matrixElectricalBlockerCode(MatrixElectricalBlocker blocker) {
    switch (blocker) {
    case MatrixElectricalBlocker::StructuralConfigurationInvalid:
        return "structural_configuration_invalid";
    case MatrixElectricalBlocker::UnverifiedExpanderVariant:
        return "expander_variant_unverified";
    case MatrixElectricalBlocker::Wake1ExpanderVariantUnknown:
        return "wake_1_expander_variant_unverified";
    case MatrixElectricalBlocker::MatrixRow1ExpanderVariantUnknown:
        return "matrix_row_1_expander_variant_unverified";
    case MatrixElectricalBlocker::Wake1Mcp23017Gpa7OrGpb7Input:
        return "wake_1_mcp23017_gpa7_gpb7_input_unsupported";
    case MatrixElectricalBlocker::MatrixRow1Mcp23017Gpa7OrGpb7Input:
        return "matrix_row_1_mcp23017_gpa7_gpb7_input_unsupported";
    case MatrixElectricalBlocker::OtherMatrixRowMcp23017Gpa7OrGpb7Input:
        return "matrix_other_row_mcp23017_gpa7_gpb7_input_unsupported";
    case MatrixElectricalBlocker::OtherWakeMcp23017Gpa7OrGpb7Input:
        return "wake_other_mcp23017_gpa7_gpb7_input_unsupported";
    }
    return "unknown_matrix_electrical_blocker";
}

inline bool isMcp23017OutputOnlyPin(int pin) {
    return pin == 7 || pin == 15;
}

inline void addMatrixElectricalBlocker(MatrixElectricalQualification &result,
                                       MatrixElectricalBlocker blocker) {
    result.blockers |= static_cast<std::uint32_t>(blocker);
}

inline void qualifyMatrixInputEndpoint(
    MatrixElectricalQualification &result, const MatrixEndpoint &endpoint,
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> &variants,
    MatrixElectricalBlocker unknownBlocker, MatrixElectricalBlocker mcp23017Blocker) {
    const ExpanderInputVariant variant = variants[static_cast<std::size_t>(endpoint.handlerIndex)];
    if (variant == ExpanderInputVariant::Unknown) {
        addMatrixElectricalBlocker(result, MatrixElectricalBlocker::UnverifiedExpanderVariant);
        addMatrixElectricalBlocker(result, unknownBlocker);
    } else if (variant == ExpanderInputVariant::Mcp23017 &&
               isMcp23017OutputOnlyPin(endpoint.pin)) {
        addMatrixElectricalBlocker(result, mcp23017Blocker);
    }
}

inline MatrixElectricalQualification qualifyMatrixElectricalInputs(
    const MatrixConfiguration &configuration,
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> &variants) {
    MatrixElectricalQualification result;
    if (validateMatrixConfiguration(configuration) != MatrixConfigurationError::None) {
        addMatrixElectricalBlocker(result,
                                   MatrixElectricalBlocker::StructuralConfigurationInvalid);
        return result;
    }

    for (std::size_t position = 0; position < configuration.rowCount; ++position) {
        const MatrixLine &row = configuration.rows[position];
        qualifyMatrixInputEndpoint(
            result, row.endpoint, variants,
            row.coordinate == 1 ? MatrixElectricalBlocker::MatrixRow1ExpanderVariantUnknown
                                : MatrixElectricalBlocker::UnverifiedExpanderVariant,
            row.coordinate == 1 ? MatrixElectricalBlocker::MatrixRow1Mcp23017Gpa7OrGpb7Input
                                : MatrixElectricalBlocker::OtherMatrixRowMcp23017Gpa7OrGpb7Input);
    }
    for (std::size_t position = 0; position < configuration.reservedInputCount; ++position) {
        qualifyMatrixInputEndpoint(
            result, configuration.reservedInputs[position], variants,
            position == 0 ? MatrixElectricalBlocker::Wake1ExpanderVariantUnknown
                          : MatrixElectricalBlocker::UnverifiedExpanderVariant,
            position == 0 ? MatrixElectricalBlocker::Wake1Mcp23017Gpa7OrGpb7Input
                          : MatrixElectricalBlocker::OtherWakeMcp23017Gpa7OrGpb7Input);
    }
    return result;
}

inline MatrixElectricalQualification
qualifyMatrixElectricalInputs(const MatrixConfiguration &configuration) {
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> unknown = {};
    return qualifyMatrixElectricalInputs(configuration, unknown);
}

} // namespace runtime
} // namespace larp
