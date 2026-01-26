/**
 * @file optical_sensor.c
 * @brief Active Optical Sensing Service Implementation
 *
 * Measures water turbidity (NTU) and DOC index using TSL2591 light sensor
 * and WS2812B RGB LED backscatter analysis.
 */

#include "optical_sensor.h"
#include "tsl2591_driver.h"
#include "ws2812b_driver.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "optical_sensor";

// =============================================================================
// NVS Keys
// =============================================================================
#define NVS_KEY_CAL_DONE        "cal_done"
#define NVS_KEY_CLR_GREEN       "clr_green"
#define NVS_KEY_CLR_BLUE        "clr_blue"
#define NVS_KEY_CLR_RED         "clr_red"
#define NVS_KEY_CLR_RATIO       "clr_ratio"
#define NVS_KEY_CLR_TS          "clr_ts"
#define NVS_KEY_DRT_DONE        "drt_done"
#define NVS_KEY_DRT_GREEN       "drt_green"
#define NVS_KEY_DRT_RATIO       "drt_ratio"
#define NVS_KEY_DRT_NTU         "drt_ntu"
#define NVS_KEY_DRT_TS          "drt_ts"

// =============================================================================
// Constants
// =============================================================================
#define UNCALIBRATED_NTU_FACTOR     0.01f   // Rough scaling for uncalibrated NTU
#define NTU_MIN                     0.0f
#define NTU_MAX                     1000.0f
#define DOC_MIN                     0.0f
#define DOC_MAX                     100.0f

// Clamp macro
#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

// =============================================================================
// Static Variables
// =============================================================================
static bool s_initialized = false;
static nvs_handle_t s_nvs_handle = 0;

// Calibration data
static optical_calibration_t s_calibration = {0};

// Moving average filter
static float s_ntu_buffer[OPTICAL_MOVING_AVG_SIZE] = {0};
static float s_doc_buffer[OPTICAL_MOVING_AVG_SIZE] = {0};
static uint8_t s_filter_index = 0;
static uint8_t s_filter_count = 0;
static float s_filtered_ntu = -1.0f;
static float s_filtered_doc = -1.0f;

// Status tracking
static time_t s_last_measurement_time = 0;
static uint32_t s_measurement_count = 0;
static uint32_t s_high_ambient_count = 0;
static float s_last_ntu_raw = 0.0f;
static float s_last_doc_raw = 0.0f;

// =============================================================================
// NVS Helper Functions
// =============================================================================

static esp_err_t save_float(const char *key, float value)
{
    return nvs_set_blob(s_nvs_handle, key, &value, sizeof(float));
}

static esp_err_t load_float(const char *key, float *value)
{
    size_t len = sizeof(float);
    return nvs_get_blob(s_nvs_handle, key, value, &len);
}

static esp_err_t load_calibration(void)
{
    memset(&s_calibration, 0, sizeof(s_calibration));

    uint8_t cal_done = 0;
    if (nvs_get_u8(s_nvs_handle, NVS_KEY_CAL_DONE, &cal_done) == ESP_OK) {
        s_calibration.calibrated = (cal_done != 0);
    }

    if (s_calibration.calibrated) {
        load_float(NVS_KEY_CLR_GREEN, &s_calibration.clear_green);
        load_float(NVS_KEY_CLR_BLUE, &s_calibration.clear_blue);
        load_float(NVS_KEY_CLR_RED, &s_calibration.clear_red);
        load_float(NVS_KEY_CLR_RATIO, &s_calibration.clear_ratio);

        int64_t ts = 0;
        if (nvs_get_i64(s_nvs_handle, NVS_KEY_CLR_TS, &ts) == ESP_OK) {
            s_calibration.clear_timestamp = (time_t)ts;
        }

        ESP_LOGI(TAG, "Loaded clear water calibration: G=%.1f, B=%.1f, R=%.1f, ratio=%.3f",
                 s_calibration.clear_green, s_calibration.clear_blue,
                 s_calibration.clear_red, s_calibration.clear_ratio);
    }

    uint8_t drt_done = 0;
    if (nvs_get_u8(s_nvs_handle, NVS_KEY_DRT_DONE, &drt_done) == ESP_OK) {
        s_calibration.has_dirty_reference = (drt_done != 0);
    }

    if (s_calibration.has_dirty_reference) {
        load_float(NVS_KEY_DRT_GREEN, &s_calibration.dirty_green);
        load_float(NVS_KEY_DRT_RATIO, &s_calibration.dirty_ratio);
        load_float(NVS_KEY_DRT_NTU, &s_calibration.dirty_ntu_reference);

        int64_t ts = 0;
        if (nvs_get_i64(s_nvs_handle, NVS_KEY_DRT_TS, &ts) == ESP_OK) {
            s_calibration.dirty_timestamp = (time_t)ts;
        }

        ESP_LOGI(TAG, "Loaded dirty water reference: G=%.1f, ratio=%.3f, NTU=%.1f",
                 s_calibration.dirty_green, s_calibration.dirty_ratio,
                 s_calibration.dirty_ntu_reference);
    }

    return ESP_OK;
}

