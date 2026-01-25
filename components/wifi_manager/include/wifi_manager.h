/**
 * @file wifi_manager.h
 * @brief WiFi Connection and Provisioning Manager
 *
 * Handles WiFi station mode connection and AP provisioning mode.
 * Stores credentials in NVS for persistence across reboots.
 */

#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define WIFI_MANAGER_NVS_NAMESPACE  "wifi"
#define WIFI_MANAGER_MAX_SSID_LEN   32
#define WIFI_MANAGER_MAX_PASS_LEN   64
#define WIFI_MANAGER_MAX_RETRIES    3
#define WIFI_MANAGER_RETRY_DELAY_MS 10000

// Default AP credentials for provisioning
#define WIFI_MANAGER_AP_SSID        CONFIG_FISHTANK_WIFI_AP_SSID
#define WIFI_MANAGER_AP_PASS        CONFIG_FISHTANK_WIFI_AP_PASS

// =============================================================================
// State Enumeration
// =============================================================================
typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE,
    WIFI_STATE_ERROR
} wifi_state_t;

// =============================================================================
// Stored Credentials Structure
// =============================================================================
typedef struct {
    char ssid[WIFI_MANAGER_MAX_SSID_LEN + 1];
    char password[WIFI_MANAGER_MAX_PASS_LEN + 1];
    bool valid;
} wifi_credentials_t;

// =============================================================================
// Connection Info Structure
// =============================================================================
typedef struct {
    wifi_state_t state;
    char ip_addr[16];
    char ssid[WIFI_MANAGER_MAX_SSID_LEN + 1];
    int8_t rssi;
    uint8_t channel;
} wifi_info_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize WiFi manager
 *
 * Sets up event handlers and attempts connection with stored credentials.
 * Falls back to AP mode if no credentials or connection fails.
 *
 * @param event_group Event group to signal connection state
 * @param connected_bit Bit to set when connected
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Start WiFi connection with stored credentials
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no credentials
 */
esp_err_t wifi_manager_connect(void);

/**
 * @brief Start provisioning AP mode
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_start_ap(void);

/**
 * @brief Stop current WiFi mode
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_stop(void);

/**
 * @brief Save WiFi credentials to NVS
 * @param ssid Network SSID
 * @param password Network password
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);

/**
 * @brief Load WiFi credentials from NVS
 * @param creds Pointer to credentials structure
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_load_credentials(wifi_credentials_t *creds);

/**
 * @brief Clear stored WiFi credentials
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_clear_credentials(void);

/**
 * @brief Get current WiFi state
 * @return Current state
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief Get current connection info
 * @param info Pointer to info structure
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_info(wifi_info_t *info);

/**
 * @brief Check if WiFi is connected
 * @return true if connected
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Check if in AP mode
 * @return true if in AP mode
 */
bool wifi_manager_is_ap_mode(void);

/**
 * @brief Get WiFi event group handle
 * @return Event group handle (WIFI_CONNECTED_BIT = BIT0)
 */
void* wifi_manager_get_event_group(void);

#ifdef __cplusplus
}
#endif
