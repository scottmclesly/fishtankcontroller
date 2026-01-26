/**
 * @file ws2812b_driver.h
 * @brief WS2812B RGB LED Driver
 *
 * Simple driver for controlling a WS2812B LED using ESP-IDF's led_strip component.
 * Designed for optical sensing with controlled color pulses.
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
#define WS2812B_GPIO            1       // Default GPIO for LED data line
#define WS2812B_LED_COUNT       1       // Single LED

// =============================================================================
// Color Structure
// =============================================================================
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ws2812b_color_t;

// =============================================================================
// Predefined Colors (50% brightness for optical sensing)
// =============================================================================
#define WS2812B_COLOR_OFF       ((ws2812b_color_t){0, 0, 0})
#define WS2812B_COLOR_RED_50    ((ws2812b_color_t){128, 0, 0})
#define WS2812B_COLOR_GREEN_50  ((ws2812b_color_t){0, 128, 0})
#define WS2812B_COLOR_BLUE_50   ((ws2812b_color_t){0, 0, 128})
#define WS2812B_COLOR_WHITE_50  ((ws2812b_color_t){128, 128, 128})

// Full brightness colors
#define WS2812B_COLOR_RED       ((ws2812b_color_t){255, 0, 0})
#define WS2812B_COLOR_GREEN     ((ws2812b_color_t){0, 255, 0})
#define WS2812B_COLOR_BLUE      ((ws2812b_color_t){0, 0, 255})
#define WS2812B_COLOR_WHITE     ((ws2812b_color_t){255, 255, 255})

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize the WS2812B LED driver
 * @param gpio_num GPIO pin connected to LED data line (use WS2812B_GPIO for default)
 * @return ESP_OK on success
 */
esp_err_t ws2812b_init(int gpio_num);

/**
 * @brief Set LED color
 * @param color Color to set
 * @return ESP_OK on success
 */
esp_err_t ws2812b_set_color(ws2812b_color_t color);

/**
 * @brief Set LED color by RGB values
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @return ESP_OK on success
 */
esp_err_t ws2812b_set_rgb(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Turn off the LED
 * @return ESP_OK on success
 */
esp_err_t ws2812b_off(void);

/**
 * @brief Deinitialize the driver and release resources
 * @return ESP_OK on success
 */
esp_err_t ws2812b_deinit(void);

/**
 * @brief Check if driver is initialized
 * @return true if initialized
 */
bool ws2812b_is_initialized(void);

#ifdef __cplusplus
}
#endif
