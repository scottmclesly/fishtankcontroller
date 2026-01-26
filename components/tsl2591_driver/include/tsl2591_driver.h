/**
 * @file tsl2591_driver.h
 * @brief TSL2591 Ambient Light Sensor I2C Driver
 *
 * Driver for the TSL2591 high-sensitivity light sensor.
 * Provides visible + IR (CH0) and IR-only (CH1) channels.
 */

#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define TSL2591_I2C_ADDR        0x29
#define TSL2591_I2C_FREQ_HZ     400000
#define TSL2591_DEVICE_ID       0x50    // Expected value from ID register

// =============================================================================
// Gain Settings
// =============================================================================
typedef enum {
    TSL2591_GAIN_LOW   = 0x00,  // 1x gain
    TSL2591_GAIN_MED   = 0x10,  // 25x gain
    TSL2591_GAIN_HIGH  = 0x20,  // 428x gain
    TSL2591_GAIN_MAX   = 0x30,  // 9876x gain
} tsl2591_gain_t;

// =============================================================================
// Integration Time Settings
// =============================================================================
typedef enum {
    TSL2591_INTEGTIME_100MS = 0x00,
    TSL2591_INTEGTIME_200MS = 0x01,
    TSL2591_INTEGTIME_300MS = 0x02,
    TSL2591_INTEGTIME_400MS = 0x03,
    TSL2591_INTEGTIME_500MS = 0x04,
    TSL2591_INTEGTIME_600MS = 0x05,
} tsl2591_integtime_t;

// =============================================================================
// Sensor Configuration
// =============================================================================
typedef struct {
    tsl2591_gain_t gain;
    tsl2591_integtime_t integration_time;
} tsl2591_config_t;

// =============================================================================
// Raw Sensor Reading
// =============================================================================
typedef struct {
    uint16_t ch0_full;      // Full spectrum (visible + IR)
    uint16_t ch1_ir;        // Infrared only
    float visible;          // Calculated: ch0_full - ch1_ir
    bool valid;
    bool saturated;         // True if sensor reading is saturated
} tsl2591_reading_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize the TSL2591 sensor
 * @param bus I2C master bus handle
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if sensor not detected
 */
esp_err_t tsl2591_init(i2c_master_bus_handle_t bus);

/**
 * @brief Check if TSL2591 sensor is detected on the bus
 * @return true if sensor responds with correct ID
 */
bool tsl2591_is_present(void);

/**
 * @brief Power on the sensor
 * @return ESP_OK on success
 */
esp_err_t tsl2591_enable(void);

/**
 * @brief Power off the sensor
 * @return ESP_OK on success
 */
esp_err_t tsl2591_disable(void);

/**
 * @brief Set sensor gain and integration time
 * @param config Configuration settings
 * @return ESP_OK on success
 */
esp_err_t tsl2591_set_config(const tsl2591_config_t *config);

/**
 * @brief Get current sensor configuration
 * @param config Pointer to store configuration
 * @return ESP_OK on success
 */
esp_err_t tsl2591_get_config(tsl2591_config_t *config);

/**
 * @brief Read sensor data (blocking)
 *
 * Enables sensor, waits for integration, reads data, optionally disables.
 * Automatically detects saturation and sets the saturated flag.
 *
 * @param reading Pointer to store reading
 * @return ESP_OK on success
 */
esp_err_t tsl2591_read(tsl2591_reading_t *reading);

/**
 * @brief Read raw channel data (assumes sensor is already enabled)
 *
 * Does not enable/disable sensor. Used for fast sequential reads.
 *
 * @param reading Pointer to store reading
 * @return ESP_OK on success
 */
esp_err_t tsl2591_read_raw(tsl2591_reading_t *reading);

/**
 * @brief Get integration time in milliseconds
 * @param integtime Integration time setting
 * @return Time in milliseconds
 */
uint32_t tsl2591_get_integration_ms(tsl2591_integtime_t integtime);

/**
 * @brief Check if reading is saturated at current gain
 * @param reading Sensor reading to check
 * @return true if saturated
 */
bool tsl2591_is_saturated(const tsl2591_reading_t *reading);

/**
 * @brief Auto-ranging read with gain adjustment
 *
 * Attempts to read with current gain. If saturated, reduces gain and retries.
 * If too dark, increases gain and retries.
 *
 * @param reading Pointer to store reading
 * @return ESP_OK on success
 */
esp_err_t tsl2591_read_auto(tsl2591_reading_t *reading);

#ifdef __cplusplus
}
#endif
