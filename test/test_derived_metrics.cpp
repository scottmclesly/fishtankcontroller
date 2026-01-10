#include <Arduino.h>
#include <unity.h>
#include "../lib/DerivedMetrics/DerivedMetrics.h"

// Test helper to check if value is within expected range
void assertInRange(float value, float min, float max, const char* message) {
    char msg[100];
    snprintf(msg, sizeof(msg), "%s (got %.6f, expected %.6f-%.6f)", message, value, min, max);
    TEST_ASSERT_TRUE_MESSAGE(value >= min && value <= max, msg);
}

// Test: Basic ammonia calculation at reference conditions
void test_ammonia_calculation_reference() {
    // pH=7.52, temp=22.28°C (from user's actual tank data)
    float fraction = DerivedMetrics::calculateToxicAmmoniaRatio(22.28, 7.52);

    // Expected: ~1.52% (0.0152 as fraction)
    // Allow small tolerance for floating point arithmetic
    assertInRange(fraction, 0.0148, 0.0156, "NH3 fraction at pH 7.52, 22.28°C");
}

// Test: Ammonia calculation at higher pH (more toxic)
void test_ammonia_calculation_high_ph() {
    // pH=8.2, temp=28°C (warmer, higher pH = more toxic NH3)
    float fraction = DerivedMetrics::calculateToxicAmmoniaRatio(28.0, 8.2);

    // At higher pH, fraction should be higher than reference
    // Expected: ~10-15% (0.10-0.15 as fraction)
    assertInRange(fraction, 0.08, 0.20, "NH3 fraction at pH 8.2, 28°C");

    // Should be significantly higher than reference
    float reference = DerivedMetrics::calculateToxicAmmoniaRatio(22.28, 7.52);
    TEST_ASSERT_TRUE_MESSAGE(fraction > reference * 5, "Higher pH should give much higher NH3 fraction");
}

// Test: Ammonia calculation at lower pH (less toxic)
void test_ammonia_calculation_low_ph() {
    // pH=6.5, temp=20°C (lower pH = less toxic NH3)
    float fraction = DerivedMetrics::calculateToxicAmmoniaRatio(20.0, 6.5);

    // At lower pH, fraction should be very small
    // Expected: < 0.5% (0.005 as fraction)
    assertInRange(fraction, 0.0, 0.005, "NH3 fraction at pH 6.5, 20°C");
}

// Test: Fraction is always between 0 and 1
void test_ammonia_fraction_bounds() {
    // Test various combinations
    float test_temps[] = {5.0, 15.0, 20.0, 25.0, 30.0, 35.0};
    float test_phs[] = {6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0};

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 7; j++) {
            float fraction = DerivedMetrics::calculateToxicAmmoniaRatio(test_temps[i], test_phs[j]);

            char msg[100];
            snprintf(msg, sizeof(msg), "Fraction must be [0,1] at temp=%.1f, pH=%.1f", test_temps[i], test_phs[j]);
            TEST_ASSERT_TRUE_MESSAGE(fraction >= 0.0 && fraction <= 1.0, msg);
        }
    }
}

// Test: Input validation - invalid temperature
void test_ammonia_invalid_temperature() {
    // Too cold
    float fraction1 = DerivedMetrics::calculateToxicAmmoniaRatio(-5.0, 7.5);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, fraction1, "Negative temperature should return 0");

    // Too hot
    float fraction2 = DerivedMetrics::calculateToxicAmmoniaRatio(60.0, 7.5);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, fraction2, "Temperature > 50°C should return 0");
}

// Test: Input validation - invalid pH
void test_ammonia_invalid_ph() {
    // Negative pH
    float fraction1 = DerivedMetrics::calculateToxicAmmoniaRatio(25.0, -1.0);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, fraction1, "Negative pH should return 0");

    // pH too high
    float fraction2 = DerivedMetrics::calculateToxicAmmoniaRatio(25.0, 15.0);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, fraction2, "pH > 14 should return 0");
}

// Test: Actual NH3 concentration calculation
void test_nh3_ppm_calculation() {
    // If TAN = 1.0 ppm and toxic ratio is 0.015 (1.5%), then NH3 = 0.015 ppm
    float ratio = 0.015;
    float tan_ppm = 1.0;
    float nh3_ppm = DerivedMetrics::calculateActualNH3(tan_ppm, ratio);

    assertInRange(nh3_ppm, 0.014, 0.016, "NH3 ppm from TAN=1.0, ratio=1.5%");
}

// Test: Zero TAN gives zero NH3
void test_nh3_ppm_zero_tan() {
    float ratio = 0.015;  // Even with 1.5% toxic fraction
    float tan_ppm = 0.0;  // If TAN is zero
    float nh3_ppm = DerivedMetrics::calculateActualNH3(tan_ppm, ratio);

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0, nh3_ppm, "Zero TAN should give zero NH3");
}

void setup() {
    delay(2000);  // Wait for serial
    UNITY_BEGIN();

    RUN_TEST(test_ammonia_calculation_reference);
    RUN_TEST(test_ammonia_calculation_high_ph);
    RUN_TEST(test_ammonia_calculation_low_ph);
    RUN_TEST(test_ammonia_fraction_bounds);
    RUN_TEST(test_ammonia_invalid_temperature);
    RUN_TEST(test_ammonia_invalid_ph);
    RUN_TEST(test_nh3_ppm_calculation);
    RUN_TEST(test_nh3_ppm_zero_tan);

    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}
