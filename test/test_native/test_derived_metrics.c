/**
 * @file test_derived_metrics.c
 * @brief Unit tests for derived_metrics calculations
 *
 * Tests run natively on the host computer using Unity framework.
 * These are pure C calculation functions with no hardware dependencies.
 */

#include <stdio.h>
#include <unity.h>
#include <math.h>
#include "derived_metrics.h"

// =============================================================================
// Test Setup/Teardown
// =============================================================================

void setUp(void) {
    // No setup needed for pure calculation tests
}

void tearDown(void) {
    // No teardown needed
}

// =============================================================================
// Helper Functions
// =============================================================================

static void assert_float_within(float expected, float actual, float tolerance, const char *msg) {
    float diff = fabsf(expected - actual);
    if (diff > tolerance) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s: expected %.6f, got %.6f (diff: %.6f)",
                 msg, expected, actual, diff);
        TEST_FAIL_MESSAGE(buf);
    }
}

// =============================================================================
// TDS Calculation Tests
// =============================================================================

void test_tds_basic_calculation(void) {
    // EC = 1.0 mS/cm = 1000 uS/cm
    // TDS = 1000 * 0.64 = 640 ppm
    float tds = derived_metrics_calc_tds(1.0f, 0.64f);
    assert_float_within(640.0f, tds, 0.1f, "TDS at EC=1.0 mS/cm");
}

void test_tds_zero_ec(void) {
    float tds = derived_metrics_calc_tds(0.0f, 0.64f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, tds);
}

void test_tds_various_factors(void) {
    float ec = 0.5f;  // 500 uS/cm

    // Factor 0.5 (low estimate)
    float tds_low = derived_metrics_calc_tds(ec, 0.5f);
    assert_float_within(250.0f, tds_low, 0.1f, "TDS with factor 0.5");

    // Factor 0.7 (high estimate)
    float tds_high = derived_metrics_calc_tds(ec, 0.7f);
    assert_float_within(350.0f, tds_high, 0.1f, "TDS with factor 0.7");
}

void test_tds_negative_ec_returns_zero(void) {
    float tds = derived_metrics_calc_tds(-1.0f, 0.64f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, tds);
}

// =============================================================================
// CO2 Calculation Tests
// =============================================================================

void test_co2_neutral_ph(void) {
    // pH=7.0, KH=4 dKH
    // CO2 = 3.0 * 4 * 10^(7.0-7.0) = 3.0 * 4 * 1 = 12 ppm
    float co2 = derived_metrics_calc_co2(7.0f, 4.0f);
    assert_float_within(12.0f, co2, 0.5f, "CO2 at pH 7.0, KH 4 dKH");
}

void test_co2_low_ph_high_co2(void) {
    // pH=6.0, KH=4 dKH
    // CO2 = 3.0 * 4 * 10^(7.0-6.0) = 12 * 10 = 120 ppm (capped at 100)
    float co2 = derived_metrics_calc_co2(6.0f, 4.0f);
    assert_float_within(100.0f, co2, 0.5f, "CO2 at pH 6.0 should cap at 100 ppm");
}

void test_co2_high_ph_low_co2(void) {
    // pH=8.0, KH=4 dKH
    // CO2 = 3.0 * 4 * 10^(7.0-8.0) = 12 * 0.1 = 1.2 ppm
    float co2 = derived_metrics_calc_co2(8.0f, 4.0f);
    assert_float_within(1.2f, co2, 0.2f, "CO2 at pH 8.0, KH 4 dKH");
}

void test_co2_zero_kh_returns_zero(void) {
    float co2 = derived_metrics_calc_co2(7.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, co2);
}

void test_co2_invalid_inputs(void) {
    // Negative pH
    float co2_neg_ph = derived_metrics_calc_co2(-1.0f, 4.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, co2_neg_ph);

    // Negative KH
    float co2_neg_kh = derived_metrics_calc_co2(7.0f, -1.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, co2_neg_kh);
}

