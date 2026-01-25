/**
 * @file poet_sensor.h
 * @brief POET pH/ORP/EC/Temperature I2C Sensor Driver
 *
 * Driver for the Sentron POET multi-parameter water quality sensor.
 * Supports asynchronous measurement to avoid blocking other tasks.
 */

#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define POET_I2C_ADDR           0x1F
#define POET_I2C_FREQ_HZ        400000

// =============================================================================
// Command Bits (LSB to MSB)
// =============================================================================
typedef enum {
    POET_CMD_TEMP = 0x01,   // bit0: Temperature measurement
    POET_CMD_ORP  = 0x02,   // bit1: ORP measurement
    POET_CMD_PH   = 0x04,   // bit2: pH measurement
    POET_CMD_EC   = 0x08,   // bit3: EC measurement
    POET_CMD_ALL  = 0x0F,   // All measurements
} poet_cmd_t;

// =============================================================================
// Measurement Result (Raw Values)
// =============================================================================
typedef struct {
    int32_t temp_mC;        // Temperature in milli-Celsius
    int32_t orp_uV;         // ORP in microvolts
    int32_t ugs_uV;         // pH glass electrode in microvolts
    int32_t ec_nA;          // EC current in nanoamperes
    int32_t ec_uV;          // EC voltage in microvolts
    bool valid;             // True if measurement successful
} poet_result_t;

// =============================================================================
// Measurement Timing (milliseconds)
// =============================================================================
#define POET_DELAY_BASE_MS      100
#define POET_DELAY_TEMP_MS      384
#define POET_DELAY_ORP_MS       1664
#define POET_DELAY_PH_MS        384
#define POET_DELAY_EC_MS        256
#define POET_DELAY_ALL_MS       (POET_DELAY_BASE_MS + POET_DELAY_TEMP_MS + \
                                 POET_DELAY_ORP_MS + POET_DELAY_PH_MS + \
                                 POET_DELAY_EC_MS)  // ~2788ms

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize the POET sensor
 * @param bus I2C master bus handle
 * @return ESP_OK on success
 */
esp_err_t poet_sensor_init(i2c_master_bus_handle_t bus);

/**
 * @brief Check if POET sensor is detected on the bus
 * @return true if sensor responds
 */
bool poet_sensor_is_present(void);

/**
 * @brief Start asynchronous measurement
 *
 * Sends command to sensor and returns immediately.
 * Call poet_sensor_read_result() after appropriate delay.
 *
 * @param cmd Measurement command (bitmask of POET_CMD_*)
 * @return ESP_OK on success
 */
esp_err_t poet_sensor_measure_async(poet_cmd_t cmd);

/**
 * @brief Get required delay for measurement command
 * @param cmd Measurement command
 * @return Delay in milliseconds
 */
uint32_t poet_sensor_get_delay_ms(poet_cmd_t cmd);

/**
 * @brief Read measurement result after delay
 * @param cmd Original command (to know which values to read)
 * @param result Pointer to result structure
 * @return ESP_OK on success
 */
esp_err_t poet_sensor_read_result(poet_cmd_t cmd, poet_result_t *result);

/**
 * @brief Synchronous measurement (blocking)
 *
 * Convenience function that sends command, waits, and reads result.
 * Blocks for ~2.8 seconds for POET_CMD_ALL.
 *
 * @param cmd Measurement command
 * @param result Pointer to result structure
 * @return ESP_OK on success
 */
esp_err_t poet_sensor_measure(poet_cmd_t cmd, poet_result_t *result);

/**
 * @brief Convert raw temperature to Celsius
 * @param temp_mC Raw temperature in milli-Celsius
 * @return Temperature in degrees Celsius
 */
float poet_convert_temp_c(int32_t temp_mC);

/**
 * @brief Convert raw ORP to millivolts
 * @param orp_uV Raw ORP in microvolts
 * @return ORP in millivolts
 */
float poet_convert_orp_mv(int32_t orp_uV);

#ifdef __cplusplus
}
#endif
