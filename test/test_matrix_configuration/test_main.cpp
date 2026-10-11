#include <array>
#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <unity.h>

#include "GameRuntime/MatrixElectricalQualification.h"
#include "GameRuntime/MatrixConfiguration.h"

using larp::runtime::ExpanderPinConfiguration;
using larp::runtime::ExpanderInputVariant;
using larp::runtime::isValidExpanderConfiguration;
using larp::runtime::matrixElectricalBlockerCode;
using larp::runtime::MatrixConfiguration;
using larp::runtime::MatrixConfigurationError;
using larp::runtime::MatrixElectricalBlocker;
using larp::runtime::qualifyMatrixElectricalInputs;
using larp::runtime::validateMatrixConfiguration;

static_assert(std::is_trivially_copyable<MatrixConfiguration>::value,
              "Matrix mappings use fixed value storage");

void setUp() {}
void tearDown() {}

MatrixConfiguration installedMapping() {
    MatrixConfiguration configuration;
    configuration.availableHandlers = {{true, true}};
    configuration.rowCount = 9;
    configuration.columnCount = 9;
    for (int row = 1; row <= 8; ++row)
        configuration.rows[row - 1] = {row, {1, 16 - row}};
    configuration.rows[8] = {9, {0, 8}};
    const std::array<int, 9> columnPins = {{9, 0, 1, 2, 3, 4, 5, 6, 13}};
    for (std::size_t column = 0; column < columnPins.size(); ++column)
        configuration.columns[column] = {static_cast<int>(column), {0, columnPins[column]}};
    configuration.reservedInputCount = 4;
    configuration.reservedInputs = {{{0, 15}, {0, 14}, {0, 12}, {0, 11}}};
    return configuration;
}

void assertConfigurationError(MatrixConfigurationError expected,
                              const MatrixConfiguration &configuration) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected),
                          static_cast<int>(validateMatrixConfiguration(configuration)));
}

void test_installed_nine_by_nine_mapping_and_wake_pins_are_structurally_valid() {
    const MatrixConfiguration configuration = installedMapping();
    assertConfigurationError(MatrixConfigurationError::None, configuration);
    TEST_ASSERT_EQUAL_UINT(
        81, static_cast<unsigned>(configuration.rowCount * configuration.columnCount));
}

void test_unknown_expander_variants_fail_closed_with_specific_installed_mapping_reasons() {
    const auto result = qualifyMatrixElectricalInputs(installedMapping());
    TEST_ASSERT_FALSE(result.qualified());
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::UnverifiedExpanderVariant));
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::Wake1ExpanderVariantUnknown));
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::MatrixRow1ExpanderVariantUnknown));
}

