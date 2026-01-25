/**
 * @file main.c
 * @brief Fishtank Controller - Main Application Entry Point
 *
 * ESP-IDF based aquarium controller for monitoring pH, ORP, EC, and temperature
 * using the POET I2C sensor. Features web UI, MQTT with Home Assistant integration,
 * OLED display, and OTA updates.
 *
 * Target Hardware: ESP32-C6 (Seeed XIAO ESP32-C6)
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"

// Phase 2 components (implemented)
#include "poet_sensor.h"
#include "calibration.h"
#include "warning_manager.h"
#include "derived_metrics.h"
#include "tank_settings.h"

// Phase 3 components (implemented)
#include "wifi_manager.h"
#include "mqtt_manager.h"

// Phase 4 components (implemented)
#include "http_server.h"

// Phase 5 components (implemented)
#include "display_driver.h"
#include "data_history.h"
#include "ota_manager.h"

// HTTP server instance
static http_server_t s_http_server = {0};

static const char *TAG = "fishtank_main";

// =============================================================================
// Pin Definitions (Seeed XIAO ESP32-C6)
// =============================================================================
#define I2C_MASTER_SCL_IO       7       // GPIO for I2C clock
#define I2C_MASTER_SDA_IO       6       // GPIO for I2C data
#define I2C_MASTER_FREQ_HZ      400000  // I2C clock frequency (400kHz for POET)
#define I2C_PORT_NUM            I2C_NUM_0

// =============================================================================
// I2C Device Addresses
// =============================================================================
#define POET_SENSOR_ADDR        0x1F    // POET pH/ORP/EC/Temp sensor
#define SSD1306_DISPLAY_ADDR    0x3C    // OLED display

// =============================================================================
// Task Configuration
// =============================================================================
#define SENSOR_TASK_STACK_SIZE  4096
#define SENSOR_TASK_PRIORITY    5

#define HTTP_TASK_STACK_SIZE    8192
#define HTTP_TASK_PRIORITY      4

#define MQTT_TASK_STACK_SIZE    4096
#define MQTT_TASK_PRIORITY      3

#define DISPLAY_TASK_STACK_SIZE 2048
#define DISPLAY_TASK_PRIORITY   2

#define MONITOR_TASK_STACK_SIZE 2048
#define MONITOR_TASK_PRIORITY   1

// =============================================================================
// Timing Configuration
// =============================================================================
#define SENSOR_READ_INTERVAL_MS     5000    // Read sensors every 5 seconds
#define POET_MEASUREMENT_DELAY_MS   2800    // POET sensor measurement time
#define DISPLAY_CYCLE_INTERVAL_MS   3000    // Cycle display metrics every 3 seconds

// =============================================================================
// Global Synchronization Primitives
// =============================================================================
static QueueHandle_t sensor_data_queue = NULL;
static EventGroupHandle_t system_events = NULL;

// Event bits
#define WIFI_CONNECTED_BIT      BIT0
#define SENSOR_DATA_READY_BIT   BIT1
#define MQTT_CONNECTED_BIT      BIT2

// =============================================================================
// Shared Data Structure
// =============================================================================
typedef struct {
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    float tds_ppm;
    float co2_ppm;
    float nh3_ratio;
    float nh3_ppm;
    float max_do_mg_l;
    float stocking_density;
    bool valid;
    uint8_t temp_warning_state;
    uint8_t ph_warning_state;
    uint8_t nh3_warning_state;
    uint8_t orp_warning_state;
    uint8_t ec_warning_state;
    uint8_t do_warning_state;
    time_t timestamp;
} sensor_data_t;

// =============================================================================
// I2C Bus Handle (shared between POET sensor and display)
// =============================================================================
static i2c_master_bus_handle_t i2c_bus_handle = NULL;

/**
 * @brief Initialize the I2C master bus
 * @return ESP_OK on success
 */
static esp_err_t i2c_master_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C master bus initialized (SDA: GPIO%d, SCL: GPIO%d, Freq: %d Hz)",
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);

    return ESP_OK;
}

