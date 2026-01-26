/**
 * @file ws2812b_driver.c
 * @brief WS2812B RGB LED Driver
 *
 * Uses ESP-IDF's led_strip component for RMT-based WS2812B control.
 */

#include "ws2812b_driver.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "ws2812b";

// =============================================================================
// Static Variables
// =============================================================================
static led_strip_handle_t s_led_strip = NULL;
static bool s_initialized = false;

// =============================================================================
// Public API Implementation
// =============================================================================

esp_err_t ws2812b_init(int gpio_num)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // LED strip configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_num,
        .max_leds = WS2812B_LED_COUNT,
        .led_model = LED_MODEL_WS2812,  // WS2812B uses GRB order internally
        .flags.invert_out = false,
    };

    // RMT configuration
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10 MHz
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    // Clear the LED
    ret = led_strip_clear(s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear LED: %s", esp_err_to_name(ret));
        led_strip_del(s_led_strip);
        s_led_strip = NULL;
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "WS2812B initialized on GPIO %d", gpio_num);
    return ESP_OK;
}

esp_err_t ws2812b_set_color(ws2812b_color_t color)
{
    return ws2812b_set_rgb(color.red, color.green, color.blue);
}

esp_err_t ws2812b_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_initialized || s_led_strip == NULL) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = led_strip_set_pixel(s_led_strip, 0, red, green, blue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set pixel: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = led_strip_refresh(s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Set color: R=%u G=%u B=%u", red, green, blue);
    return ESP_OK;
}

esp_err_t ws2812b_off(void)
{
    if (!s_initialized || s_led_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = led_strip_clear(s_led_strip);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "LED off");
    }
    return ret;
}

esp_err_t ws2812b_deinit(void)
{
    if (!s_initialized || s_led_strip == NULL) {
        return ESP_OK;
    }

    // Turn off LED before deinit
    led_strip_clear(s_led_strip);

    esp_err_t ret = led_strip_del(s_led_strip);
    s_led_strip = NULL;
    s_initialized = false;

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WS2812B deinitialized");
    }
    return ret;
}

bool ws2812b_is_initialized(void)
{
    return s_initialized;
}
