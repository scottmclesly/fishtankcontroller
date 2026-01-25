/**
 * @file calibration.h
 * @brief pH and EC Sensor Calibration Manager
 *
 * Handles calibration data storage and calculation for pH (1-point/2-point)
 * and EC (cell constant) calibration.
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
#define CALIBRATION_NVS_NAMESPACE   "calibration"
#define CALIBRATION_DEFAULT_PH_SENS 52.0f   // Default Nernstian sensitivity (mV/pH)

// =============================================================================
// pH Calibration Structure
// =============================================================================
typedef struct {
    bool calibrated;            // Has any calibration been done
    bool two_point;             // Is 2-point calibration active

    // Point 1 (always used)
    float point1_ph;            // Known pH value (e.g., 7.0)
    float point1_ugs_uV;        // Raw glass electrode reading (uV)

    // Point 2 (only for 2-point)
    float point2_ph;            // Known pH value (e.g., 4.0)
    float point2_ugs_uV;        // Raw glass electrode reading (uV)

    // Calculated values
    float sensitivity_mV_pH;    // Slope (mV per pH unit)
    float offset_mV;            // Offset at pH 7

    time_t timestamp;           // Last calibration time
} ph_calibration_t;

// =============================================================================
// EC Calibration Structure
// =============================================================================
typedef struct {
    bool calibrated;            // Has calibration been done

    // Calibration solution
    float solution_ec_mS_cm;    // Known conductivity (mS/cm)
    float solution_temp_c;      // Temperature during calibration

    // Raw readings during calibration
    float raw_ec_nA;            // Current reading
    float raw_ec_uV;            // Voltage reading

    // Calculated cell constant
    float cell_constant;        // K = R × EC

    time_t timestamp;           // Last calibration time
} ec_calibration_t;

// =============================================================================
// Calibration Status
// =============================================================================
typedef struct {
    bool ph_calibrated;
    bool ph_two_point;
    time_t ph_timestamp;
    bool ec_calibrated;
    time_t ec_timestamp;
} calibration_status_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize calibration manager
 *
 * Loads calibration data from NVS.
 *
 * @return ESP_OK on success
 */
esp_err_t calibration_init(void);

// -----------------------------------------------------------------------------
// pH Calibration
// -----------------------------------------------------------------------------

/**
 * @brief Perform 1-point pH calibration
 * @param known_ph Known pH value of buffer (typically 7.0)
 * @param raw_ugs_uV Raw glass electrode reading in microvolts
 * @return ESP_OK on success
 */
esp_err_t calibration_ph_1point(float known_ph, float raw_ugs_uV);

/**
 * @brief Add second point for 2-point pH calibration
 *
 * Must call calibration_ph_1point() first.
 *
 * @param known_ph Known pH value of second buffer (e.g., 4.0 or 10.0)
 * @param raw_ugs_uV Raw glass electrode reading in microvolts
 * @return ESP_OK on success
 */
esp_err_t calibration_ph_2point(float known_ph, float raw_ugs_uV);

/**
 * @brief Calculate pH from raw reading
 * @param raw_ugs_uV Raw glass electrode reading in microvolts
 * @return Calculated pH value
 */
float calibration_calculate_ph(float raw_ugs_uV);

/**
 * @brief Get current pH calibration data
 * @param cal Pointer to calibration structure
 * @return ESP_OK on success
 */
esp_err_t calibration_get_ph(ph_calibration_t *cal);

/**
 * @brief Clear pH calibration
 * @return ESP_OK on success
 */
esp_err_t calibration_clear_ph(void);

// -----------------------------------------------------------------------------
// EC Calibration
// -----------------------------------------------------------------------------

/**
 * @brief Perform EC calibration with known solution
 * @param known_ec_mS_cm Known conductivity in mS/cm
 * @param temp_c Temperature during calibration
 * @param raw_ec_nA Raw current reading in nanoamperes
 * @param raw_ec_uV Raw voltage reading in microvolts
 * @return ESP_OK on success
 */
esp_err_t calibration_ec(float known_ec_mS_cm, float temp_c,
                         float raw_ec_nA, float raw_ec_uV);

/**
 * @brief Calculate EC from raw readings
 * @param raw_ec_nA Raw current reading in nanoamperes
 * @param raw_ec_uV Raw voltage reading in microvolts
 * @return Calculated EC in mS/cm
 */
float calibration_calculate_ec(float raw_ec_nA, float raw_ec_uV);

/**
 * @brief Get current EC calibration data
 * @param cal Pointer to calibration structure
 * @return ESP_OK on success
 */
esp_err_t calibration_get_ec(ec_calibration_t *cal);

/**
 * @brief Clear EC calibration
 * @return ESP_OK on success
 */
esp_err_t calibration_clear_ec(void);

// -----------------------------------------------------------------------------
// General
// -----------------------------------------------------------------------------

/**
 * @brief Get calibration status
 * @param status Pointer to status structure
 * @return ESP_OK on success
 */
esp_err_t calibration_get_status(calibration_status_t *status);

/**
 * @brief Clear all calibration data
 * @return ESP_OK on success
 */
esp_err_t calibration_clear_all(void);

#ifdef __cplusplus
}
#endif