/**
 * @brief Sensor reading task
 *
 * Reads POET sensor every SENSOR_READ_INTERVAL_MS, applies calibration,
 * calculates derived metrics, and posts data to the queue.
 */
static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started");
    TickType_t last_wake_time = xTaskGetTickCount();

    // Initialize POET sensor
    esp_err_t ret = poet_sensor_init(i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize POET sensor: %s", esp_err_to_name(ret));
        // Continue running but with invalid data
    }

    // Get tank settings for derived metrics
    float tds_factor = tank_settings_get_tds_factor();
    float kh_dkh = tank_settings_get_kh();
    float tan_ppm = tank_settings_get_tan();
    float tank_volume = tank_settings_get_volume();
    float total_fish_length = tank_settings_get_total_fish_length();

    while (1) {
        sensor_data_t data = {0};
        data.valid = false;

        // Start async measurement (all sensors: temp, ORP, pH, EC)
        ret = poet_sensor_measure_async(POET_CMD_ALL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start POET measurement: %s", esp_err_to_name(ret));
            goto next_cycle;
        }

        // Wait for measurement to complete
        vTaskDelay(pdMS_TO_TICKS(POET_MEASUREMENT_DELAY_MS));

        // Read raw results
        poet_result_t raw;
        ret = poet_sensor_read_result(POET_CMD_ALL, &raw);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read POET result: %s", esp_err_to_name(ret));
            goto next_cycle;
        }

        // Store raw values with unit conversion
        data.temp_c = poet_convert_temp_c(raw.temp_mC);
        data.orp_mv = poet_convert_orp_mv(raw.orp_uV);

        // Apply pH calibration
        data.ph = calibration_calculate_ph(raw.ugs_uV);

        // Apply EC calibration (converts nA/uV to mS/cm)
        data.ec_ms_cm = calibration_calculate_ec(raw.ec_nA, raw.ec_uV);

        // Calculate derived metrics
        data.tds_ppm = derived_metrics_calc_tds(data.ec_ms_cm, tds_factor);
        data.co2_ppm = derived_metrics_calc_co2(data.ph, kh_dkh);
        data.nh3_ratio = derived_metrics_calc_nh3_ratio(data.temp_c, data.ph);
        data.nh3_ppm = derived_metrics_calc_nh3(tan_ppm, data.nh3_ratio);
        data.max_do_mg_l = derived_metrics_calc_max_do(data.temp_c, 0.0f); // Assume freshwater
        data.stocking_density = derived_metrics_calc_stocking_density(total_fish_length, tank_volume);

        // Evaluate warning thresholds
        data.temp_warning_state = warning_manager_evaluate_temp(data.temp_c);
        data.ph_warning_state = warning_manager_evaluate_ph(data.ph);
        data.nh3_warning_state = warning_manager_evaluate_nh3(data.nh3_ppm);
        data.orp_warning_state = warning_manager_evaluate_orp(data.orp_mv);
        data.ec_warning_state = warning_manager_evaluate_ec(data.ec_ms_cm * 1000.0f);  // Convert to uS/cm for thresholds
        data.do_warning_state = warning_manager_evaluate_do(data.max_do_mg_l);

        // Mark data as valid and add timestamp
        data.valid = true;
        data.timestamp = time(NULL);

        // Post to queue (overwrite old data)
        xQueueOverwrite(sensor_data_queue, &data);
        xEventGroupSetBits(system_events, SENSOR_DATA_READY_BIT);

        // Add to data history
        data_point_t history_point = {
            .timestamp = data.timestamp,
            .temp_c = data.temp_c,
            .orp_mv = data.orp_mv,
            .ph = data.ph,
            .ec_ms_cm = data.ec_ms_cm,
            .tds_ppm = data.tds_ppm,
            .co2_ppm = data.co2_ppm,
            .nh3_ppm = data.nh3_ppm,
            .valid = data.valid
        };
        data_history_add(&history_point);

        // Update HTTP server with sensor data and broadcast to WebSocket clients
        http_server_update_sensor_data(
            data.temp_c, data.orp_mv, data.ph, data.ec_ms_cm,
            data.tds_ppm, data.co2_ppm, data.nh3_ppm, data.max_do_mg_l,
            data.temp_warning_state, data.ph_warning_state, data.orp_warning_state,
            data.ec_warning_state, data.nh3_warning_state, data.do_warning_state, data.valid);
        http_server_broadcast_sensor_data();

        ESP_LOGI(TAG, "Sensor: T=%.1f°C, ORP=%.0fmV, pH=%.2f, EC=%.3fmS/cm",
                 data.temp_c, data.orp_mv, data.ph, data.ec_ms_cm);
        ESP_LOGD(TAG, "Derived: TDS=%.0fppm, CO2=%.0fppm, NH3=%.3fppm, DO=%.1fmg/L",
                 data.tds_ppm, data.co2_ppm, data.nh3_ppm, data.max_do_mg_l);

