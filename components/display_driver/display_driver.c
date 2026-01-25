/**
 * @file display_driver.c
 * @brief SSD1306 OLED Display Driver for 128x32 display
 */

#include "display_driver.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "display_driver";

// SSD1306 Commands
#define SSD1306_SETCONTRAST         0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON        0xA5
#define SSD1306_NORMALDISPLAY       0xA6
#define SSD1306_INVERTDISPLAY       0xA7
#define SSD1306_DISPLAYOFF          0xAE
#define SSD1306_DISPLAYON           0xAF
#define SSD1306_SETDISPLAYOFFSET    0xD3
#define SSD1306_SETCOMPINS          0xDA
#define SSD1306_SETVCOMDETECT       0xDB
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5
#define SSD1306_SETPRECHARGE        0xD9
#define SSD1306_SETMULTIPLEX        0xA8
#define SSD1306_SETLOWCOLUMN        0x00
#define SSD1306_SETHIGHCOLUMN       0x10
#define SSD1306_SETSTARTLINE        0x40
#define SSD1306_MEMORYMODE          0x20
#define SSD1306_COLUMNADDR          0x21
#define SSD1306_PAGEADDR            0x22
#define SSD1306_COMSCANINC          0xC0
#define SSD1306_COMSCANDEC          0xC8
#define SSD1306_SEGREMAP            0xA0
#define SSD1306_CHARGEPUMP          0x8D

// Simple 5x7 font (ASCII 32-127)
static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // space
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x08, 0x2A, 0x1C, 0x2A, 0x08, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x50, 0x30, 0x00, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x00, 0x08, 0x14, 0x22, 0x41, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x41, 0x22, 0x14, 0x08, 0x00, // >
    0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x01, 0x01, // F
    0x3E, 0x41, 0x41, 0x51, 0x32, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x04, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x7F, 0x20, 0x18, 0x20, 0x7F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x03, 0x04, 0x78, 0x04, 0x03, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x00, 0x00, 0x7F, 0x41, 0x41, // [
    0x02, 0x04, 0x08, 0x10, 0x20, // backslash
    0x41, 0x41, 0x7F, 0x00, 0x00, // ]
    0x04, 0x02, 0x01, 0x02, 0x04, // ^
    0x40, 0x40, 0x40, 0x40, 0x40, // _
    0x00, 0x01, 0x02, 0x04, 0x00, // `
    0x20, 0x54, 0x54, 0x54, 0x78, // a
    0x7F, 0x48, 0x44, 0x44, 0x38, // b
    0x38, 0x44, 0x44, 0x44, 0x20, // c
    0x38, 0x44, 0x44, 0x48, 0x7F, // d
    0x38, 0x54, 0x54, 0x54, 0x18, // e
    0x08, 0x7E, 0x09, 0x01, 0x02, // f
    0x08, 0x14, 0x54, 0x54, 0x3C, // g
    0x7F, 0x08, 0x04, 0x04, 0x78, // h
    0x00, 0x44, 0x7D, 0x40, 0x00, // i
    0x20, 0x40, 0x44, 0x3D, 0x00, // j
    0x00, 0x7F, 0x10, 0x28, 0x44, // k
    0x00, 0x41, 0x7F, 0x40, 0x00, // l
    0x7C, 0x04, 0x18, 0x04, 0x78, // m
    0x7C, 0x08, 0x04, 0x04, 0x78, // n
    0x38, 0x44, 0x44, 0x44, 0x38, // o
    0x7C, 0x14, 0x14, 0x14, 0x08, // p
    0x08, 0x14, 0x14, 0x18, 0x7C, // q
    0x7C, 0x08, 0x04, 0x04, 0x08, // r
    0x48, 0x54, 0x54, 0x54, 0x20, // s
    0x04, 0x3F, 0x44, 0x40, 0x20, // t
    0x3C, 0x40, 0x40, 0x20, 0x7C, // u
    0x1C, 0x20, 0x40, 0x20, 0x1C, // v
    0x3C, 0x40, 0x30, 0x40, 0x3C, // w
    0x44, 0x28, 0x10, 0x28, 0x44, // x
    0x0C, 0x50, 0x50, 0x50, 0x3C, // y
    0x44, 0x64, 0x54, 0x4C, 0x44, // z
    0x00, 0x08, 0x36, 0x41, 0x00, // {
    0x00, 0x00, 0x7F, 0x00, 0x00, // |
    0x00, 0x41, 0x36, 0x08, 0x00, // }
    0x08, 0x08, 0x2A, 0x1C, 0x08, // ->
    0x08, 0x1C, 0x2A, 0x08, 0x08, // <-
};