static esp_err_t save_clear_calibration(void)
{
    esp_err_t ret;

    ret = nvs_set_u8(s_nvs_handle, NVS_KEY_CAL_DONE, 1);
    if (ret != ESP_OK) return ret;

    ret = save_float(NVS_KEY_CLR_GREEN, s_calibration.clear_green);
    if (ret != ESP_OK) return ret;

    ret = save_float(NVS_KEY_CLR_BLUE, s_calibration.clear_blue);
    if (ret != ESP_OK) return ret;

    ret = save_float(NVS_KEY_CLR_RED, s_calibration.clear_red);
    if (ret != ESP_OK) return ret;

    ret = save_float(NVS_KEY_CLR_RATIO, s_calibration.clear_ratio);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_i64(s_nvs_handle, NVS_KEY_CLR_TS, (int64_t)s_calibration.clear_timestamp);
    if (ret != ESP_OK) return ret;

    return nvs_commit(s_nvs_handle);
}

static esp_err_t save_dirty_calibration(void)
{
    esp_err_t ret;

    ret = nvs_set_u8(s_nvs_handle, NVS_KEY_DRT_DONE, 1);
    if (ret != ESP_OK) return ret;

    ret = save_float(NVS_KEY_DRT_GREEN, s_calibration.dirty_green);
    if (ret != ESP_OK) return ret;

    ret = save_float(NVS_KEY_DRT_RATIO, s_calibration.dirty_ratio);
    if (ret != ESP_OK) return ret;

    ret = save_float(NVS_KEY_DRT_NTU, s_calibration.dirty_ntu_reference);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_i64(s_nvs_handle, NVS_KEY_DRT_TS, (int64_t)s_calibration.dirty_timestamp);
    if (ret != ESP_OK) return ret;

    return nvs_commit(s_nvs_handle);
}

// =============================================================================
// Moving Average Filter
// =============================================================================

static void update_filter(float ntu, float doc)
{
    s_ntu_buffer[s_filter_index] = ntu;
    s_doc_buffer[s_filter_index] = doc;

    s_filter_index = (s_filter_index + 1) % OPTICAL_MOVING_AVG_SIZE;
    if (s_filter_count < OPTICAL_MOVING_AVG_SIZE) {
        s_filter_count++;
    }

    // Calculate averages
    float ntu_sum = 0.0f;
    float doc_sum = 0.0f;
    for (uint8_t i = 0; i < s_filter_count; i++) {
        ntu_sum += s_ntu_buffer[i];
        doc_sum += s_doc_buffer[i];
    }

    s_filtered_ntu = ntu_sum / (float)s_filter_count;
    s_filtered_doc = doc_sum / (float)s_filter_count;
}

// =============================================================================
// Measurement Helpers
// =============================================================================

static optical_err_t read_channel(optical_channel_t *channel)
{
    tsl2591_reading_t reading;
    esp_err_t ret = tsl2591_read_raw(&reading);

    if (ret != ESP_OK) {
        memset(channel, 0, sizeof(optical_channel_t));
        return OPTICAL_ERR_TSL2591_FAILED;
    }

    channel->ch0_full = reading.ch0_full;
    channel->ch1_ir = reading.ch1_ir;
    channel->visible = reading.visible;
    channel->valid = reading.valid;
    channel->saturated = reading.saturated;

    return OPTICAL_OK;
}