next_cycle:
        // Wait for next interval
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));

        // Refresh tank settings periodically (in case they changed via web UI)
        tds_factor = tank_settings_get_tds_factor();
        kh_dkh = tank_settings_get_kh();
        tan_ppm = tank_settings_get_tan();
        tank_volume = tank_settings_get_volume();
        total_fish_length = tank_settings_get_total_fish_length();
    }
}

/**
 * @brief HTTP server task
 *
 * Starts the web server after WiFi connection and handles REST API requests.
 */
static void http_task(void *pvParameters)
{
    ESP_LOGI(TAG, "HTTP task started, waiting for WiFi...");

    // Wait for WiFi connection (or AP mode) - timeout allows AP mode startup
    (void)xEventGroupWaitBits(system_events, WIFI_CONNECTED_BIT,
                              pdFALSE, pdTRUE, pdMS_TO_TICKS(35000));

    // Start HTTP server (works in both STA and AP mode)
    ESP_LOGI(TAG, "Starting HTTP server...");
    esp_err_t ret = http_server_start(&s_http_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "HTTP server running on port 80");
    }

    while (1) {
        // Monitor server health
        if (!http_server_is_running(&s_http_server)) {
            ESP_LOGW(TAG, "HTTP server stopped, restarting...");
            http_server_start(&s_http_server);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief MQTT publishing task
 *
 * Publishes sensor data to MQTT broker when new data is available.
 */
static void mqtt_task(void *pvParameters)
{
    ESP_LOGI(TAG, "MQTT task started, waiting for WiFi...");

    // Wait for WiFi connection before MQTT operations
    xEventGroupWaitBits(system_events, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi connected, connecting to MQTT broker...");

    // Attempt initial connection
    esp_err_t ret = mqtt_manager_connect();
    if (ret == ESP_OK) {
        xEventGroupSetBits(system_events, MQTT_CONNECTED_BIT);
    }

    while (1) {
        // Wait for sensor data with timeout
        EventBits_t bits = xEventGroupWaitBits(
            system_events, SENSOR_DATA_READY_BIT,
            pdTRUE, pdTRUE, pdMS_TO_TICKS(10000)
        );

        if (bits & SENSOR_DATA_READY_BIT) {
            // Get sensor data from queue and publish
            sensor_data_t data;
            if (xQueuePeek(sensor_data_queue, &data, 0) == pdTRUE && data.valid) {
                if (mqtt_manager_is_connected()) {
                    // Convert sensor_data_t to mqtt_sensor_data_t
                    mqtt_sensor_data_t mqtt_data = {
                        .temp_c = data.temp_c,
                        .orp_mv = data.orp_mv,
                        .ph = data.ph,
                        .ec_ms_cm = data.ec_ms_cm,
                        .tds_ppm = data.tds_ppm,
                        .co2_ppm = data.co2_ppm,
                        .nh3_ratio = data.nh3_ratio,
                        .nh3_ppm = data.nh3_ppm,
                        .max_do_mg_l = data.max_do_mg_l,
                        .stocking_density = data.stocking_density,
                        .valid = data.valid,
                        .temp_warning_state = data.temp_warning_state,
                        .ph_warning_state = data.ph_warning_state,
                        .nh3_warning_state = data.nh3_warning_state,
                        .orp_warning_state = data.orp_warning_state,
                        .ec_warning_state = data.ec_warning_state,
                        .do_warning_state = data.do_warning_state,
                    };

                    ret = mqtt_manager_publish_sensor_data(&mqtt_data);
                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to publish sensor data: %s", esp_err_to_name(ret));
                    }
                }
            }
        }

        // Handle reconnection with exponential backoff
        mqtt_manager_loop();

        // Update connection status bit
        if (mqtt_manager_is_connected()) {
            xEventGroupSetBits(system_events, MQTT_CONNECTED_BIT);
        } else {
            xEventGroupClearBits(system_events, MQTT_CONNECTED_BIT);
        }
    }
}

/**
 * @brief Display cycling task
 *
 * Cycles through sensor metrics on the OLED display.
 */
static void display_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Display task started");

    // Initialize display
    esp_err_t ret = display_driver_init(i2c_bus_handle);
    bool display_available = (ret == ESP_OK);
    if (!display_available) {
        ESP_LOGW(TAG, "Display not available: %s", esp_err_to_name(ret));
    } else {
        display_driver_show_status("Aquarium", "Starting...");
    }

    display_metric_t current_metric = DISPLAY_METRIC_TEMPERATURE;

    while (1) {
        // Get latest sensor data
        sensor_data_t data;
        if (xQueuePeek(sensor_data_queue, &data, 0) == pdTRUE && data.valid) {
            if (display_available) {
                // Convert to display data format
                display_data_t disp_data = {
                    .temp_c = data.temp_c,
                    .orp_mv = data.orp_mv,
                    .ph = data.ph,
                    .ec_ms_cm = data.ec_ms_cm,
                    .valid = data.valid
                };
                display_driver_show_metric(current_metric, &disp_data);
            }

            // Log what we're displaying
            switch (current_metric) {
                case DISPLAY_METRIC_TEMPERATURE:
                    ESP_LOGD(TAG, "Display: Temperature %.1f C", data.temp_c);
                    break;
                case DISPLAY_METRIC_ORP:
                    ESP_LOGD(TAG, "Display: ORP %.0f mV", data.orp_mv);
                    break;
                case DISPLAY_METRIC_PH:
                    ESP_LOGD(TAG, "Display: pH %.2f", data.ph);
                    break;
                case DISPLAY_METRIC_EC:
                    ESP_LOGD(TAG, "Display: EC %.3f mS/cm", data.ec_ms_cm);
                    break;
                default:
                    break;
            }
        }

        // Cycle to next metric
        current_metric = (current_metric + 1) % DISPLAY_METRIC_COUNT;

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_CYCLE_INTERVAL_MS));
    }
}