// State
static i2c_master_dev_handle_t s_display_dev = NULL;
static uint8_t s_framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8] = {0};
static bool s_initialized = false;

// I2C write helper
static esp_err_t display_write_cmd(uint8_t cmd)
{
    if (!s_display_dev) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = {0x00, cmd};  // Co=0, D/C=0 for command
    return i2c_master_transmit(s_display_dev, buf, 2, 100);
}

static esp_err_t display_write_data(const uint8_t *data, size_t len)
{
    if (!s_display_dev) return ESP_ERR_INVALID_STATE;

    // Write in chunks with data control byte
    uint8_t buf[129];  // 1 control + up to 128 data
    buf[0] = 0x40;  // Co=0, D/C=1 for data

    size_t offset = 0;
    while (offset < len) {
        size_t chunk = (len - offset > 128) ? 128 : (len - offset);
        memcpy(&buf[1], &data[offset], chunk);
        esp_err_t ret = i2c_master_transmit(s_display_dev, buf, chunk + 1, 100);
        if (ret != ESP_OK) return ret;
        offset += chunk;
    }
    return ESP_OK;
}

esp_err_t display_driver_init(i2c_master_bus_handle_t bus)
{
    if (!bus) {
        ESP_LOGE(TAG, "Invalid I2C bus handle");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing SSD1306 display at 0x%02X", DISPLAY_I2C_ADDR);

    // Create I2C device
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DISPLAY_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_display_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return ret;
    }

    // SSD1306 initialization sequence for 128x32
    const uint8_t init_cmds[] = {
        SSD1306_DISPLAYOFF,
        SSD1306_SETDISPLAYCLOCKDIV, 0x80,
        SSD1306_SETMULTIPLEX, 0x1F,           // 32 rows - 1
        SSD1306_SETDISPLAYOFFSET, 0x00,
        SSD1306_SETSTARTLINE | 0x00,
        SSD1306_CHARGEPUMP, 0x14,             // Enable charge pump
        SSD1306_MEMORYMODE, 0x00,             // Horizontal addressing
        SSD1306_SEGREMAP | 0x01,              // Segment remap
        SSD1306_COMSCANDEC,                   // COM scan direction
        SSD1306_SETCOMPINS, 0x02,             // COM pins for 128x32
        SSD1306_SETCONTRAST, 0x8F,
        SSD1306_SETPRECHARGE, 0xF1,
        SSD1306_SETVCOMDETECT, 0x40,
        SSD1306_DISPLAYALLON_RESUME,
        SSD1306_NORMALDISPLAY,
        SSD1306_DISPLAYON
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        ret = display_write_cmd(init_cmds[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Init command %d failed: %s", (int)i, esp_err_to_name(ret));
            return ret;
        }
    }

    s_initialized = true;
    display_driver_clear();
    display_driver_refresh();

    ESP_LOGI(TAG, "SSD1306 initialized successfully");
    return ESP_OK;
}

bool display_driver_is_present(void)
{
    return s_initialized && s_display_dev != NULL;
}

void display_driver_clear(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
}

void display_driver_refresh(void)
{
    if (!s_initialized) return;

    // Set column and page address
    display_write_cmd(SSD1306_COLUMNADDR);
    display_write_cmd(0);
    display_write_cmd(DISPLAY_WIDTH - 1);
    display_write_cmd(SSD1306_PAGEADDR);
    display_write_cmd(0);
    display_write_cmd((DISPLAY_HEIGHT / 8) - 1);

    // Send framebuffer
    display_write_data(s_framebuffer, sizeof(s_framebuffer));
}

void display_driver_set_brightness(uint8_t brightness)
{
    if (!s_initialized) return;
    display_write_cmd(SSD1306_SETCONTRAST);
    display_write_cmd(brightness);
}

void display_driver_power(bool on)
{
    if (!s_initialized) return;
    display_write_cmd(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void display_driver_draw_pixel(int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;

    // SSD1306 uses pages (8 vertical pixels per page)
    uint16_t byte_idx = x + (y / 8) * DISPLAY_WIDTH;
    uint8_t bit = 1 << (y & 7);

    if (color) {
        s_framebuffer[byte_idx] |= bit;
    } else {
        s_framebuffer[byte_idx] &= ~bit;
    }
}

void display_driver_draw_text(int16_t x, int16_t y, const char *text, uint8_t size)
{
    if (!text) return;

    int16_t cursor_x = x;
    while (*text) {
        char c = *text++;
        if (c < 32 || c > 127) c = '?';

        const uint8_t *glyph = &font5x7[(c - 32) * 5];

        for (int col = 0; col < 5; col++) {
            uint8_t line = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (line & (1 << row)) {
                    if (size == 1) {
                        display_driver_draw_pixel(cursor_x + col, y + row, 1);
                    } else {
                        // Scale up for larger sizes
                        for (int sy = 0; sy < size; sy++) {
                            for (int sx = 0; sx < size; sx++) {
                                display_driver_draw_pixel(
                                    cursor_x + col * size + sx,
                                    y + row * size + sy, 1);
                            }
                        }
                    }
                }
            }
        }
        cursor_x += (5 + 1) * size;  // Character width + spacing
    }
}

void display_driver_draw_large_number(int16_t x, int16_t y, float value, uint8_t decimals)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.*f", decimals, (double)value);
    display_driver_draw_text(x, y, buf, 2);  // Size 2 = 12x16 pixels
}

void display_driver_show_metric(display_metric_t metric, const display_data_t *data)
{
    if (!data) return;

    switch (metric) {
        case DISPLAY_METRIC_TEMPERATURE:
            display_driver_show_temperature(data->temp_c, data->valid);
            break;
        case DISPLAY_METRIC_ORP:
            display_driver_show_orp(data->orp_mv, data->valid);
            break;
        case DISPLAY_METRIC_PH:
            display_driver_show_ph(data->ph, data->valid);
            break;
        case DISPLAY_METRIC_EC:
            display_driver_show_ec(data->ec_ms_cm, data->valid);
            break;
        default:
            break;
    }
}

void display_driver_show_temperature(float temp_c, bool valid)
{
    display_driver_clear();

    display_driver_draw_text(0, 0, "TEMP", 1);

    if (valid) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", (double)temp_c);
        display_driver_draw_text(0, 10, buf, 2);
        display_driver_draw_text(96, 16, "C", 2);
    } else {
        display_driver_draw_text(0, 12, "---", 2);
    }

    display_driver_refresh();
}