static float calculate_ntu(float green_backscatter)
{
    float ntu;

    if (s_calibration.calibrated && s_calibration.has_dirty_reference) {
        // Full calibration: scale between clear and dirty
        float range = s_calibration.dirty_green - s_calibration.clear_green;
        if (fabsf(range) > 0.001f) {
            ntu = (green_backscatter - s_calibration.clear_green) / range
                  * s_calibration.dirty_ntu_reference;
        } else {
            ntu = 0.0f;
        }
    } else if (s_calibration.calibrated) {
        // Only clear calibration: offset from baseline
        ntu = (green_backscatter - s_calibration.clear_green) * UNCALIBRATED_NTU_FACTOR;
    } else {
        // No calibration: rough estimate
        ntu = green_backscatter * UNCALIBRATED_NTU_FACTOR;
    }

    return CLAMP(ntu, NTU_MIN, NTU_MAX);
}

static float calculate_doc_index(float blue_backscatter, float red_backscatter)
{
    // Avoid division by zero
    if (red_backscatter < 1.0f) {
        return 0.0f;
    }

    float current_ratio = blue_backscatter / red_backscatter;
    float doc_index;

    if (s_calibration.calibrated && s_calibration.has_dirty_reference) {
        // Full calibration: scale between clear and dirty ratios
        float range = s_calibration.clear_ratio - s_calibration.dirty_ratio;
        if (fabsf(range) > 0.001f) {
            doc_index = (s_calibration.clear_ratio - current_ratio) / range * 100.0f;
        } else {
            doc_index = 0.0f;
        }
    } else if (s_calibration.calibrated) {
        // Only clear calibration: percentage deviation from clear
        if (s_calibration.clear_ratio > 0.001f) {
            doc_index = (1.0f - current_ratio / s_calibration.clear_ratio) * 100.0f;
        } else {
            doc_index = (1.0f - current_ratio) * 100.0f;
        }
    } else {
        // No calibration: rough estimate (assuming ratio ~1.0 for clear water)
        doc_index = (1.0f - current_ratio) * 100.0f;
    }

    return CLAMP(doc_index, DOC_MIN, DOC_MAX);
}

// =============================================================================
// Public API Implementation
// =============================================================================

esp_err_t optical_sensor_init(i2c_master_bus_handle_t i2c_bus, int led_gpio)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // Open NVS namespace
    esp_err_t ret = nvs_open(OPTICAL_NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize TSL2591
    ret = tsl2591_init(i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TSL2591 not found - optical sensing disabled");
        // Continue initialization - optical sensing will be unavailable
    }

    // Initialize WS2812B
    ret = ws2812b_init(led_gpio);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WS2812B: %s", esp_err_to_name(ret));
        nvs_close(s_nvs_handle);
        return ret;
    }

    // Ensure LED is off
    ws2812b_off();

    // Configure TSL2591 for optical sensing
    if (tsl2591_is_present()) {
        tsl2591_config_t config = {
            .gain = TSL2591_GAIN_MED,           // 25x gain
            .integration_time = TSL2591_INTEGTIME_200MS
        };
        tsl2591_set_config(&config);
    }

    // Load calibration from NVS
    load_calibration();

    s_initialized = true;
    ESP_LOGI(TAG, "Optical sensor initialized (TSL2591: %s, WS2812B: GPIO %d)",
             tsl2591_is_present() ? "present" : "not found", led_gpio);

    return ESP_OK;
}

