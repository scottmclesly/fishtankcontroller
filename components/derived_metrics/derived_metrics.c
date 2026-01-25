/**
 * @file derived_metrics.c
 * @brief Derived Water Quality Metrics Calculator Implementation
 *
 * Ported from Arduino C++ to pure C for ESP-IDF compatibility.
 */

#include "derived_metrics.h"
#include <math.h>

// =============================================================================
// TDS Calculation
// =============================================================================

float derived_metrics_calc_tds(float ec_ms_cm, float factor)
{
    // TDS (ppm) = EC (uS/cm) * conversion factor
    // EC in mS/cm * 1000 = uS/cm
    if (ec_ms_cm < 0.0f) {
        return 0.0f;
    }
    return (ec_ms_cm * 1000.0f) * factor;
}

// =============================================================================
// CO2 Calculation
// =============================================================================

float derived_metrics_calc_co2(float ph, float kh_dkh)
{
    // CO2 (ppm) = 3.0 * KH * 10^(7.0 - pH)
    // Based on equilibrium equation for carbonate system
    if (kh_dkh <= 0.0f || ph < 0.0f || ph > 14.0f) {
        return 0.0f;
    }

    float co2_ppm = 3.0f * kh_dkh * powf(10.0f, (7.0f - ph));

    // Sanity check: CO2 should not exceed 100 ppm in normal aquarium conditions
    if (co2_ppm > 100.0f) {
        co2_ppm = 100.0f;
    }
    if (co2_ppm < 0.0f) {
        co2_ppm = 0.0f;
    }

    return co2_ppm;
}

// =============================================================================
// Ammonia Calculations
// =============================================================================

float derived_metrics_calc_nh3_ratio(float temp_c, float ph)
{
    // Calculate the fraction of total ammonia (TAN) that exists as toxic NH3
    // Based on temperature-dependent dissociation equilibrium (Emerson et al. 1975)
    //
    // Formula (freshwater):
    // 1. Convert temp to Kelvin: T_k = temp_c + 273.15
    // 2. Calculate pKa: pKa = 0.09018 + (2729.92 / T_k)
    // 3. Calculate fraction: f = 1 / (10^(pKa - pH) + 1)
    //
    // Returns fraction (0.0-1.0), NOT percentage
    // Multiply by 100 in UI layer for percentage display

    // Strict input validation
    if (temp_c < 0.0f || temp_c > 50.0f) {
        return 0.0f;
    }
    if (ph < 0.0f || ph > 14.0f) {
        return 0.0f;
    }

    float t_kelvin = temp_c + 273.15f;
    float pKa = 0.09018f + (2729.92f / t_kelvin);
    float fraction = 1.0f / (powf(10.0f, (pKa - ph)) + 1.0f);

    // Hard clamp to valid range [0, 1]
    // This should never trigger with correct formula, but prevents display bugs
    if (fraction < 0.0f) {
        fraction = 0.0f;
    }
    if (fraction > 1.0f) {
        fraction = 1.0f;
    }

    return fraction;
}

float derived_metrics_calc_nh3(float total_tan_ppm, float toxic_ratio)
{
    // Actual toxic ammonia = Total Ammonia Nitrogen * Toxic Fraction
    if (total_tan_ppm < 0.0f || toxic_ratio < 0.0f) {
        return 0.0f;
    }

    return total_tan_ppm * toxic_ratio;
}

// =============================================================================
// Dissolved Oxygen Calculation
// =============================================================================

float derived_metrics_calc_max_do(float temp_c, float salinity_ppt)
{
    // Maximum dissolved oxygen saturation based on temperature and salinity
    // Using simplified polynomial approximation for freshwater
    //
    // Formula (for freshwater, salinity = 0):
    // DO (mg/L) = 14.652 - 0.41022*T + 0.007991*T^2 - 0.000077774*T^3

    if (temp_c < 0.0f || temp_c > 50.0f) {
        return 0.0f;
    }

    // Polynomial coefficients for freshwater DO saturation
    float do_mg_l = 14.652f
                    - (0.41022f * temp_c)
                    + (0.007991f * temp_c * temp_c)
                    - (0.000077774f * temp_c * temp_c * temp_c);

    // For saltwater, apply salinity correction
    if (salinity_ppt > 0.0f) {
        // Rough approximation: each ppt of salinity reduces DO by ~0.2%
        float salinity_factor = 1.0f - (salinity_ppt * 0.002f);
        do_mg_l *= salinity_factor;
    }

    // Sanity check
    if (do_mg_l < 0.0f) {
        do_mg_l = 0.0f;
    }
    if (do_mg_l > 20.0f) {
        do_mg_l = 20.0f;
    }

    return do_mg_l;
}

// =============================================================================
// Stocking Density Calculation
// =============================================================================

float derived_metrics_calc_stocking_density(float total_fish_cm, float tank_volume_liters)
{
    // Stocking density = total fish length (cm) / tank volume (liters)
    // Rule of thumb: 1 cm of fish per 1-2 liters for small tropical fish
    //
    // Density < 1: Lightly stocked
    // Density 1-2: Moderately stocked
    // Density > 2: Heavily stocked (may require extra filtration/water changes)

    if (tank_volume_liters <= 0.0f) {
        return 0.0f;
    }
    if (total_fish_cm < 0.0f) {
        return 0.0f;
    }

    return total_fish_cm / tank_volume_liters;
}