// =============================================================================
// Ammonia Ratio (NH3) Calculation Tests
// =============================================================================

void test_nh3_ratio_reference_conditions(void) {
    // pH=7.52, temp=22.28°C (typical aquarium)
    // Expected: ~1.5% (0.015) toxic ammonia
    float ratio = derived_metrics_calc_nh3_ratio(22.28f, 7.52f);
    assert_float_within(0.015f, ratio, 0.002f, "NH3 ratio at pH 7.52, 22.28°C");
}

void test_nh3_ratio_high_ph_more_toxic(void) {
    // Higher pH = more toxic ammonia
    float ratio_low = derived_metrics_calc_nh3_ratio(25.0f, 7.0f);
    float ratio_high = derived_metrics_calc_nh3_ratio(25.0f, 8.5f);

    TEST_ASSERT_TRUE_MESSAGE(ratio_high > ratio_low * 5,
        "Higher pH should have significantly more toxic ammonia");
}

void test_nh3_ratio_higher_temp_more_toxic(void) {
    // Higher temp = more toxic ammonia
    float ratio_cold = derived_metrics_calc_nh3_ratio(15.0f, 7.5f);
    float ratio_warm = derived_metrics_calc_nh3_ratio(30.0f, 7.5f);

    TEST_ASSERT_TRUE_MESSAGE(ratio_warm > ratio_cold,
        "Higher temp should have more toxic ammonia");
}

void test_nh3_ratio_bounds(void) {
    // Test that ratio is always between 0 and 1
    float temps[] = {5.0f, 15.0f, 20.0f, 25.0f, 30.0f, 35.0f};
    float phs[] = {6.0f, 6.5f, 7.0f, 7.5f, 8.0f, 8.5f, 9.0f};

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 7; j++) {
            float ratio = derived_metrics_calc_nh3_ratio(temps[i], phs[j]);
            TEST_ASSERT_TRUE_MESSAGE(ratio >= 0.0f && ratio <= 1.0f,
                "NH3 ratio must be between 0 and 1");
        }
    }
}

void test_nh3_ratio_invalid_temperature(void) {
    // Too cold
    float ratio_cold = derived_metrics_calc_nh3_ratio(-10.0f, 7.5f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ratio_cold);

    // Too hot
    float ratio_hot = derived_metrics_calc_nh3_ratio(60.0f, 7.5f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ratio_hot);
}

void test_nh3_ratio_invalid_ph(void) {
    // Negative pH
    float ratio_neg = derived_metrics_calc_nh3_ratio(25.0f, -1.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ratio_neg);

    // pH > 14
    float ratio_high = derived_metrics_calc_nh3_ratio(25.0f, 15.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ratio_high);
}

// =============================================================================
// Actual NH3 Concentration Tests
// =============================================================================

void test_nh3_ppm_calculation(void) {
    // TAN = 1.0 ppm, ratio = 1.5%
    // NH3 = 1.0 * 0.015 = 0.015 ppm
    float nh3 = derived_metrics_calc_nh3(1.0f, 0.015f);
    assert_float_within(0.015f, nh3, 0.001f, "NH3 ppm from TAN=1.0, ratio=1.5%");
}

void test_nh3_ppm_zero_tan(void) {
    float nh3 = derived_metrics_calc_nh3(0.0f, 0.015f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, nh3);
}

void test_nh3_ppm_zero_ratio(void) {
    float nh3 = derived_metrics_calc_nh3(1.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, nh3);
}

void test_nh3_ppm_negative_tan(void) {
    float nh3 = derived_metrics_calc_nh3(-1.0f, 0.015f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, nh3);
}

// =============================================================================
// Dissolved Oxygen Saturation Tests
// =============================================================================

void test_do_freshwater_25c(void) {
    // At 25°C freshwater, DO max is approximately 8.3 mg/L
    float do_max = derived_metrics_calc_max_do(25.0f, 0.0f);
    assert_float_within(8.3f, do_max, 0.3f, "DO max at 25°C freshwater");
}