void display_driver_show_orp(float orp_mv, bool valid)
{
    display_driver_clear();

    display_driver_draw_text(0, 0, "ORP", 1);

    if (valid) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", (double)orp_mv);
        display_driver_draw_text(0, 10, buf, 2);
        display_driver_draw_text(80, 16, "mV", 2);
    } else {
        display_driver_draw_text(0, 12, "---", 2);
    }

    display_driver_refresh();
}

void display_driver_show_ph(float ph, bool valid)
{
    display_driver_clear();

    display_driver_draw_text(0, 0, "pH", 1);

    if (valid) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", (double)ph);
        display_driver_draw_text(0, 10, buf, 2);
    } else {
        display_driver_draw_text(0, 12, "---", 2);
    }

    display_driver_refresh();
}

void display_driver_show_ec(float ec_ms_cm, bool valid)
{
    display_driver_clear();

    display_driver_draw_text(0, 0, "EC", 1);

    if (valid) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", (double)ec_ms_cm);
        display_driver_draw_text(0, 10, buf, 2);
        display_driver_draw_text(80, 16, "mS", 2);
    } else {
        display_driver_draw_text(0, 12, "---", 2);
    }

    display_driver_refresh();
}

void display_driver_show_status(const char *line1, const char *line2)
{
    display_driver_clear();

    if (line1) {
        display_driver_draw_text(0, 0, line1, 1);
    }
    if (line2) {
        display_driver_draw_text(0, 16, line2, 1);
    }

    display_driver_refresh();
}

void display_driver_show_wifi_status(bool connected, const char *ip_addr)
{
    display_driver_clear();

    display_driver_draw_text(0, 0, "WiFi", 1);

    if (connected) {
        display_driver_draw_text(0, 10, "Connected", 1);
        if (ip_addr) {
            display_driver_draw_text(0, 20, ip_addr, 1);
        }
    } else {
        display_driver_draw_text(0, 10, "Disconnected", 1);
    }

    display_driver_refresh();
}

void display_driver_show_error(const char *message)
{
    display_driver_clear();

    display_driver_draw_text(0, 0, "ERROR", 1);
    if (message) {
        display_driver_draw_text(0, 12, message, 1);
    }

    display_driver_refresh();
}
