/**
 * @file tsl2591_driver.c
 * @brief TSL2591 Ambient Light Sensor I2C Driver
 *
 * Driver for the TSL2591 high-sensitivity light sensor.
 * Uses ESP-IDF 5.x i2c_master driver for I2C communication.
 */

#include "tsl2591_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "tsl2591";

// =============================================================================
// TSL2591 Register Definitions
// =============================================================================
#define TSL2591_CMD_BIT         0x80    // Command bit (must be set)
#define TSL2591_CMD_NORMAL      0x00    // Normal transaction
#define TSL2591_CMD_SPECIAL     0x60    // Special function

// Register addresses
#define TSL2591_REG_ENABLE      0x00
#define TSL2591_REG_CONFIG      0x01
#define TSL2591_REG_AILTL       0x04    // ALS interrupt low threshold low byte
#define TSL2591_REG_AILTH       0x05    // ALS interrupt low threshold high byte
#define TSL2591_REG_AIHTL       0x06    // ALS interrupt high threshold low byte
#define TSL2591_REG_AIHTH       0x07    // ALS interrupt high threshold high byte
#define TSL2591_REG_NPAILTL     0x08    // No persist ALS interrupt low threshold
#define TSL2591_REG_NPAILTH     0x09
#define TSL2591_REG_NPAIHTL     0x0A    // No persist ALS interrupt high threshold
#define TSL2591_REG_NPAIHTH     0x0B
#define TSL2591_REG_PERSIST     0x0C    // Interrupt persistence filter
#define TSL2591_REG_PID         0x11    // Package ID
#define TSL2591_REG_ID          0x12    // Device ID
#define TSL2591_REG_STATUS      0x13    // Status register
#define TSL2591_REG_C0DATAL     0x14    // CH0 ADC low byte
#define TSL2591_REG_C0DATAH     0x15    // CH0 ADC high byte
#define TSL2591_REG_C1DATAL     0x16    // CH1 ADC low byte
#define TSL2591_REG_C1DATAH     0x17    // CH1 ADC high byte

// Enable register bits
#define TSL2591_ENABLE_PON      0x01    // Power ON
#define TSL2591_ENABLE_AEN      0x02    // ALS Enable
#define TSL2591_ENABLE_AIEN     0x10    // ALS Interrupt Enable
#define TSL2591_ENABLE_NPIEN    0x80    // No Persist Interrupt Enable

// Status register bits
#define TSL2591_STATUS_AVALID   0x01    // ALS Valid
#define TSL2591_STATUS_AINT     0x10    // ALS Interrupt

// Saturation thresholds (based on integration time)
// Full scale for 16-bit ADC is 65535, but saturation occurs earlier
#define TSL2591_SATURATION_100MS    36863
#define TSL2591_SATURATION_200MS    65535
#define TSL2591_SATURATION_300MS    65535
#define TSL2591_SATURATION_400MS    65535
#define TSL2591_SATURATION_500MS    65535
#define TSL2591_SATURATION_600MS    65535

// =============================================================================
// Static Variables
// =============================================================================
static i2c_master_dev_handle_t s_tsl_dev = NULL;
static tsl2591_config_t s_config = {
    .gain = TSL2591_GAIN_MED,
    .integration_time = TSL2591_INTEGTIME_200MS
};
static bool s_initialized = false;

