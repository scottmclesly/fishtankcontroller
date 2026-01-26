/**
 * @file http_server.h
 * @brief Async HTTP Server with REST API and WebSocket Support
 *
 * Serves web UI, REST API endpoints, and WebSocket for live sensor updates.
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define HTTP_SERVER_PORT            80
#define HTTP_SERVER_MAX_CLIENTS     4
#define HTTP_SERVER_MAX_URI_LEN     512
#define HTTP_SERVER_STACK_SIZE      8192

// =============================================================================
// Server Handle Structure
// =============================================================================
typedef struct {
    httpd_handle_t handle;
    bool running;
    uint32_t request_count;
} http_server_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Start HTTP server
 * @param server Pointer to server structure
 * @return ESP_OK on success
 */
esp_err_t http_server_start(http_server_t *server);

/**
 * @brief Stop HTTP server
 * @param server Pointer to server structure
 * @return ESP_OK on success
 */
esp_err_t http_server_stop(http_server_t *server);

/**
 * @brief Check if server is running
 * @param server Pointer to server structure
 * @return true if running
 */
bool http_server_is_running(http_server_t *server);

// =============================================================================
// Route Registration Functions
// =============================================================================

/**
 * @brief Register sensor data API routes
 * - GET /api/sensors - Current sensor readings
 * - GET /api/history - Historical data
 */
void http_server_register_sensor_routes(httpd_handle_t server);

/**
 * @brief Register calibration API routes
 * - GET /api/calibration/status
 * - POST /api/calibration/ph/1point
 * - POST /api/calibration/ph/2point
 * - POST /api/calibration/ec
 * - POST /api/calibration/clear
 */
void http_server_register_calibration_routes(httpd_handle_t server);

/**
 * @brief Register MQTT configuration API routes
 * - GET /api/mqtt/config
 * - POST /api/mqtt/config
 * - GET /api/mqtt/status
 * - POST /api/mqtt/ca_cert
 * - DELETE /api/mqtt/ca_cert
 */
void http_server_register_mqtt_routes(httpd_handle_t server);

/**
 * @brief Register tank settings API routes
 * - GET /api/settings/tank
 * - POST /api/settings/tank
 * - GET /api/settings/fish
 * - POST /api/settings/fish
 */
void http_server_register_settings_routes(httpd_handle_t server);

/**
 * @brief Register OTA update API routes
 * - GET /api/ota/status
 * - POST /api/ota/update
 * - POST /api/ota/upload
 * - POST /api/ota/confirm
 * - POST /api/ota/rollback
 */
void http_server_register_ota_routes(httpd_handle_t server);

/**
 * @brief Register optical sensor API routes
 * - GET /api/optical/status
 * - GET /api/optical/reading
 * - POST /api/optical/measure
 * - GET /api/optical/calibration
 * - POST /api/optical/calibrate/clear
 * - POST /api/optical/calibrate/dirty
 * - DELETE /api/optical/calibration
 * - POST /api/optical/thresholds
 */
void http_server_register_optical_routes(httpd_handle_t server);

/**
 * @brief Register static file routes (HTML, CSS, JS from LittleFS)
 * - GET / - Dashboard
 * - GET /calibration - Calibration page
 * - GET /charts - Charts page
 * - GET /css/(file) - Stylesheets
 * - GET /js/(file) - JavaScript files
 */
void http_server_register_static_routes(httpd_handle_t server);

/**
 * @brief Register WebSocket endpoint for live updates
 * - WS /ws - Live sensor data stream
 */
void http_server_register_websocket(httpd_handle_t server);

// =============================================================================
// WebSocket Functions
// =============================================================================

/**
 * @brief Broadcast sensor data to all connected WebSocket clients
 * @param json_data JSON string of sensor data
 * @return Number of clients notified
 */
int http_server_ws_broadcast(const char *json_data);

/**
 * @brief Get number of connected WebSocket clients
 * @return Number of clients
 */
int http_server_ws_get_client_count(void);

/**
 * @brief Update shared sensor data (called from sensor task)
 */
void http_server_update_sensor_data(float temp_c, float orp_mv, float ph, float ec_ms_cm,
                                     float tds_ppm, float co2_ppm, float nh3_ppm, float max_do_mg_l,
                                     uint8_t temp_w, uint8_t ph_w, uint8_t orp_w, uint8_t ec_w,
                                     uint8_t nh3_w, uint8_t do_w, bool valid);

/**
 * @brief Broadcast sensor data to all WebSocket clients
 */
void http_server_broadcast_sensor_data(void);

/**
 * @brief Update optical sensor data (called from sensor task)
 */
void http_server_update_optical_data(float ntu, float doc_index,
                                      uint8_t ntu_warning, uint8_t doc_warning,
                                      bool valid);

#ifdef __cplusplus
}
#endif