/**
 * @brief System monitoring task
 *
 * Periodically logs heap usage and system stats for stability profiling.
 */
static void monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Monitor task started");

    // Track minimum free heap for leak detection
    uint32_t min_free_heap = UINT32_MAX;
    uint32_t initial_free_heap = esp_get_free_heap_size();

    while (1) {
        // Get current heap stats
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_ever = esp_get_minimum_free_heap_size();

        // Get internal SRAM heap (most constrained)
        uint32_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

        // Track minimum during this session
        if (free_heap < min_free_heap) {
            min_free_heap = free_heap;
        }

        // Calculate usage
        int32_t heap_change = (int32_t)initial_free_heap - (int32_t)free_heap;

        ESP_LOGI(TAG, "Heap: free=%lu, min_ever=%lu, session_min=%lu, change=%+ld",
                 (unsigned long)free_heap,
                 (unsigned long)min_ever,
                 (unsigned long)min_free_heap,
                 (long)heap_change);
        ESP_LOGI(TAG, "Internal: free=%lu, largest_block=%lu",
                 (unsigned long)free_internal,
                 (unsigned long)largest_block);

        // Warn if heap is getting low
        if (free_heap < 20000) {
            ESP_LOGW(TAG, "Low memory warning! Free heap: %lu bytes", (unsigned long)free_heap);
        }

        // Check for potential memory leak (heap trending down)
        if (heap_change > 50000) {
            ESP_LOGW(TAG, "Potential memory leak detected! Heap decreased by %ld bytes since boot",
                     (long)heap_change);
        }

        // Log every 60 seconds
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

/**
 * @brief Application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "Fishtank Controller - ESP-IDF");
    ESP_LOGI(TAG, "Target: ESP32-C6");
    ESP_LOGI(TAG, "==============================================");

    // Initialize NVS (required for WiFi and config storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize I2C bus (shared by POET sensor and OLED display)
    ESP_ERROR_CHECK(i2c_master_init());

    // Create synchronization primitives
    sensor_data_queue = xQueueCreate(1, sizeof(sensor_data_t));
    if (sensor_data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor data queue");
        return;
    }

    system_events = xEventGroupCreate();
    if (system_events == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Initialize Phase 2 components
    ESP_LOGI(TAG, "Initializing components...");

    ret = calibration_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Calibration init failed: %s (using defaults)", esp_err_to_name(ret));
    }

    ret = tank_settings_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Tank settings init failed: %s (using defaults)", esp_err_to_name(ret));
    }

    ret = warning_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Warning manager init failed: %s (using defaults)", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Phase 2 components initialized");

    // Initialize Phase 3 components (connectivity)
    ESP_LOGI(TAG, "Initializing connectivity...");

    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi manager init failed: %s", esp_err_to_name(ret));
    } else {
        // Check if WiFi connected (station mode) or AP mode started
        if (wifi_manager_is_connected()) {
            xEventGroupSetBits(system_events, WIFI_CONNECTED_BIT);
            ESP_LOGI(TAG, "WiFi connected in station mode");
        } else if (wifi_manager_is_ap_mode()) {
            ESP_LOGI(TAG, "WiFi in AP provisioning mode");
        }
    }

    ret = mqtt_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT manager init failed: %s (will retry in task)", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Phase 3 components initialized");

    // Initialize Phase 5 components
    ret = data_history_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Data history init failed: %s", esp_err_to_name(ret));
    }

    ret = ota_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA manager init failed: %s", esp_err_to_name(ret));
    } else {
        // Auto-confirm OTA update after successful boot
        // This prevents automatic rollback after 5 minutes
        if (ota_manager_is_pending_verify()) {
            ESP_LOGI(TAG, "OTA update pending verification, confirming...");
            ota_manager_confirm_update();
        }
    }

    ESP_LOGI(TAG, "Phase 5 components initialized");

    // Create FreeRTOS tasks
    ESP_LOGI(TAG, "Creating FreeRTOS tasks...");

    xTaskCreate(sensor_task, "sensor_task",
                SENSOR_TASK_STACK_SIZE, NULL, SENSOR_TASK_PRIORITY, NULL);

    xTaskCreate(http_task, "http_task",
                HTTP_TASK_STACK_SIZE, NULL, HTTP_TASK_PRIORITY, NULL);

    xTaskCreate(mqtt_task, "mqtt_task",
                MQTT_TASK_STACK_SIZE, NULL, MQTT_TASK_PRIORITY, NULL);

    xTaskCreate(display_task, "display_task",
                DISPLAY_TASK_STACK_SIZE, NULL, DISPLAY_TASK_PRIORITY, NULL);

    xTaskCreate(monitor_task, "monitor_task",
                MONITOR_TASK_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, NULL);

    // Log initial heap state
    ESP_LOGI(TAG, "Initial heap: free=%lu, min_ever=%lu",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)esp_get_minimum_free_heap_size());

    ESP_LOGI(TAG, "All tasks created, scheduler running");

    // app_main() can return - FreeRTOS scheduler continues
}
