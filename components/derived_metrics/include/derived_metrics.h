/**
 * @file derived_metrics.h
 * @brief Derived Water Quality Metrics Calculator
 *
 * Utility functions for calculating derived water quality parameters
 * from primary sensor readings and user-configured tank settings.
 *
 * Ported from Arduino C++ to pure C for ESP-IDF compatibility.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constants
// =============================================================================
#define DERIVED_METRICS_DEFAULT_TDS_FACTOR  0.64f   // Typical freshwater factor

// =============================================================================
// TDS Calculation
// =============================================================================

/**
 * @brief Calculate Total Dissolved Solids (TDS) from Electrical Conductivity (EC)
 *
 * TDS (ppm) = EC (uS/cm) * conversion factor
 * EC in mS/cm * 1000 = uS/cm
 *
 * @param ec_ms_cm Electrical conductivity in mS/cm
 * @param factor Conversion factor (0.5-0.7, typically 0.64 for freshwater)
 * @return TDS in ppm
 */
float derived_metrics_calc_tds(float ec_ms_cm, float factor);

// =============================================================================
// CO2 Calculation
// =============================================================================

/**
 * @brief Calculate dissolved CO2 concentration from pH and carbonate hardness (KH)
 *
 * CO2 (ppm) = 3.0 * KH * 10^(7.0 - pH)
 * Based on equilibrium equation for carbonate system
 *
 * @param ph Current pH value
 * @param kh_dkh Carbonate hardness in dKH (degrees)
 * @return CO2 concentration in ppm (capped at 100 ppm)
 */
float derived_metrics_calc_co2(float ph, float kh_dkh);

// =============================================================================
// Ammonia Calculations
// =============================================================================

/**
 * @brief Calculate the fraction of total ammonia that exists as toxic NH3
 *
 * Based on temperature-dependent dissociation equilibrium (Emerson et al. 1975)
 *
 * Formula (freshwater):
 * 1. Convert temp to Kelvin: T_k = temp_c + 273.15
 * 2. Calculate pKa: pKa = 0.09018 + (2729.92 / T_k)
 * 3. Calculate fraction: f = 1 / (10^(pKa - pH) + 1)
 *
 * @param temp_c Temperature in degrees Celsius
 * @param ph Current pH value
 * @return Fraction (0.0-1.0) representing toxic ammonia percentage
 */
float derived_metrics_calc_nh3_ratio(float temp_c, float ph);

/**
 * @brief Calculate actual toxic ammonia (NH3) concentration
 *
 * @param total_tan_ppm Total ammonia nitrogen in ppm
 * @param toxic_ratio Fraction of ammonia that is toxic (from calc_nh3_ratio)
 * @return Toxic ammonia (NH3) in ppm
 */
float derived_metrics_calc_nh3(float total_tan_ppm, float toxic_ratio);

// =============================================================================
// Dissolved Oxygen Calculation
// =============================================================================

/**
 * @brief Calculate maximum dissolved oxygen saturation for current conditions
 *
 * Using simplified polynomial approximation:
 * DO (mg/L) = 14.652 - 0.41022*T + 0.007991*T^2 - 0.000077774*T^3
 *
 * For saltwater, applies salinity correction (each ppt reduces DO by ~0.2%)
 *
 * @param temp_c Temperature in degrees Celsius
 * @param salinity_ppt Salinity in parts per thousand (0 for freshwater)
 * @return Maximum DO saturation in mg/L
 */
float derived_metrics_calc_max_do(float temp_c, float salinity_ppt);

// =============================================================================
// Stocking Density Calculation
// =============================================================================

/**
 * @brief Calculate stocking density as a measure of bio-load
 *
 * Stocking density = total fish length (cm) / tank volume (liters)
 *
 * Rule of thumb: 1 cm of fish per 1-2 liters for small tropical fish
 * - Density < 1: Lightly stocked
 * - Density 1-2: Moderately stocked
 * - Density > 2: Heavily stocked (may require extra filtration/water changes)
 *
 * @param total_fish_cm Sum of all fish lengths in cm
 * @param tank_volume_liters Tank volume in liters
 * @return Stocking density in cm of fish per liter
 */
float derived_metrics_calc_stocking_density(float total_fish_cm, float tank_volume_liters);

#ifdef __cplusplus
}
#endif
