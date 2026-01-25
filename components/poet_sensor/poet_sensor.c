/**
 * @file poet_sensor.c
 * @brief POET pH/ORP/EC/Temperature I2C Sensor Driver
 *
 * Driver for the Sentron POET multi-parameter water quality sensor.
 * Uses ESP-IDF 5.x i2c_master driver for I2C communication.
 */

#include "poet_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "poet_sensor";

// Device handle for I2C communication
static i2c_master_dev_handle_t s_poet_dev = NULL;

// Last command sent (needed for reading correct number of bytes)
static poet_cmd_t s_last_cmd = 0;

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * @brief Read 32-bit signed integer in little-endian format from buffer
 */
static int32_t read_int32_le(const uint8_t *buf)
{
    return (int32_t)buf[0] |
           ((int32_t)buf[1] << 8) |
           ((int32_t)buf[2] << 16) |
           ((int32_t)buf[3] << 24);
}

/**
 * @brief Calculate expected number of bytes for a command
 */
static uint8_t get_expected_bytes(poet_cmd_t cmd)
{
    uint8_t bytes = 0;
    if (cmd & POET_CMD_TEMP) bytes += 4;
    if (cmd & POET_CMD_ORP) bytes += 4;
    if (cmd & POET_CMD_PH) bytes += 4;
    if (cmd & POET_CMD_EC) bytes += 8;  // EC returns 2 values
    return bytes;
}

// =============================================================================
// Public API Implementation
// =============================================================================

esp_err_t poet_sensor_init(i2c_master_bus_handle_t bus)
{
    if (bus == NULL) {
        ESP_LOGE(TAG, "Invalid I2C bus handle");
        return ESP_ERR_INVALID_ARG;
    }

    // Configure device on the I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = POET_I2C_ADDR,
        .scl_speed_hz = POET_I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_poet_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add POET device to I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "POET sensor initialized at address 0x%02X", POET_I2C_ADDR);
    return ESP_OK;
}

bool poet_sensor_is_present(void)
{
    if (s_poet_dev == NULL) {
        return false;
    }

    // Try to probe the device by sending an empty transmission
    uint8_t dummy = 0;
    esp_err_t ret = i2c_master_transmit(s_poet_dev, &dummy, 0, pdMS_TO_TICKS(100));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "POET sensor detected");
        return true;
    }

    ESP_LOGW(TAG, "POET sensor not detected: %s", esp_err_to_name(ret));
    return false;
}

esp_err_t poet_sensor_measure_async(poet_cmd_t cmd)
{
    if (s_poet_dev == NULL) {
        ESP_LOGE(TAG, "POET sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (cmd == 0) {
        ESP_LOGE(TAG, "Invalid command: at least one measurement must be selected");
        return ESP_ERR_INVALID_ARG;
    }

    // Send command byte to start measurement
    uint8_t cmd_byte = (uint8_t)cmd;
    esp_err_t ret = i2c_master_transmit(s_poet_dev, &cmd_byte, 1, pdMS_TO_TICKS(100));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command 0x%02X: %s", cmd_byte, esp_err_to_name(ret));
        return ret;
    }

    // Store command for later use when reading results
    s_last_cmd = cmd;

    ESP_LOGD(TAG, "Started measurement with command 0x%02X, wait %lu ms",
             cmd_byte, (unsigned long)poet_sensor_get_delay_ms(cmd));
    return ESP_OK;
}

uint32_t poet_sensor_get_delay_ms(poet_cmd_t cmd)
{
    uint32_t delay = POET_DELAY_BASE_MS;
    if (cmd & POET_CMD_TEMP) delay += POET_DELAY_TEMP_MS;
    if (cmd & POET_CMD_ORP) delay += POET_DELAY_ORP_MS;
    if (cmd & POET_CMD_PH) delay += POET_DELAY_PH_MS;
    if (cmd & POET_CMD_EC) delay += POET_DELAY_EC_MS;
    return delay;
}

esp_err_t poet_sensor_read_result(poet_cmd_t cmd, poet_result_t *result)
{
    if (s_poet_dev == NULL) {
        ESP_LOGE(TAG, "POET sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize result
    memset(result, 0, sizeof(poet_result_t));
    result->valid = false;

    // Calculate expected bytes based on command
    uint8_t expected_bytes = get_expected_bytes(cmd);
    if (expected_bytes == 0) {
        ESP_LOGE(TAG, "Invalid command: no measurements selected");
        return ESP_ERR_INVALID_ARG;
    }

    // Read data from sensor
    uint8_t data[20] = {0};  // Max: 4+4+4+8 = 20 bytes
    esp_err_t ret = i2c_master_receive(s_poet_dev, data, expected_bytes, pdMS_TO_TICKS(100));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read %d bytes: %s", expected_bytes, esp_err_to_name(ret));
        return ret;
    }

    // Parse data in order: Temperature, ORP, pH, EC
    uint8_t offset = 0;

    if (cmd & POET_CMD_TEMP) {
        result->temp_mC = read_int32_le(&data[offset]);
        offset += 4;
    }

    if (cmd & POET_CMD_ORP) {
        result->orp_uV = read_int32_le(&data[offset]);
        offset += 4;
    }

    if (cmd & POET_CMD_PH) {
        result->ugs_uV = read_int32_le(&data[offset]);
        offset += 4;
    }

    if (cmd & POET_CMD_EC) {
        result->ec_nA = read_int32_le(&data[offset]);
        offset += 4;
        result->ec_uV = read_int32_le(&data[offset]);
        offset += 4;
    }

    result->valid = true;

    ESP_LOGD(TAG, "Read %d bytes successfully", expected_bytes);
    return ESP_OK;
}

esp_err_t poet_sensor_measure(poet_cmd_t cmd, poet_result_t *result)
{
    // Start measurement
    esp_err_t ret = poet_sensor_measure_async(cmd);
    if (ret != ESP_OK) {
        return ret;
    }

    // Wait for measurement to complete
    uint32_t delay_ms = poet_sensor_get_delay_ms(cmd);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    // Read result
    return poet_sensor_read_result(cmd, result);
}

float poet_convert_temp_c(int32_t temp_mC)
{
    return (float)temp_mC / 1000.0f;
}

float poet_convert_orp_mv(int32_t orp_uV)
{
    return (float)orp_uV / 1000.0f;
}