void test_do_cold_water_has_more_oxygen(void) {
    float do_cold = derived_metrics_calc_max_do(10.0f, 0.0f);
    float do_warm = derived_metrics_calc_max_do(30.0f, 0.0f);

    TEST_ASSERT_TRUE_MESSAGE(do_cold > do_warm,
        "Cold water should hold more dissolved oxygen");
}

void test_do_saltwater_less_than_freshwater(void) {
    float do_fresh = derived_metrics_calc_max_do(25.0f, 0.0f);
    float do_salt = derived_metrics_calc_max_do(25.0f, 35.0f);  // Full marine salinity

    TEST_ASSERT_TRUE_MESSAGE(do_fresh > do_salt,
        "Saltwater should hold less dissolved oxygen");
}

void test_do_invalid_temperature(void) {
    // Negative temperature
    float do_neg = derived_metrics_calc_max_do(-10.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, do_neg);

    // Too hot
    float do_hot = derived_metrics_calc_max_do(60.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, do_hot);
}

// =============================================================================
// Stocking Density Tests
// =============================================================================

void test_stocking_density_basic(void) {
    // 50 cm of fish in 50L tank = 1.0 cm/L
    float density = derived_metrics_calc_stocking_density(50.0f, 50.0f);
    assert_float_within(1.0f, density, 0.01f, "Stocking density 50cm/50L");
}

void test_stocking_density_zero_fish(void) {
    float density = derived_metrics_calc_stocking_density(0.0f, 100.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, density);
}

void test_stocking_density_zero_volume(void) {
    // Division by zero protection
    float density = derived_metrics_calc_stocking_density(50.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, density);
}

void test_stocking_density_negative_inputs(void) {
    float density_neg_fish = derived_metrics_calc_stocking_density(-10.0f, 50.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, density_neg_fish);

    float density_neg_vol = derived_metrics_calc_stocking_density(50.0f, -10.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, density_neg_vol);
}

// =============================================================================
// Main
// =============================================================================

int main(void) {
    UNITY_BEGIN();

    // TDS tests
    RUN_TEST(test_tds_basic_calculation);
    RUN_TEST(test_tds_zero_ec);
    RUN_TEST(test_tds_various_factors);
    RUN_TEST(test_tds_negative_ec_returns_zero);

    // CO2 tests
    RUN_TEST(test_co2_neutral_ph);
    RUN_TEST(test_co2_low_ph_high_co2);
    RUN_TEST(test_co2_high_ph_low_co2);
    RUN_TEST(test_co2_zero_kh_returns_zero);
    RUN_TEST(test_co2_invalid_inputs);

    // NH3 ratio tests
    RUN_TEST(test_nh3_ratio_reference_conditions);
    RUN_TEST(test_nh3_ratio_high_ph_more_toxic);
    RUN_TEST(test_nh3_ratio_higher_temp_more_toxic);
    RUN_TEST(test_nh3_ratio_bounds);
    RUN_TEST(test_nh3_ratio_invalid_temperature);
    RUN_TEST(test_nh3_ratio_invalid_ph);

    // NH3 ppm tests
    RUN_TEST(test_nh3_ppm_calculation);
    RUN_TEST(test_nh3_ppm_zero_tan);
    RUN_TEST(test_nh3_ppm_zero_ratio);
    RUN_TEST(test_nh3_ppm_negative_tan);

    // DO tests
    RUN_TEST(test_do_freshwater_25c);
    RUN_TEST(test_do_cold_water_has_more_oxygen);
    RUN_TEST(test_do_saltwater_less_than_freshwater);
    RUN_TEST(test_do_invalid_temperature);

    // Stocking density tests
    RUN_TEST(test_stocking_density_basic);
    RUN_TEST(test_stocking_density_zero_fish);
    RUN_TEST(test_stocking_density_zero_volume);
    RUN_TEST(test_stocking_density_negative_inputs);

    return UNITY_END();
}
