/**
 * @file test_components.c
 * @brief Embedded unit tests for ESP-IDF components
 *
 * Tests run on the actual ESP32-C6 device using Unity framework.
 * Tests warning_manager, data_history, and calibration components.
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "warning_manager.h"
#include "data_history.h"
#include "calibration.h"

static const char *TAG = "test_components";

// =============================================================================
// Test Setup/Teardown
// =============================================================================

void setUp(void) {
    // Each test starts fresh
}

void tearDown(void) {
    // Cleanup after each test
}

// =============================================================================
// Warning Manager Tests
// =============================================================================

void test_warning_manager_init(void) {
    esp_err_t ret = warning_manager_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_warning_manager_evaluate_temp_normal(void) {
    // Freshwater community default: 22-28°C warn, 18-32°C crit
    warning_state_t state = warning_manager_evaluate_temp(25.0f);
    TEST_ASSERT_EQUAL(WARNING_STATE_NORMAL, state);
}

void test_warning_manager_evaluate_temp_warning(void) {
    // Low warning (below 22°C)
    warning_state_t state_low = warning_manager_evaluate_temp(20.0f);
    TEST_ASSERT_EQUAL(WARNING_STATE_WARNING, state_low);

    // High warning (above 28°C)
    warning_state_t state_high = warning_manager_evaluate_temp(30.0f);
    TEST_ASSERT_EQUAL(WARNING_STATE_WARNING, state_high);
}

void test_warning_manager_evaluate_temp_critical(void) {
    // Below critical low (18°C)
    warning_state_t state_low = warning_manager_evaluate_temp(15.0f);
    TEST_ASSERT_EQUAL(WARNING_STATE_CRITICAL, state_low);

    // Above critical high (32°C)
    warning_state_t state_high = warning_manager_evaluate_temp(35.0f);
    TEST_ASSERT_EQUAL(WARNING_STATE_CRITICAL, state_high);
}

void test_warning_manager_evaluate_ph_normal(void) {
    // Freshwater community default: 6.5-7.5 warn, 6.0-8.0 crit
    warning_state_t state = warning_manager_evaluate_ph(7.0f);
    TEST_ASSERT_EQUAL(WARNING_STATE_NORMAL, state);
}

void test_warning_manager_evaluate_nh3(void) {
    // NH3 is high-only threshold
    warning_state_t state_safe = warning_manager_evaluate_nh3(0.01f);
    TEST_ASSERT_EQUAL(WARNING_STATE_NORMAL, state_safe);

    // Warning level
    warning_state_t state_warn = warning_manager_evaluate_nh3(0.03f);
    TEST_ASSERT_TRUE(state_warn == WARNING_STATE_WARNING || state_warn == WARNING_STATE_CRITICAL);
}

void test_warning_state_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", warning_state_to_string(WARNING_STATE_UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("NORMAL", warning_state_to_string(WARNING_STATE_NORMAL));
    TEST_ASSERT_EQUAL_STRING("WARNING", warning_state_to_string(WARNING_STATE_WARNING));
    TEST_ASSERT_EQUAL_STRING("CRITICAL", warning_state_to_string(WARNING_STATE_CRITICAL));
}

void test_warning_manager_get_thresholds(void) {
    warning_thresholds_t thresholds;
    esp_err_t ret = warning_manager_get_thresholds(&thresholds);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Check that thresholds are reasonable
    TEST_ASSERT_TRUE(thresholds.temperature.warn_low > 0);
    TEST_ASSERT_TRUE(thresholds.temperature.warn_high > thresholds.temperature.warn_low);
    TEST_ASSERT_TRUE(thresholds.ph.warn_low > 0);
    TEST_ASSERT_TRUE(thresholds.ph.warn_high > thresholds.ph.warn_low);
}

// =============================================================================
// Data History Tests
// =============================================================================

void test_data_history_init(void) {
    esp_err_t ret = data_history_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_data_history_add_and_count(void) {
    // Clear first
    data_history_clear();
    TEST_ASSERT_EQUAL(0, data_history_count());

    // Add a data point
    data_point_t point = {
        .timestamp = 1000,
        .temp_c = 25.0f,
        .ph = 7.2f,
        .orp_mv = 350.0f,
        .ec_ms_cm = 0.5f,
        .tds_ppm = 320.0f,
        .co2_ppm = 15.0f,
        .nh3_ppm = 0.01f,
        .valid = true
    };

    esp_err_t ret = data_history_add(&point);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(1, data_history_count());
}

void test_data_history_get_latest(void) {
    data_history_clear();

    // Add multiple points
    for (int i = 0; i < 5; i++) {
        data_point_t point = {
            .timestamp = 1000 + i * 5,
            .temp_c = 25.0f + i * 0.1f,
            .ph = 7.0f,
            .valid = true
        };
        data_history_add(&point);
    }

    // Get latest
    data_point_t latest;
    esp_err_t ret = data_history_get_latest(&latest);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(1020, latest.timestamp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.4f, latest.temp_c);
}

void test_data_history_circular_buffer(void) {
    data_history_clear();

    // Fill buffer beyond capacity
    for (uint32_t i = 0; i < DATA_HISTORY_SIZE + 10; i++) {
        data_point_t point = {
            .timestamp = i,
            .temp_c = 20.0f + (i % 10) * 0.1f,
            .valid = true
        };
        data_history_add(&point);
    }

    // Count should be capped at buffer size
    TEST_ASSERT_EQUAL(DATA_HISTORY_SIZE, data_history_count());

    // Latest should be the last one added
    data_point_t latest;
    data_history_get_latest(&latest);
    TEST_ASSERT_EQUAL(DATA_HISTORY_SIZE + 9, latest.timestamp);
}

void test_data_history_get_stats(void) {
    data_history_clear();

    // Add some test data
    float temps[] = {20.0f, 22.0f, 24.0f, 26.0f, 28.0f};
    for (int i = 0; i < 5; i++) {
        data_point_t point = {
            .timestamp = 1000 + i * 5,
            .temp_c = temps[i],
            .ph = 7.0f + i * 0.1f,
            .orp_mv = 300.0f + i * 10.0f,
            .ec_ms_cm = 0.5f,
            .valid = true
        };
        data_history_add(&point);
    }

    data_history_stats_t stats;
    esp_err_t ret = data_history_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_EQUAL(5, stats.valid_samples);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, stats.min_temp_c);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 28.0f, stats.max_temp_c);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 24.0f, stats.avg_temp_c);
}

// =============================================================================
// Calibration Tests
// =============================================================================

void test_calibration_init(void) {
    esp_err_t ret = calibration_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_calibration_ph_uncalibrated(void) {
    // Clear calibration first
    calibration_clear_ph();

    // Uncalibrated should return ~7.0 at 0mV
    float ph = calibration_calculate_ph(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 7.0f, ph);
}

void test_calibration_ph_1point(void) {
    // Perform 1-point calibration at pH 7.0
    esp_err_t ret = calibration_ph_1point(7.0f, 0.0f);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Check status
    ph_calibration_t cal;
    calibration_get_ph(&cal);
    TEST_ASSERT_TRUE(cal.calibrated);
    TEST_ASSERT_FALSE(cal.two_point);
}

void test_calibration_ec_uncalibrated(void) {
    // Clear calibration first
    calibration_clear_ec();

    // With K=1.0 (uncalibrated), calculate EC
    // R = V/I = 1000uV / 100nA = 10 kOhm = 10000 Ohm
    // EC = K/R = 1.0/10000 = 0.0001 S/cm = 0.1 mS/cm
    float ec = calibration_calculate_ec(100.0f, 1000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.1f, ec);
}

void test_calibration_get_status(void) {
    calibration_status_t status;
    esp_err_t ret = calibration_get_status(&status);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    // Just check it returns valid structure
}

// =============================================================================
// Main Test Runner
// =============================================================================

void app_main(void) {
    // Initialize NVS (required for components)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    ESP_LOGI(TAG, "Starting embedded component tests");

    UNITY_BEGIN();

    // Warning Manager tests
    RUN_TEST(test_warning_manager_init);
    RUN_TEST(test_warning_manager_evaluate_temp_normal);
    RUN_TEST(test_warning_manager_evaluate_temp_warning);
    RUN_TEST(test_warning_manager_evaluate_temp_critical);
    RUN_TEST(test_warning_manager_evaluate_ph_normal);
    RUN_TEST(test_warning_manager_evaluate_nh3);
    RUN_TEST(test_warning_state_to_string);
    RUN_TEST(test_warning_manager_get_thresholds);

    // Data History tests
    RUN_TEST(test_data_history_init);
    RUN_TEST(test_data_history_add_and_count);
    RUN_TEST(test_data_history_get_latest);
    RUN_TEST(test_data_history_circular_buffer);
    RUN_TEST(test_data_history_get_stats);

    // Calibration tests
    RUN_TEST(test_calibration_init);
    RUN_TEST(test_calibration_ph_uncalibrated);
    RUN_TEST(test_calibration_ph_1point);
    RUN_TEST(test_calibration_ec_uncalibrated);
    RUN_TEST(test_calibration_get_status);

    UNITY_END();
}