void test_mcp23017_rejects_wake_1_and_matrix_row_1_gpb7_as_separate_input_conflicts() {
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> variants = {
        {ExpanderInputVariant::Mcp23017, ExpanderInputVariant::Mcp23017}};
    const auto result = qualifyMatrixElectricalInputs(installedMapping(), variants);
    TEST_ASSERT_FALSE(result.qualified());
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::Wake1Mcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::MatrixRow1Mcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_EQUAL_STRING(
        "wake_1_mcp23017_gpa7_gpb7_input_unsupported",
        matrixElectricalBlockerCode(MatrixElectricalBlocker::Wake1Mcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_EQUAL_STRING(
        "matrix_row_1_mcp23017_gpa7_gpb7_input_unsupported",
        matrixElectricalBlockerCode(
            MatrixElectricalBlocker::MatrixRow1Mcp23017Gpa7OrGpb7Input));
}

void test_each_mcp23017_gpb7_conflict_remains_blocking_when_only_the_other_is_reassigned() {
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> variants = {
        {ExpanderInputVariant::Mcp23017, ExpanderInputVariant::Mcp23017}};
    MatrixConfiguration configuration = installedMapping();
    configuration.reservedInputs[0].pin = 10;
    auto result = qualifyMatrixElectricalInputs(configuration, variants);
    TEST_ASSERT_FALSE(result.qualified());
    TEST_ASSERT_FALSE(result.has(MatrixElectricalBlocker::Wake1Mcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::MatrixRow1Mcp23017Gpa7OrGpb7Input));

    configuration = installedMapping();
    configuration.rows[0].endpoint.pin = 0;
    result = qualifyMatrixElectricalInputs(configuration, variants);
    TEST_ASSERT_FALSE(result.qualified());
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::Wake1Mcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_FALSE(result.has(MatrixElectricalBlocker::MatrixRow1Mcp23017Gpa7OrGpb7Input));
}

void test_mcp23017_gpa7_is_also_rejected_when_used_as_an_input() {
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> variants = {
        {ExpanderInputVariant::Mcp23017, ExpanderInputVariant::Mcp23017}};
    MatrixConfiguration configuration = installedMapping();
    configuration.reservedInputs[0].pin = 10;
    configuration.rows[0].endpoint.pin = 7;
    const auto result = qualifyMatrixElectricalInputs(configuration, variants);
    TEST_ASSERT_FALSE(result.qualified());
    TEST_ASSERT_FALSE(result.has(MatrixElectricalBlocker::Wake1Mcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::MatrixRow1Mcp23017Gpa7OrGpb7Input));
}

void test_later_wake_and_matrix_row_have_distinct_mcp23017_blocker_codes() {
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> variants = {
        {ExpanderInputVariant::Mcp23017, ExpanderInputVariant::Mcp23017}};
    MatrixConfiguration wake = installedMapping();
    wake.reservedInputs[0].pin = 10;
    wake.rows[0].endpoint.pin = 0;
    wake.reservedInputs[1].pin = 15;
    auto result = qualifyMatrixElectricalInputs(wake, variants);
    TEST_ASSERT_FALSE(result.qualified());
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::OtherWakeMcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_FALSE(
        result.has(MatrixElectricalBlocker::OtherMatrixRowMcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_EQUAL_STRING(
        "wake_other_mcp23017_gpa7_gpb7_input_unsupported",
        matrixElectricalBlockerCode(MatrixElectricalBlocker::OtherWakeMcp23017Gpa7OrGpb7Input));

    MatrixConfiguration row = installedMapping();
    row.reservedInputs[0].pin = 10;
    row.rows[0].endpoint.pin = 0;
    row.rows[1].endpoint.pin = 7;
    result = qualifyMatrixElectricalInputs(row, variants);
    TEST_ASSERT_FALSE(result.qualified());
    TEST_ASSERT_FALSE(result.has(MatrixElectricalBlocker::OtherWakeMcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_TRUE(
        result.has(MatrixElectricalBlocker::OtherMatrixRowMcp23017Gpa7OrGpb7Input));
    TEST_ASSERT_EQUAL_STRING(
        "matrix_other_row_mcp23017_gpa7_gpb7_input_unsupported",
        matrixElectricalBlockerCode(
            MatrixElectricalBlocker::OtherMatrixRowMcp23017Gpa7OrGpb7Input));
}

void test_reassigned_mcp23017_inputs_can_pass_without_claiming_the_installed_hardware() {
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> variants = {
        {ExpanderInputVariant::Mcp23017, ExpanderInputVariant::Mcp23017}};
    MatrixConfiguration candidate = installedMapping();
    candidate.reservedInputs[0].pin = 10;
    candidate.rows[0].endpoint.pin = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MatrixConfigurationError::None),
                          static_cast<int>(validateMatrixConfiguration(candidate)));
    TEST_ASSERT_TRUE(qualifyMatrixElectricalInputs(candidate, variants).qualified());
}

void test_verified_bidirectional_profile_is_explicit_and_unknown_remains_the_default() {
    const std::array<ExpanderInputVariant, MatrixConfiguration::HandlerCount> verified = {
        {ExpanderInputVariant::VerifiedGpa7Gpb7Bidirectional,
         ExpanderInputVariant::VerifiedGpa7Gpb7Bidirectional}};
    TEST_ASSERT_TRUE(qualifyMatrixElectricalInputs(installedMapping(), verified).qualified());
    TEST_ASSERT_FALSE(qualifyMatrixElectricalInputs(installedMapping()).qualified());
}

void test_structural_failure_is_reported_separately_and_stops_electrical_inspection() {
    MatrixConfiguration configuration = installedMapping();
    configuration.rowCount = configuration.rows.size() + 1;
    const auto result = qualifyMatrixElectricalInputs(configuration);
    TEST_ASSERT_FALSE(result.qualified());
    TEST_ASSERT_TRUE(result.has(MatrixElectricalBlocker::StructuralConfigurationInvalid));
    TEST_ASSERT_FALSE(result.has(MatrixElectricalBlocker::UnverifiedExpanderVariant));
}

void test_null_handler_availability_is_rejected_before_using_any_endpoint() {
    for (std::size_t handler = 0; handler < MatrixConfiguration::HandlerCount; ++handler) {
        MatrixConfiguration configuration = installedMapping();
        configuration.availableHandlers[handler] = false;
        assertConfigurationError(MatrixConfigurationError::MissingHandler, configuration);
    }
}

void test_zero_or_oversized_axis_counts_are_rejected_without_indexing_arrays() {
    MatrixConfiguration configuration = installedMapping();
    configuration.rowCount = 0;
    assertConfigurationError(MatrixConfigurationError::InvalidCount, configuration);
    configuration = installedMapping();
    configuration.columnCount = 0;
    assertConfigurationError(MatrixConfigurationError::InvalidCount, configuration);
    configuration = installedMapping();
    configuration.rowCount = configuration.rows.size() + 1;
    assertConfigurationError(MatrixConfigurationError::InvalidCount, configuration);
    configuration = installedMapping();
    configuration.columnCount = configuration.columns.size() + 1;
    assertConfigurationError(MatrixConfigurationError::InvalidCount, configuration);
    configuration = installedMapping();
    configuration.rowCount = static_cast<std::size_t>(-1);
    assertConfigurationError(MatrixConfigurationError::InvalidCount, configuration);
    configuration = installedMapping();
    configuration.reservedInputCount = configuration.reservedInputs.size() + 1;
    assertConfigurationError(MatrixConfigurationError::InvalidCount, configuration);
}

void test_logical_row_and_column_ranges_are_checked_before_narrowing() {
    for (int row : {0, -1, 10, 256}) {
        MatrixConfiguration configuration = installedMapping();
        configuration.rows[0].coordinate = row;
        assertConfigurationError(MatrixConfigurationError::InvalidCoordinate, configuration);
    }
    for (int column : {-1, 9, 255}) {
        MatrixConfiguration configuration = installedMapping();
        configuration.columns[0].coordinate = column;
        assertConfigurationError(MatrixConfigurationError::InvalidCoordinate, configuration);
    }
}

void test_duplicate_logical_coordinates_cannot_make_a_mapping_ambiguous() {
    MatrixConfiguration configuration = installedMapping();
    configuration.rows[1].coordinate = configuration.rows[0].coordinate;
    assertConfigurationError(MatrixConfigurationError::DuplicateCoordinate, configuration);
    configuration = installedMapping();
    configuration.columns[1].coordinate = configuration.columns[0].coordinate;
    assertConfigurationError(MatrixConfigurationError::DuplicateCoordinate, configuration);
}

void test_negative_or_out_of_range_handler_indexes_and_pins_are_rejected() {
    for (int handler : {-1, 2, 1000}) {
        MatrixConfiguration configuration = installedMapping();
        configuration.rows[0].endpoint.handlerIndex = handler;
        assertConfigurationError(MatrixConfigurationError::InvalidEndpoint, configuration);
    }
    for (int pin : {-1, 16, 255}) {
        MatrixConfiguration configuration = installedMapping();
        configuration.columns[0].endpoint.pin = pin;
        assertConfigurationError(MatrixConfigurationError::InvalidEndpoint, configuration);
    }
    MatrixConfiguration configuration = installedMapping();
    configuration.reservedInputs[0].pin = 16;
    assertConfigurationError(MatrixConfigurationError::InvalidEndpoint, configuration);
}

void test_duplicate_physical_pins_within_or_across_axes_are_rejected() {
    MatrixConfiguration configuration = installedMapping();
    configuration.rows[1].endpoint = configuration.rows[0].endpoint;
    assertConfigurationError(MatrixConfigurationError::DuplicatePin, configuration);
    configuration = installedMapping();
    configuration.columns[1].endpoint = configuration.columns[0].endpoint;
    assertConfigurationError(MatrixConfigurationError::DuplicatePin, configuration);
    configuration = installedMapping();
    configuration.columns[0].endpoint = configuration.rows[0].endpoint;
    assertConfigurationError(MatrixConfigurationError::DuplicatePin, configuration);
}

void test_reserved_wake_inputs_cannot_share_matrix_or_other_wake_pins() {
    MatrixConfiguration configuration = installedMapping();
    configuration.reservedInputs[0] = configuration.rows[0].endpoint;
    assertConfigurationError(MatrixConfigurationError::DuplicatePin, configuration);
    configuration = installedMapping();
    configuration.reservedInputs[1] = configuration.reservedInputs[0];
    assertConfigurationError(MatrixConfigurationError::DuplicatePin, configuration);
}

void test_same_pin_number_on_distinct_expanders_is_not_a_collision() {
    MatrixConfiguration configuration;
    configuration.availableHandlers = {{true, true}};
    configuration.rowCount = configuration.columnCount = 1;
    configuration.rows[0] = {1, {0, 0}};
    configuration.columns[0] = {0, {1, 0}};
    assertConfigurationError(MatrixConfigurationError::None, configuration);
    configuration.columns[0].endpoint.handlerIndex = 0;
    assertConfigurationError(MatrixConfigurationError::DuplicatePin, configuration);
}

std::array<ExpanderPinConfiguration, MatrixConfiguration::PinsPerHandler> completeExpanderPins() {
    std::array<ExpanderPinConfiguration, MatrixConfiguration::PinsPerHandler> pins = {};
    for (std::size_t pin = 0; pin < pins.size(); ++pin)
        pins[pin] = {static_cast<int>(pin), static_cast<int>(pin % 3)};
    return pins;
}

void test_expander_address_and_count_bounds_are_checked() {
    const auto pins = completeExpanderPins();
    for (int address = 0x20; address <= 0x27; ++address)
        TEST_ASSERT_TRUE(isValidExpanderConfiguration(address, pins, pins.size()));
    for (int address : {-1, 0x1F, 0x28, 0x7F})
        TEST_ASSERT_FALSE(isValidExpanderConfiguration(address, pins, pins.size()));
    TEST_ASSERT_FALSE(isValidExpanderConfiguration(0x20, pins, 0));
    TEST_ASSERT_FALSE(isValidExpanderConfiguration(0x20, pins, pins.size() + 1));
    TEST_ASSERT_FALSE(isValidExpanderConfiguration(0x20, pins, static_cast<std::size_t>(-1)));
}

void test_expander_rejects_invalid_pin_modes_and_duplicate_configured_pins() {
    for (int pin : {-1, 16}) {
        auto pins = completeExpanderPins();
        pins[0].pin = pin;
        TEST_ASSERT_FALSE(isValidExpanderConfiguration(0x20, pins, pins.size()));
    }
    for (int mode : {-1, 3}) {
        auto pins = completeExpanderPins();
        pins[0].mode = mode;
        TEST_ASSERT_FALSE(isValidExpanderConfiguration(0x20, pins, pins.size()));
    }
    auto pins = completeExpanderPins();
    pins[1].pin = pins[0].pin;
    TEST_ASSERT_FALSE(isValidExpanderConfiguration(0x20, pins, pins.size()));
}

void test_unconfigured_tail_is_not_interpreted_as_active_pin_configuration() {
    auto pins = completeExpanderPins();
    for (std::size_t pin = 1; pin < pins.size(); ++pin)
        pins[pin] = {-1, -1};
    TEST_ASSERT_TRUE(isValidExpanderConfiguration(0x20, pins, 1));
    TEST_ASSERT_FALSE(isValidExpanderConfiguration(0x20, pins, 2));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_installed_nine_by_nine_mapping_and_wake_pins_are_structurally_valid);
    RUN_TEST(test_unknown_expander_variants_fail_closed_with_specific_installed_mapping_reasons);
    RUN_TEST(test_mcp23017_rejects_wake_1_and_matrix_row_1_gpb7_as_separate_input_conflicts);
    RUN_TEST(test_each_mcp23017_gpb7_conflict_remains_blocking_when_only_the_other_is_reassigned);
    RUN_TEST(test_mcp23017_gpa7_is_also_rejected_when_used_as_an_input);
    RUN_TEST(test_later_wake_and_matrix_row_have_distinct_mcp23017_blocker_codes);
    RUN_TEST(test_reassigned_mcp23017_inputs_can_pass_without_claiming_the_installed_hardware);
    RUN_TEST(test_verified_bidirectional_profile_is_explicit_and_unknown_remains_the_default);
    RUN_TEST(test_structural_failure_is_reported_separately_and_stops_electrical_inspection);
    RUN_TEST(test_null_handler_availability_is_rejected_before_using_any_endpoint);
    RUN_TEST(test_zero_or_oversized_axis_counts_are_rejected_without_indexing_arrays);
    RUN_TEST(test_logical_row_and_column_ranges_are_checked_before_narrowing);
    RUN_TEST(test_duplicate_logical_coordinates_cannot_make_a_mapping_ambiguous);
    RUN_TEST(test_negative_or_out_of_range_handler_indexes_and_pins_are_rejected);
    RUN_TEST(test_duplicate_physical_pins_within_or_across_axes_are_rejected);
    RUN_TEST(test_reserved_wake_inputs_cannot_share_matrix_or_other_wake_pins);
    RUN_TEST(test_same_pin_number_on_distinct_expanders_is_not_a_collision);
    RUN_TEST(test_expander_address_and_count_bounds_are_checked);
    RUN_TEST(test_expander_rejects_invalid_pin_modes_and_duplicate_configured_pins);
    RUN_TEST(test_unconfigured_tail_is_not_interpreted_as_active_pin_configuration);
    return UNITY_END();
}
