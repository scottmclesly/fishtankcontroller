/**
 * @file tank_settings.h
 * @brief Tank Configuration and Fish Profile Manager
 *
 * Stores tank dimensions, fish profiles, and parameters needed
 * for derived metric calculations.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define TANK_SETTINGS_NVS_NAMESPACE "tank"
#define TANK_MAX_FISH_PROFILES      10
#define TANK_MAX_SPECIES_NAME_LEN   32

// =============================================================================
// Tank Shape Enumeration
// =============================================================================
typedef enum {
    TANK_SHAPE_RECTANGLE = 0,
    TANK_SHAPE_CUBE,
    TANK_SHAPE_CYLINDER,
    TANK_SHAPE_CUSTOM
} tank_shape_t;

// =============================================================================
// Tank Dimensions Structure
// =============================================================================
typedef struct {
    float length_cm;        // For rectangle/cube
    float width_cm;         // For rectangle/cube
    float height_cm;        // For all shapes
    float radius_cm;        // For cylinder
} tank_dimensions_t;

// =============================================================================
// Fish Profile Structure
// =============================================================================
typedef struct {
    char species[TANK_MAX_SPECIES_NAME_LEN];
    uint8_t count;
    float avg_length_cm;
} fish_profile_t;

// =============================================================================
// Tank Settings Structure
// =============================================================================
typedef struct {
    // Tank geometry
    tank_shape_t shape;
    tank_dimensions_t dimensions;
    float calculated_volume_liters;
    float manual_volume_liters;     // Override if > 0

    // Water parameters (for calculations)
    float manual_kh_dkh;            // Carbonate hardness
    float manual_tan_ppm;           // Total ammonia nitrogen
    float tds_conversion_factor;    // EC to TDS factor (typically 0.5-0.7)

    // Fish stocking
    uint8_t fish_profile_count;
    fish_profile_t fish_profiles[TANK_MAX_FISH_PROFILES];

    // Metadata
    time_t timestamp;
} tank_settings_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize tank settings manager
 *
 * Loads settings from NVS or sets defaults.
 *
 * @return ESP_OK on success
 */
esp_err_t tank_settings_init(void);

/**
 * @brief Get current tank settings
 * @param settings Pointer to settings structure
 * @return ESP_OK on success
 */
esp_err_t tank_settings_get(tank_settings_t *settings);

/**
 * @brief Save tank settings
 * @param settings Pointer to settings structure
 * @return ESP_OK on success
 */
esp_err_t tank_settings_save(const tank_settings_t *settings);

/**
 * @brief Reset to default settings
 * @return ESP_OK on success
 */
esp_err_t tank_settings_reset(void);

// -----------------------------------------------------------------------------
// Convenience Functions
// -----------------------------------------------------------------------------

/**
 * @brief Calculate tank volume from dimensions
 * @param shape Tank shape
 * @param dims Pointer to dimensions
 * @return Volume in liters
 */
float tank_settings_calculate_volume(tank_shape_t shape, const tank_dimensions_t *dims);

/**
 * @brief Get effective tank volume (manual override or calculated)
 * @return Volume in liters
 */
float tank_settings_get_volume(void);

/**
 * @brief Get total fish stocking length
 * @return Total length of all fish in cm
 */
float tank_settings_get_total_fish_length(void);

/**
 * @brief Get KH value
 * @return KH in dKH
 */
float tank_settings_get_kh(void);

/**
 * @brief Get TAN (Total Ammonia Nitrogen)
 * @return TAN in ppm
 */
float tank_settings_get_tan(void);

/**
 * @brief Get TDS conversion factor
 * @return Conversion factor
 */
float tank_settings_get_tds_factor(void);

// -----------------------------------------------------------------------------
// Fish Profile Functions
// -----------------------------------------------------------------------------

/**
 * @brief Add fish profile
 * @param species Species name
 * @param count Number of fish
 * @param avg_length Average length in cm
 * @return ESP_OK on success, ESP_ERR_NO_MEM if full
 */
esp_err_t tank_settings_add_fish(const char *species, uint8_t count, float avg_length);

/**
 * @brief Remove fish profile by index
 * @param index Profile index
 * @return ESP_OK on success
 */
esp_err_t tank_settings_remove_fish(uint8_t index);

/**
 * @brief Clear all fish profiles
 * @return ESP_OK on success
 */
esp_err_t tank_settings_clear_fish(void);

#ifdef __cplusplus
}
#endif
