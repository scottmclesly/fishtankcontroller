/**
 * @file display_driver.h
 * @brief SSD1306 OLED Display Driver
 *
 * Minimal driver for 128x32 SSD1306 OLED display.
 * Displays sensor metrics with cycling.
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
#define DISPLAY_I2C_ADDR        0x3C
#define DISPLAY_WIDTH           128
#define DISPLAY_HEIGHT          32

// =============================================================================
// Display Metric Enumeration
// =============================================================================
typedef enum {
    DISPLAY_METRIC_TEMPERATURE = 0,
    DISPLAY_METRIC_ORP,
    DISPLAY_METRIC_PH,
    DISPLAY_METRIC_EC,
    DISPLAY_METRIC_COUNT
} display_metric_t;

// =============================================================================
// Sensor Data for Display
// =============================================================================
typedef struct {
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    bool valid;
} display_data_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize display driver
 * @param bus I2C master bus handle
 * @return ESP_OK on success
 */
esp_err_t display_driver_init(i2c_master_bus_handle_t bus);

/**
 * @brief Check if display is present
 * @return true if display responds
 */
bool display_driver_is_present(void);

/**
 * @brief Clear display
 */
void display_driver_clear(void);

/**
 * @brief Update display buffer (call display_driver_refresh to show)
 */
void display_driver_refresh(void);

/**
 * @brief Set display brightness
 * @param brightness 0-255
 */
void display_driver_set_brightness(uint8_t brightness);

/**
 * @brief Turn display on/off
 * @param on true to turn on
 */
void display_driver_power(bool on);

// -----------------------------------------------------------------------------
// High-Level Display Functions
// -----------------------------------------------------------------------------

/**
 * @brief Show a specific metric on display
 * @param metric Which metric to show
 * @param data Pointer to sensor data
 */
void display_driver_show_metric(display_metric_t metric, const display_data_t *data);

/**
 * @brief Show temperature
 * @param temp_c Temperature in Celsius
 * @param valid true if reading is valid
 */
void display_driver_show_temperature(float temp_c, bool valid);

/**
 * @brief Show ORP
 * @param orp_mv ORP in millivolts
 * @param valid true if reading is valid
 */
void display_driver_show_orp(float orp_mv, bool valid);

/**
 * @brief Show pH
 * @param ph pH value
 * @param valid true if reading is valid
 */
void display_driver_show_ph(float ph, bool valid);

/**
 * @brief Show EC/conductivity
 * @param ec_ms_cm EC in mS/cm
 * @param valid true if reading is valid
 */
void display_driver_show_ec(float ec_ms_cm, bool valid);

/**
 * @brief Show status message
 * @param line1 First line (max 16 chars)
 * @param line2 Second line (max 16 chars)
 */
void display_driver_show_status(const char *line1, const char *line2);

/**
 * @brief Show WiFi status
 * @param connected true if WiFi connected
 * @param ip_addr IP address string (or NULL)
 */
void display_driver_show_wifi_status(bool connected, const char *ip_addr);

/**
 * @brief Show error message
 * @param message Error message
 */
void display_driver_show_error(const char *message);

// -----------------------------------------------------------------------------
// Low-Level Drawing Functions
// -----------------------------------------------------------------------------

/**
 * @brief Draw a pixel
 * @param x X coordinate
 * @param y Y coordinate
 * @param color 1 = white, 0 = black
 */
void display_driver_draw_pixel(int16_t x, int16_t y, uint8_t color);

/**
 * @brief Draw text
 * @param x X coordinate
 * @param y Y coordinate
 * @param text Text string
 * @param size Text size (1 = 6x8, 2 = 12x16)
 */
void display_driver_draw_text(int16_t x, int16_t y, const char *text, uint8_t size);

/**
 * @brief Draw large number (for metric display)
 * @param x X coordinate
 * @param y Y coordinate
 * @param value Numeric value
 * @param decimals Number of decimal places
 */
void display_driver_draw_large_number(int16_t x, int16_t y, float value, uint8_t decimals);

#ifdef __cplusplus
}
#endif
