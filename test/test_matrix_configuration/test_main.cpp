#include <array>
#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <unity.h>

#include "GameRuntime/MatrixConfiguration.h"

using larp::runtime::ExpanderPinConfiguration;
using larp::runtime::isValidExpanderConfiguration;
using larp::runtime::MatrixConfiguration;
using larp::runtime::MatrixConfigurationError;
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

void test_installed_nine_by_nine_mapping_and_wake_pins_are_valid() {
    const MatrixConfiguration configuration = installedMapping();
    assertConfigurationError(MatrixConfigurationError::None, configuration);
    TEST_ASSERT_EQUAL_UINT(
        81, static_cast<unsigned>(configuration.rowCount * configuration.columnCount));
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
    RUN_TEST(test_installed_nine_by_nine_mapping_and_wake_pins_are_valid);
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