// =============================================================================
// Helper Functions
// =============================================================================

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {
        TSL2591_CMD_BIT | reg,
        value
    };
    return i2c_master_transmit(s_tsl_dev, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t read_register(uint8_t reg, uint8_t *value)
{
    uint8_t cmd = TSL2591_CMD_BIT | reg;
    return i2c_master_transmit_receive(s_tsl_dev, &cmd, 1, value, 1, pdMS_TO_TICKS(100));
}

static esp_err_t read_register_16(uint8_t reg, uint16_t *value)
{
    uint8_t cmd = TSL2591_CMD_BIT | reg;
    uint8_t data[2] = {0};
    esp_err_t ret = i2c_master_transmit_receive(s_tsl_dev, &cmd, 1, data, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    }
    return ret;
}

static uint16_t get_saturation_threshold(tsl2591_integtime_t integtime)
{
    switch (integtime) {
        case TSL2591_INTEGTIME_100MS:
            return TSL2591_SATURATION_100MS;
        default:
            return TSL2591_SATURATION_200MS;
    }
}

// =============================================================================
// Public API Implementation
// =============================================================================

esp_err_t tsl2591_init(i2c_master_bus_handle_t bus)
{
    if (bus == NULL) {
        ESP_LOGE(TAG, "Invalid I2C bus handle");
        return ESP_ERR_INVALID_ARG;
    }

    // Configure device on the I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TSL2591_I2C_ADDR,
        .scl_speed_hz = TSL2591_I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_tsl_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add TSL2591 to I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Verify device ID
    uint8_t id = 0;
    ret = read_register(TSL2591_REG_ID, &id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID: %s", esp_err_to_name(ret));
        i2c_master_bus_rm_device(s_tsl_dev);
        s_tsl_dev = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    if (id != TSL2591_DEVICE_ID) {
        ESP_LOGE(TAG, "Invalid device ID: 0x%02X (expected 0x%02X)", id, TSL2591_DEVICE_ID);
        i2c_master_bus_rm_device(s_tsl_dev);
        s_tsl_dev = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    // Set default configuration
    ret = tsl2591_set_config(&s_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set default config");
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "TSL2591 initialized at address 0x%02X (ID: 0x%02X)", TSL2591_I2C_ADDR, id);
    return ESP_OK;
}

bool tsl2591_is_present(void)
{
    if (s_tsl_dev == NULL) {
        return false;
    }

    uint8_t id = 0;
    esp_err_t ret = read_register(TSL2591_REG_ID, &id);
    return (ret == ESP_OK && id == TSL2591_DEVICE_ID);
}

esp_err_t tsl2591_enable(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = write_register(TSL2591_REG_ENABLE, TSL2591_ENABLE_PON | TSL2591_ENABLE_AEN);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Sensor enabled");
    }
    return ret;
}

esp_err_t tsl2591_disable(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = write_register(TSL2591_REG_ENABLE, 0x00);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Sensor disabled");
    }
    return ret;
}

esp_err_t tsl2591_set_config(const tsl2591_config_t *config)
{
    if (s_tsl_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cfg_value = (config->gain & 0x30) | (config->integration_time & 0x07);
    esp_err_t ret = write_register(TSL2591_REG_CONFIG, cfg_value);

    if (ret == ESP_OK) {
        s_config = *config;
        ESP_LOGD(TAG, "Config set: gain=0x%02X, integ=0x%02X",
                 config->gain, config->integration_time);
    }

    return ret;
}

esp_err_t tsl2591_get_config(tsl2591_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *config = s_config;
    return ESP_OK;
}

uint32_t tsl2591_get_integration_ms(tsl2591_integtime_t integtime)
{
    switch (integtime) {
        case TSL2591_INTEGTIME_100MS: return 100;
        case TSL2591_INTEGTIME_200MS: return 200;
        case TSL2591_INTEGTIME_300MS: return 300;
        case TSL2591_INTEGTIME_400MS: return 400;
        case TSL2591_INTEGTIME_500MS: return 500;
        case TSL2591_INTEGTIME_600MS: return 600;
        default: return 200;
    }
}

bool tsl2591_is_saturated(const tsl2591_reading_t *reading)
{
    if (reading == NULL) {
        return false;
    }

    uint16_t threshold = get_saturation_threshold(s_config.integration_time);
    return (reading->ch0_full >= threshold || reading->ch1_ir >= threshold);
}

esp_err_t tsl2591_read_raw(tsl2591_reading_t *reading)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(reading, 0, sizeof(tsl2591_reading_t));

    // Read CH0 (full spectrum)
    esp_err_t ret = read_register_16(TSL2591_REG_C0DATAL, &reading->ch0_full);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CH0: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read CH1 (IR only)
    ret = read_register_16(TSL2591_REG_C1DATAL, &reading->ch1_ir);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CH1: %s", esp_err_to_name(ret));
        return ret;
    }

    // Calculate visible light
    if (reading->ch0_full > reading->ch1_ir) {
        reading->visible = (float)(reading->ch0_full - reading->ch1_ir);
    } else {
        reading->visible = 0.0f;
    }

    // Check saturation
    reading->saturated = tsl2591_is_saturated(reading);
    reading->valid = true;

    ESP_LOGD(TAG, "Raw read: CH0=%u, CH1=%u, visible=%.0f, saturated=%d",
             reading->ch0_full, reading->ch1_ir, reading->visible, reading->saturated);

    return ESP_OK;
}

esp_err_t tsl2591_read(tsl2591_reading_t *reading)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Enable sensor
    esp_err_t ret = tsl2591_enable();
    if (ret != ESP_OK) {
        return ret;
    }

    // Wait for integration + margin
    uint32_t wait_ms = tsl2591_get_integration_ms(s_config.integration_time) + 20;
    vTaskDelay(pdMS_TO_TICKS(wait_ms));

    // Read data
    ret = tsl2591_read_raw(reading);

    // Disable sensor to save power
    tsl2591_disable();

    return ret;
}

esp_err_t tsl2591_read_auto(tsl2591_reading_t *reading)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Start with current config
    esp_err_t ret = tsl2591_read(reading);
    if (ret != ESP_OK) {
        return ret;
    }

    // If saturated, try reducing gain
    if (reading->saturated && s_config.gain != TSL2591_GAIN_LOW) {
        tsl2591_config_t new_config = s_config;

        // Step down gain
        switch (s_config.gain) {
            case TSL2591_GAIN_MAX:
                new_config.gain = TSL2591_GAIN_HIGH;
                break;
            case TSL2591_GAIN_HIGH:
                new_config.gain = TSL2591_GAIN_MED;
                break;
            case TSL2591_GAIN_MED:
                new_config.gain = TSL2591_GAIN_LOW;
                break;
            default:
                break;
        }

        ESP_LOGD(TAG, "Auto-range: reducing gain from 0x%02X to 0x%02X",
                 s_config.gain, new_config.gain);

        ret = tsl2591_set_config(&new_config);
        if (ret == ESP_OK) {
            ret = tsl2591_read(reading);
        }
    }
    // If too dark (very low readings), try increasing gain
    else if (!reading->saturated && reading->ch0_full < 100 && s_config.gain != TSL2591_GAIN_MAX) {
        tsl2591_config_t new_config = s_config;

        // Step up gain
        switch (s_config.gain) {
            case TSL2591_GAIN_LOW:
                new_config.gain = TSL2591_GAIN_MED;
                break;
            case TSL2591_GAIN_MED:
                new_config.gain = TSL2591_GAIN_HIGH;
                break;
            case TSL2591_GAIN_HIGH:
                new_config.gain = TSL2591_GAIN_MAX;
                break;
            default:
                break;
        }

        ESP_LOGD(TAG, "Auto-range: increasing gain from 0x%02X to 0x%02X",
                 s_config.gain, new_config.gain);

        ret = tsl2591_set_config(&new_config);
        if (ret == ESP_OK) {
            ret = tsl2591_read(reading);
        }
    }

    return ret;
}
