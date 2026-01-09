#ifndef DERIVED_METRICS_H
#define DERIVED_METRICS_H

#include <Arduino.h>
#include <math.h>

/**
 * DerivedMetrics - Static utility class for calculating derived water quality metrics
 *
 * This class provides calculation functions for deriving additional water quality
 * parameters from primary sensor readings and user-configured tank settings.
 */
class DerivedMetrics {
public:
    /**
     * Calculate Total Dissolved Solids (TDS) from Electrical Conductivity (EC)
     *
     * @param ec_ms_cm Electrical conductivity in mS/cm
     * @param factor Conversion factor (0.5-0.7, typically 0.64 for freshwater)
     * @return TDS in ppm
     */
    static float calculateTDS(float ec_ms_cm, float factor = 0.64);

    /**
     * Calculate dissolved CO2 concentration from pH and carbonate hardness (KH)
     *
     * @param ph Current pH value
     * @param kh_dkh Carbonate hardness in dKH (degrees)
     * @return CO2 concentration in ppm
     */
    static float calculateCO2(float ph, float kh_dkh);

    /**
     * Calculate the fraction of total ammonia that exists as toxic NH3
     *
     * @param temp_c Temperature in degrees Celsius
     * @param ph Current pH value
     * @return Fraction (0.0-1.0) representing toxic ammonia percentage
     */
    static float calculateToxicAmmoniaRatio(float temp_c, float ph);

    /**
     * Calculate actual toxic ammonia (NH3) concentration
     *
     * @param total_tan_ppm Total ammonia nitrogen in ppm
     * @param toxic_ratio Fraction of ammonia that is toxic (from calculateToxicAmmoniaRatio)
     * @return Toxic ammonia (NH3) in ppm
     */
    static float calculateActualNH3(float total_tan_ppm, float toxic_ratio);

    /**
     * Calculate maximum dissolved oxygen saturation for current conditions
     *
     * @param temp_c Temperature in degrees Celsius
     * @param salinity_ppt Salinity in parts per thousand (default 0 for freshwater)
     * @return Maximum DO saturation in mg/L
     */
    static float calculateMaxDO(float temp_c, float salinity_ppt = 0.0);

    /**
     * Calculate stocking density as a measure of bio-load
     *
     * @param total_fish_cm Sum of all fish lengths in cm
     * @param tank_volume_liters Tank volume in liters
     * @return Stocking density in cm of fish per liter
     */
    static float calculateStockingDensity(float total_fish_cm, float tank_volume_liters);
};

#endif // DERIVED_METRICS_H
