/**
 * @file warning_manager.h
 * @brief Parameter Warning and Alert Manager
 *
 * Evaluates sensor readings against configurable thresholds
 * and tracks warning states with hysteresis.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define WARNING_NVS_NAMESPACE       "warnings"
#define WARNING_HYSTERESIS_PERCENT  5.0f    // 5% hysteresis to prevent flicker

// =============================================================================
// Warning State Enumeration
// =============================================================================
typedef enum {
    WARNING_STATE_UNKNOWN  = 0,
    WARNING_STATE_NORMAL   = 1,
    WARNING_STATE_WARNING  = 2,
    WARNING_STATE_CRITICAL = 3
} warning_state_t;

// =============================================================================
// Tank Type Enumeration (preset profiles)
// =============================================================================
typedef enum {
    TANK_TYPE_FRESHWATER_COMMUNITY = 0,
    TANK_TYPE_FRESHWATER_PLANTED,
    TANK_TYPE_SALTWATER_FISH_ONLY,
    TANK_TYPE_SALTWATER_REEF,
    TANK_TYPE_CUSTOM
} tank_type_t;

// =============================================================================
// Threshold Structures
// =============================================================================
typedef struct {
    float warn_low;
    float warn_high;
    float crit_low;
    float crit_high;
    float rate_change_per_hour;     // Max acceptable rate of change
} threshold_range_t;

typedef struct {
    float warn_high;
    float crit_high;
} threshold_high_only_t;

// =============================================================================
// Warning Thresholds Structure
// =============================================================================
typedef struct {
    tank_type_t tank_type;

    threshold_range_t temperature;      // Celsius
    threshold_range_t ph;               // pH units
    threshold_high_only_t nh3;          // ppm
    threshold_range_t orp;              // mV
    threshold_range_t ec;               // uS/cm
    threshold_range_t salinity;         // PSU (for saltwater)
    threshold_range_t dissolved_oxygen; // mg/L
} warning_thresholds_t;

// =============================================================================
// Warning Status Structure
// =============================================================================
typedef struct {
    warning_state_t temperature;
    warning_state_t ph;
    warning_state_t nh3;
    warning_state_t orp;
    warning_state_t ec;
    warning_state_t salinity;
    warning_state_t dissolved_oxygen;

    // Rate of change tracking
    float temp_rate_per_hour;
    float ph_rate_per_24h;
} warning_status_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize warning manager
 *
 * Loads thresholds from NVS or sets defaults.
 *
 * @return ESP_OK on success
 */
esp_err_t warning_manager_init(void);

/**
 * @brief Set tank type and load preset thresholds
 * @param type Tank type
 * @return ESP_OK on success
 */
esp_err_t warning_manager_set_tank_type(tank_type_t type);

/**
 * @brief Get current tank type
 * @return Current tank type
 */
tank_type_t warning_manager_get_tank_type(void);

/**
 * @brief Get current thresholds
 * @param thresholds Pointer to thresholds structure
 * @return ESP_OK on success
 */
esp_err_t warning_manager_get_thresholds(warning_thresholds_t *thresholds);

/**
 * @brief Set custom thresholds
 * @param thresholds Pointer to thresholds structure
 * @return ESP_OK on success
 */
esp_err_t warning_manager_set_thresholds(const warning_thresholds_t *thresholds);

// -----------------------------------------------------------------------------
// Evaluation Functions
// -----------------------------------------------------------------------------

/**
 * @brief Evaluate temperature warning state
 * @param temp_c Temperature in Celsius
 * @return Warning state
 */
warning_state_t warning_manager_evaluate_temp(float temp_c);

/**
 * @brief Evaluate pH warning state
 * @param ph pH value
 * @return Warning state
 */
warning_state_t warning_manager_evaluate_ph(float ph);

/**
 * @brief Evaluate ammonia (NH3) warning state
 * @param nh3_ppm Ammonia concentration in ppm
 * @return Warning state
 */
warning_state_t warning_manager_evaluate_nh3(float nh3_ppm);

/**
 * @brief Evaluate ORP warning state
 * @param orp_mv ORP in millivolts
 * @return Warning state
 */
warning_state_t warning_manager_evaluate_orp(float orp_mv);

/**
 * @brief Evaluate EC/conductivity warning state
 * @param ec_us_cm Conductivity in uS/cm
 * @return Warning state
 */
warning_state_t warning_manager_evaluate_ec(float ec_us_cm);

/**
 * @brief Evaluate dissolved oxygen warning state
 * @param do_mg_l Dissolved oxygen in mg/L
 * @return Warning state
 */
warning_state_t warning_manager_evaluate_do(float do_mg_l);

/**
 * @brief Evaluate all parameters and update internal status
 * @param temp_c Temperature
 * @param ph pH value
 * @param nh3_ppm Ammonia
 * @param orp_mv ORP
 * @param ec_us_cm Conductivity
 * @param do_mg_l Dissolved oxygen
 */
void warning_manager_evaluate_all(float temp_c, float ph, float nh3_ppm,
                                  float orp_mv, float ec_us_cm, float do_mg_l);

/**
 * @brief Get current warning status
 * @param status Pointer to status structure
 * @return ESP_OK on success
 */
esp_err_t warning_manager_get_status(warning_status_t *status);

/**
 * @brief Reset all warning states to unknown
 */
void warning_manager_reset_states(void);

/**
 * @brief Get string representation of warning state
 * @param state Warning state
 * @return String ("UNKNOWN", "NORMAL", "WARNING", "CRITICAL")
 */
const char* warning_state_to_string(warning_state_t state);

#ifdef __cplusplus
}
#endif
