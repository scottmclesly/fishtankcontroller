/**
 * @file optical_sensor.h
 * @brief Active Optical Sensing Service
 *
 * Measures water turbidity (NTU) and DOC index using TSL2591 light sensor
 * and WS2812B RGB LED backscatter analysis.
 */

#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define OPTICAL_NVS_NAMESPACE       "optical"
#define OPTICAL_MOVING_AVG_SIZE     10      // Moving average filter size
#define OPTICAL_LED_STABILIZE_MS    50      // LED stabilization time before read
#define OPTICAL_AMBIENT_THRESHOLD   16000   // Max ambient light to allow measurement

// =============================================================================
// Error Codes
// =============================================================================
typedef enum {
    OPTICAL_OK = 0,
    OPTICAL_ERR_NOT_INITIALIZED,
    OPTICAL_ERR_TSL2591_NOT_FOUND,
    OPTICAL_ERR_TSL2591_FAILED,
    OPTICAL_ERR_WS2812B_FAILED,
    OPTICAL_ERR_HIGH_AMBIENT,
    OPTICAL_ERR_SATURATED,
    OPTICAL_ERR_NOT_CALIBRATED,
    OPTICAL_ERR_NVS_FAILED,
    OPTICAL_ERR_INVALID_ARG,
} optical_err_t;

// =============================================================================
// Channel Reading (single LED color)
// =============================================================================
typedef struct {
    uint16_t ch0_full;      // Full spectrum (visible + IR)
    uint16_t ch1_ir;        // IR only
    float visible;          // Calculated: ch0_full - ch1_ir
    bool valid;
    bool saturated;
} optical_channel_t;

// =============================================================================
// Complete Measurement Result
// =============================================================================
typedef struct {
    // Raw readings per LED color
    optical_channel_t dark;     // Dark baseline (LED off)
    optical_channel_t green;    // Green LED backscatter
    optical_channel_t blue;     // Blue LED backscatter
    optical_channel_t red;      // Red LED backscatter

    // Dark-corrected backscatter values
    float backscatter_green;    // Green - dark
    float backscatter_blue;     // Blue - dark
    float backscatter_red;      // Red - dark

    // Calculated metrics
    float ntu;                  // Nephelometric Turbidity Units (raw)
    float doc_index;            // DOC index 0-100 (raw)

    // Metadata
    time_t timestamp;
    bool valid;
    bool aborted_high_ambient;  // True if aborted due to high ambient light
} optical_measurement_t;

// =============================================================================
// Calibration Data
// =============================================================================
typedef struct {
    bool calibrated;            // Clear water calibration done

    // Clear water baseline (0 NTU reference)
    float clear_green;          // Green backscatter in clear water
    float clear_blue;           // Blue backscatter in clear water
    float clear_red;            // Red backscatter in clear water
    float clear_ratio;          // Blue/Red ratio for fresh water
    time_t clear_timestamp;

    // Dirty water reference (optional, for better scaling)
    bool has_dirty_reference;
    float dirty_green;          // Green backscatter in dirty water
    float dirty_ratio;          // Blue/Red ratio for old water
    float dirty_ntu_reference;  // User-provided NTU value
    time_t dirty_timestamp;
} optical_calibration_t;

// =============================================================================
// Sensor Status
// =============================================================================
typedef struct {
    bool tsl2591_present;
    bool ws2812b_initialized;
    bool calibrated;
    bool has_dirty_reference;

    // Latest values (filtered)
    float last_ntu;
    float last_doc_index;

    // Latest raw values
    float last_ntu_raw;
    float last_doc_raw;

    // Warning states (matches warning_state_t)
    uint8_t ntu_warning_state;
    uint8_t doc_warning_state;

    time_t last_measurement_time;
    uint32_t measurement_count;
    uint32_t high_ambient_count;  // Count of skipped measurements
} optical_status_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize the optical sensor service
 *
 * Initializes TSL2591 driver and WS2812B driver.
 * Loads calibration from NVS if available.
 *
 * @param i2c_bus I2C master bus handle for TSL2591
 * @param led_gpio GPIO pin for WS2812B (use WS2812B_GPIO for default)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t optical_sensor_init(i2c_master_bus_handle_t i2c_bus, int led_gpio);

/**
 * @brief Perform a water clarity measurement
 *
 * Runs the full measurement sequence:
 * 1. Check ambient light (abort if too high)
 * 2. Dark baseline with LED off
 * 3. Green, blue, red LED pulses with readings
 * 4. Calculate NTU and DOC index
 * 5. Apply moving average filter
 *
 * @param result Pointer to store measurement result
 * @return OPTICAL_OK on success, error code on failure
 */
optical_err_t optical_sensor_measure(optical_measurement_t *result);

/**
 * @brief Calibrate with clear water (0 NTU baseline)
 *
 * Should be performed with fresh, distilled or RO water.
 * Takes a measurement and stores as the "clear water" reference.
 *
 * @return OPTICAL_OK on success
 */
optical_err_t optical_sensor_calibrate_clear(void);

/**
 * @brief Calibrate with dirty water reference
 *
 * Should be performed just before a water change.
 * Takes a measurement and stores as the "dirty water" reference.
 *
 * @param ntu_reference Known or estimated NTU value of dirty water
 * @return OPTICAL_OK on success
 */
optical_err_t optical_sensor_calibrate_dirty(float ntu_reference);

/**
 * @brief Clear all calibration data
 * @return OPTICAL_OK on success
 */
optical_err_t optical_sensor_clear_calibration(void);

/**
 * @brief Get current calibration data
 * @param cal Pointer to store calibration data
 * @return OPTICAL_OK on success
 */
optical_err_t optical_sensor_get_calibration(optical_calibration_t *cal);

/**
 * @brief Get current sensor status
 * @param status Pointer to store status
 * @return OPTICAL_OK on success
 */
optical_err_t optical_sensor_get_status(optical_status_t *status);

/**
 * @brief Get latest filtered NTU value
 * @return Filtered NTU value, or -1 if not available
 */
float optical_sensor_get_filtered_ntu(void);

/**
 * @brief Get latest filtered DOC index
 * @return Filtered DOC index (0-100), or -1 if not available
 */
float optical_sensor_get_filtered_doc(void);

/**
 * @brief Check if sensor is ready for measurement
 * @return true if initialized and TSL2591 is present
 */
bool optical_sensor_is_ready(void);

/**
 * @brief Deinitialize the optical sensor service
 * @return ESP_OK on success
 */
esp_err_t optical_sensor_deinit(void);

#ifdef __cplusplus
}
#endif