optical_err_t optical_sensor_measure(optical_measurement_t *result)
{
    if (!s_initialized) {
        return OPTICAL_ERR_NOT_INITIALIZED;
    }

    if (result == NULL) {
        return OPTICAL_ERR_INVALID_ARG;
    }

    if (!tsl2591_is_present()) {
        return OPTICAL_ERR_TSL2591_NOT_FOUND;
    }

    memset(result, 0, sizeof(optical_measurement_t));
    result->timestamp = time(NULL);

    // Enable TSL2591
    esp_err_t ret = tsl2591_enable();
    if (ret != ESP_OK) {
        return OPTICAL_ERR_TSL2591_FAILED;
    }

    // Wait for sensor to stabilize
    vTaskDelay(pdMS_TO_TICKS(50));

    // Step 1: Check ambient light
    optical_err_t err = read_channel(&result->dark);
    if (err != OPTICAL_OK) {
        tsl2591_disable();
        return err;
    }

    if (result->dark.ch0_full > OPTICAL_AMBIENT_THRESHOLD) {
        ESP_LOGW(TAG, "High ambient light detected (%u), skipping measurement",
                 result->dark.ch0_full);
        result->aborted_high_ambient = true;
        s_high_ambient_count++;
        tsl2591_disable();
        ws2812b_off();
        return OPTICAL_ERR_HIGH_AMBIENT;
    }

    // Step 2: Green LED measurement
    ws2812b_set_color(WS2812B_COLOR_GREEN_50);
    vTaskDelay(pdMS_TO_TICKS(OPTICAL_LED_STABILIZE_MS));
    // Wait for integration
    vTaskDelay(pdMS_TO_TICKS(tsl2591_get_integration_ms(TSL2591_INTEGTIME_200MS)));
    err = read_channel(&result->green);
    if (err != OPTICAL_OK) {
        ws2812b_off();
        tsl2591_disable();
        return err;
    }

    // Step 3: Blue LED measurement
    ws2812b_set_color(WS2812B_COLOR_BLUE_50);
    vTaskDelay(pdMS_TO_TICKS(OPTICAL_LED_STABILIZE_MS));
    vTaskDelay(pdMS_TO_TICKS(tsl2591_get_integration_ms(TSL2591_INTEGTIME_200MS)));
    err = read_channel(&result->blue);
    if (err != OPTICAL_OK) {
        ws2812b_off();
        tsl2591_disable();
        return err;
    }

    // Step 4: Red LED measurement
    ws2812b_set_color(WS2812B_COLOR_RED_50);
    vTaskDelay(pdMS_TO_TICKS(OPTICAL_LED_STABILIZE_MS));
    vTaskDelay(pdMS_TO_TICKS(tsl2591_get_integration_ms(TSL2591_INTEGTIME_200MS)));
    err = read_channel(&result->red);
    if (err != OPTICAL_OK) {
        ws2812b_off();
        tsl2591_disable();
        return err;
    }

    // Cleanup
    ws2812b_off();
    tsl2591_disable();

    // Step 5: Calculate dark-corrected backscatter
    result->backscatter_green = result->green.visible - result->dark.visible;
    result->backscatter_blue = result->blue.visible - result->dark.visible;
    result->backscatter_red = result->red.visible - result->dark.visible;

    // Ensure non-negative
    if (result->backscatter_green < 0) result->backscatter_green = 0;
    if (result->backscatter_blue < 0) result->backscatter_blue = 0;
    if (result->backscatter_red < 0) result->backscatter_red = 0;

    // Step 6: Calculate NTU and DOC index
    result->ntu = calculate_ntu(result->backscatter_green);
    result->doc_index = calculate_doc_index(result->backscatter_blue, result->backscatter_red);

    // Step 7: Update filter
    update_filter(result->ntu, result->doc_index);

    // Update status
    result->valid = true;
    s_last_measurement_time = result->timestamp;
    s_measurement_count++;
    s_last_ntu_raw = result->ntu;
    s_last_doc_raw = result->doc_index;

    ESP_LOGI(TAG, "Measurement: NTU=%.2f (filtered=%.2f), DOC=%.1f (filtered=%.1f)",
             result->ntu, s_filtered_ntu, result->doc_index, s_filtered_doc);
    ESP_LOGD(TAG, "Backscatter: G=%.0f, B=%.0f, R=%.0f (dark=%.0f)",
             result->backscatter_green, result->backscatter_blue,
             result->backscatter_red, result->dark.visible);

    return OPTICAL_OK;
}

optical_err_t optical_sensor_calibrate_clear(void)
{
    if (!s_initialized) {
        return OPTICAL_ERR_NOT_INITIALIZED;
    }

    optical_measurement_t measurement;
    optical_err_t err = optical_sensor_measure(&measurement);
    if (err != OPTICAL_OK) {
        return err;
    }

    // Store clear water reference
    s_calibration.calibrated = true;
    s_calibration.clear_green = measurement.backscatter_green;
    s_calibration.clear_blue = measurement.backscatter_blue;
    s_calibration.clear_red = measurement.backscatter_red;

    // Calculate blue/red ratio for clear water
    if (measurement.backscatter_red > 1.0f) {
        s_calibration.clear_ratio = measurement.backscatter_blue / measurement.backscatter_red;
    } else {
        s_calibration.clear_ratio = 1.0f;
    }

    s_calibration.clear_timestamp = time(NULL);

    // Save to NVS
    esp_err_t ret = save_clear_calibration();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save clear calibration: %s", esp_err_to_name(ret));
        return OPTICAL_ERR_NVS_FAILED;
    }

    // Reset filter
    s_filter_count = 0;
    s_filter_index = 0;
    s_filtered_ntu = 0.0f;
    s_filtered_doc = 0.0f;

    ESP_LOGI(TAG, "Clear water calibration saved: G=%.1f, B=%.1f, R=%.1f, ratio=%.3f",
             s_calibration.clear_green, s_calibration.clear_blue,
             s_calibration.clear_red, s_calibration.clear_ratio);

    return OPTICAL_OK;
}

