/**
 * @file mqtt_client.h
 * @brief MQTT Client with Home Assistant Discovery and TLS Support
 *
 * Publishes sensor data to MQTT broker with optional TLS encryption.
 * Supports Home Assistant MQTT Discovery for automatic entity creation.
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
#define MQTT_NVS_NAMESPACE          "mqtt"
#define MQTT_MAX_BROKER_LEN         128
#define MQTT_MAX_USERNAME_LEN       32
#define MQTT_MAX_PASSWORD_LEN       32
#define MQTT_MAX_DEVICE_ID_LEN      32
#define MQTT_CHIP_ID_LEN            7       // 6 hex chars + null
#define MQTT_DEFAULT_PORT           1883
#define MQTT_DEFAULT_PORT_TLS       8883
#define MQTT_BUFFER_SIZE            1024
#define MQTT_RECONNECT_TIMEOUT_MS   5000
#define MQTT_MAX_RECONNECT_MS       60000

// =============================================================================
// Configuration Structure (persisted to NVS)
// =============================================================================
typedef struct {
    bool enabled;
    char broker_host[MQTT_MAX_BROKER_LEN];
    uint16_t broker_port;
    char username[MQTT_MAX_USERNAME_LEN];
    char password[MQTT_MAX_PASSWORD_LEN];
    char device_id[MQTT_MAX_DEVICE_ID_LEN];
    char chip_id[MQTT_CHIP_ID_LEN];         // Read-only, derived from MAC
    uint16_t publish_interval_ms;
    bool discovery_enabled;                  // Home Assistant Discovery
    bool use_tls;                           // Use mqtts://
    bool verify_server;                     // Verify server certificate
} mqtt_config_t;

// =============================================================================
// Sensor Data Structure (matches main.c sensor_data_t)
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
} mqtt_sensor_data_t;

// =============================================================================
// Connection State
// =============================================================================
typedef enum {
    MQTT_STATE_DISCONNECTED,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR
} mqtt_state_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize MQTT manager
 *
 * Loads configuration from NVS and generates chip ID.
 * Does not connect automatically.
 *
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_init(void);

/**
 * @brief Connect to MQTT broker
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_connect(void);

/**
 * @brief Disconnect from MQTT broker
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_disconnect(void);

/**
 * @brief Check if connected to broker
 * @return true if connected
 */
bool mqtt_manager_is_connected(void);

/**
 * @brief Get current connection state
 * @return Current state
 */
mqtt_state_t mqtt_manager_get_state(void);

/**
 * @brief Publish sensor data to MQTT
 *
 * Publishes to individual topics and combined JSON topic:
 * - aquarium/<device>-<chip>/telemetry/temperature
 * - aquarium/<device>-<chip>/telemetry/orp
 * - aquarium/<device>-<chip>/telemetry/ph
 * - aquarium/<device>-<chip>/telemetry/ec
 * - aquarium/<device>-<chip>/telemetry/sensors (JSON)
 *
 * @param data Pointer to sensor data
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_publish_sensor_data(const mqtt_sensor_data_t *data);

/**
 * @brief Publish Home Assistant Discovery messages
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_publish_ha_discovery(void);

/**
 * @brief Save MQTT configuration to NVS
 * @param config Pointer to configuration
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_save_config(const mqtt_config_t *config);

/**
 * @brief Load MQTT configuration from NVS
 * @param config Pointer to configuration
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_load_config(mqtt_config_t *config);

/**
 * @brief Get current configuration
 * @return Pointer to current config (read-only)
 */
const mqtt_config_t* mqtt_manager_get_config(void);

/**
 * @brief Set CA certificate for TLS
 * @param cert PEM-encoded certificate string
 * @param len Certificate length
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_set_ca_cert(const char *cert, size_t len);

/**
 * @brief Clear stored CA certificate
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_clear_ca_cert(void);

/**
 * @brief Check if CA certificate is stored
 * @return true if certificate exists
 */
bool mqtt_manager_has_ca_cert(void);

/**
 * @brief Process MQTT events (call periodically)
 *
 * Handles reconnection with exponential backoff.
 */
void mqtt_manager_loop(void);

#ifdef __cplusplus
}
#endif
