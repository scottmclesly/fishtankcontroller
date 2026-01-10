#include "DerivedMetrics.h"

float DerivedMetrics::calculateTDS(float ec_ms_cm, float factor) {
    // TDS (ppm) = EC (μS/cm) * conversion factor
    // EC in mS/cm * 1000 = μS/cm
    if (ec_ms_cm < 0.0) return 0.0;
    return (ec_ms_cm * 1000.0) * factor;
}

float DerivedMetrics::calculateCO2(float ph, float kh_dkh) {
    // CO2 (ppm) = 3.0 * KH * 10^(7.0 - pH)
    // Based on equilibrium equation for carbonate system
    if (kh_dkh <= 0.0 || ph < 0.0 || ph > 14.0) return 0.0;

    float co2_ppm = 3.0 * kh_dkh * pow(10.0, (7.0 - ph));

    // Sanity check: CO2 should not exceed 100 ppm in normal aquarium conditions
    if (co2_ppm > 100.0) co2_ppm = 100.0;
    if (co2_ppm < 0.0) co2_ppm = 0.0;

    return co2_ppm;
}

float DerivedMetrics::calculateToxicAmmoniaRatio(float temp_c, float ph) {
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
    if (temp_c < 0.0 || temp_c > 50.0) return 0.0;
    if (ph < 0.0 || ph > 14.0) return 0.0;

    float t_kelvin = temp_c + 273.15;
    float pKa = 0.09018 + (2729.92 / t_kelvin);
    float fraction = 1.0 / (pow(10.0, (pKa - ph)) + 1.0);

    // Hard clamp to valid range [0, 1]
    // This should never trigger with correct formula, but prevents display bugs
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;

    return fraction;
}

float DerivedMetrics::calculateActualNH3(float total_tan_ppm, float toxic_ratio) {
    // Actual toxic ammonia = Total Ammonia Nitrogen * Toxic Fraction
    if (total_tan_ppm < 0.0 || toxic_ratio < 0.0) return 0.0;

    return total_tan_ppm * toxic_ratio;
}

float DerivedMetrics::calculateMaxDO(float temp_c, float salinity_ppt) {
    // Maximum dissolved oxygen saturation based on temperature and salinity
    // Using simplified polynomial approximation for freshwater
    //
    // Formula (for freshwater, salinity = 0):
    // DO (mg/L) = 14.652 - 0.41022*T + 0.007991*T² - 0.000077774*T³
    //
    // This is a simplified version; salinity adjustment can be added if needed

    if (temp_c < 0.0 || temp_c > 50.0) return 0.0;

    // Polynomial coefficients for freshwater DO saturation
    float do_mg_l = 14.652
                    - (0.41022 * temp_c)
                    + (0.007991 * temp_c * temp_c)
                    - (0.000077774 * temp_c * temp_c * temp_c);

    // For saltwater, apply salinity correction (optional, simplified)
    if (salinity_ppt > 0.0) {
        // Rough approximation: each ppt of salinity reduces DO by ~0.2%
        float salinity_factor = 1.0 - (salinity_ppt * 0.002);
        do_mg_l *= salinity_factor;
    }

    // Sanity check
    if (do_mg_l < 0.0) do_mg_l = 0.0;
    if (do_mg_l > 20.0) do_mg_l = 20.0;

    return do_mg_l;
}

float DerivedMetrics::calculateStockingDensity(float total_fish_cm, float tank_volume_liters) {
    // Stocking density = total fish length (cm) / tank volume (liters)
    // Rule of thumb: 1 cm of fish per 1-2 liters for small tropical fish
    //
    // Density < 1: Lightly stocked
    // Density 1-2: Moderately stocked
    // Density > 2: Heavily stocked (may require extra filtration/water changes)

    if (tank_volume_liters <= 0.0) return 0.0;
    if (total_fish_cm < 0.0) return 0.0;

    return total_fish_cm / tank_volume_liters;
}