optical_err_t optical_sensor_calibrate_dirty(float ntu_reference)
{
    if (!s_initialized) {
        return OPTICAL_ERR_NOT_INITIALIZED;
    }

    if (ntu_reference <= 0.0f) {
        return OPTICAL_ERR_INVALID_ARG;
    }

    optical_measurement_t measurement;
    optical_err_t err = optical_sensor_measure(&measurement);
    if (err != OPTICAL_OK) {
        return err;
    }

    // Store dirty water reference
    s_calibration.has_dirty_reference = true;
    s_calibration.dirty_green = measurement.backscatter_green;

    // Calculate blue/red ratio for dirty water
    if (measurement.backscatter_red > 1.0f) {
        s_calibration.dirty_ratio = measurement.backscatter_blue / measurement.backscatter_red;
    } else {
        s_calibration.dirty_ratio = 0.5f;  // Assume some yellowing
    }

    s_calibration.dirty_ntu_reference = ntu_reference;
    s_calibration.dirty_timestamp = time(NULL);

    // Save to NVS
    esp_err_t ret = save_dirty_calibration();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save dirty calibration: %s", esp_err_to_name(ret));
        return OPTICAL_ERR_NVS_FAILED;
    }

    ESP_LOGI(TAG, "Dirty water calibration saved: G=%.1f, ratio=%.3f, NTU=%.1f",
             s_calibration.dirty_green, s_calibration.dirty_ratio,
             s_calibration.dirty_ntu_reference);

    return OPTICAL_OK;
}

optical_err_t optical_sensor_clear_calibration(void)
{
    if (!s_initialized) {
        return OPTICAL_ERR_NOT_INITIALIZED;
    }

    // Clear calibration data
    memset(&s_calibration, 0, sizeof(s_calibration));

    // Clear NVS
    nvs_set_u8(s_nvs_handle, NVS_KEY_CAL_DONE, 0);
    nvs_set_u8(s_nvs_handle, NVS_KEY_DRT_DONE, 0);
    nvs_commit(s_nvs_handle);

    // Reset filter
    s_filter_count = 0;
    s_filter_index = 0;
    s_filtered_ntu = -1.0f;
    s_filtered_doc = -1.0f;

    ESP_LOGI(TAG, "Calibration cleared");
    return OPTICAL_OK;
}

optical_err_t optical_sensor_get_calibration(optical_calibration_t *cal)
{
    if (cal == NULL) {
        return OPTICAL_ERR_INVALID_ARG;
    }

    *cal = s_calibration;
    return OPTICAL_OK;
}

optical_err_t optical_sensor_get_status(optical_status_t *status)
{
    if (status == NULL) {
        return OPTICAL_ERR_INVALID_ARG;
    }

    memset(status, 0, sizeof(optical_status_t));

    status->tsl2591_present = tsl2591_is_present();
    status->ws2812b_initialized = ws2812b_is_initialized();
    status->calibrated = s_calibration.calibrated;
    status->has_dirty_reference = s_calibration.has_dirty_reference;

    status->last_ntu = s_filtered_ntu;
    status->last_doc_index = s_filtered_doc;
    status->last_ntu_raw = s_last_ntu_raw;
    status->last_doc_raw = s_last_doc_raw;

    status->last_measurement_time = s_last_measurement_time;
    status->measurement_count = s_measurement_count;
    status->high_ambient_count = s_high_ambient_count;

    // Warning states would be set by warning_manager
    status->ntu_warning_state = 0;
    status->doc_warning_state = 0;

    return OPTICAL_OK;
}

float optical_sensor_get_filtered_ntu(void)
{
    return s_filtered_ntu;
}

float optical_sensor_get_filtered_doc(void)
{
    return s_filtered_doc;
}

bool optical_sensor_is_ready(void)
{
    return s_initialized && tsl2591_is_present();
}

esp_err_t optical_sensor_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    ws2812b_off();
    ws2812b_deinit();
    tsl2591_disable();

    if (s_nvs_handle != 0) {
        nvs_close(s_nvs_handle);
        s_nvs_handle = 0;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Optical sensor deinitialized");
    return ESP_OK;
}
