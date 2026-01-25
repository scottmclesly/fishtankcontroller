/**
 * @file wifi_manager.c
 * @brief WiFi Connection and Provisioning Manager
 *
 * Handles WiFi station mode connection and AP provisioning mode.
 * Includes mDNS (aquarium.local) and SNTP time sync.
 */

#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mdns.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "wifi_manager";

// Event group for WiFi events
static EventGroupHandle_t s_wifi_event_group = NULL;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// State
static wifi_state_t s_wifi_state = WIFI_STATE_DISCONNECTED;
static wifi_info_t s_wifi_info = {0};
static int s_retry_count = 0;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

// Forward declarations
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);
static void start_mdns(void);
static void start_sntp(void);

// =============================================================================
// Event Handlers
// =============================================================================

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, connecting...");
        s_wifi_state = WIFI_STATE_CONNECTING;
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "Disconnected from AP, reason=%d", event->reason);

        if (s_retry_count < WIFI_MANAGER_MAX_RETRIES) {
            s_retry_count++;
            ESP_LOGI(TAG, "Retrying connection (%d/%d)...", s_retry_count, WIFI_MANAGER_MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(WIFI_MANAGER_RETRY_DELAY_MS));
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "Max retries reached, switching to AP mode");
            s_wifi_state = WIFI_STATE_DISCONNECTED;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "WiFi AP started");
        s_wifi_state = WIFI_STATE_AP_MODE;
    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Station connected to AP");
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "Station disconnected from AP");
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_wifi_info.ip_addr, sizeof(s_wifi_info.ip_addr),
                 IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", s_wifi_info.ip_addr);

        s_retry_count = 0;
        s_wifi_state = WIFI_STATE_CONNECTED;
        s_wifi_info.state = WIFI_STATE_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        // Start mDNS and SNTP after getting IP
        start_mdns();
        start_sntp();
    }
}

// =============================================================================
// mDNS and SNTP
// =============================================================================

static void start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }

    mdns_hostname_set("aquarium");
    mdns_instance_name_set("Aquarium Controller");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "mDNS started: aquarium.local");
}

static void start_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

// =============================================================================
// API Functions
// =============================================================================

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi manager");

    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default network interfaces
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &ip_event_handler, NULL));

    // Try to connect with stored credentials
    wifi_credentials_t creds;
    esp_err_t err = wifi_manager_load_credentials(&creds);
    if (err == ESP_OK && creds.valid) {
        ESP_LOGI(TAG, "Found stored credentials for SSID: %s", creds.ssid);
        err = wifi_manager_connect();
        if (err == ESP_OK) {
            // Wait for connection or failure
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));

            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "Connected to WiFi");
                return ESP_OK;
            }
        }
    }

    // Fall back to AP mode
    ESP_LOGI(TAG, "Starting provisioning AP mode");
    return wifi_manager_start_ap();
}

esp_err_t wifi_manager_connect(void)
{
    wifi_credentials_t creds;
    esp_err_t err = wifi_manager_load_credentials(&creds);
    if (err != ESP_OK || !creds.valid) {
        ESP_LOGW(TAG, "No valid credentials found");
        return ESP_ERR_NOT_FOUND;
    }

    // Configure station mode
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, creds.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, creds.password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    strncpy(s_wifi_info.ssid, creds.ssid, sizeof(s_wifi_info.ssid) - 1);

    ESP_LOGI(TAG, "Connecting to SSID: %s", creds.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t wifi_manager_start_ap(void)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_MANAGER_AP_SSID,
            .ssid_len = strlen(WIFI_MANAGER_AP_SSID),
            .channel = 1,
            .password = WIFI_MANAGER_AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(WIFI_MANAGER_AP_PASS) < 8) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    strncpy(s_wifi_info.ssid, WIFI_MANAGER_AP_SSID, sizeof(s_wifi_info.ssid) - 1);
    strcpy(s_wifi_info.ip_addr, "192.168.4.1");

    ESP_LOGI(TAG, "Starting AP mode - SSID: %s", WIFI_MANAGER_AP_SSID);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_wifi_state = WIFI_STATE_AP_MODE;
    s_wifi_info.state = WIFI_STATE_AP_MODE;

    // Start mDNS in AP mode too
    start_mdns();

    return ESP_OK;
}

esp_err_t wifi_manager_stop(void)
{
    esp_wifi_stop();
    s_wifi_state = WIFI_STATE_DISCONNECTED;
    return ESP_OK;
}

esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, "ssid", ssid);
    if (err == ESP_OK && password) {
        err = nvs_set_str(handle, "password", password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Credentials saved for SSID: %s", ssid);
    }
    return err;
}

esp_err_t wifi_manager_load_credentials(wifi_credentials_t *creds)
{
    if (!creds) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(creds, 0, sizeof(*creds));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t ssid_len = sizeof(creds->ssid);
    size_t pass_len = sizeof(creds->password);

    err = nvs_get_str(handle, "ssid", creds->ssid, &ssid_len);
    if (err == ESP_OK) {
        nvs_get_str(handle, "password", creds->password, &pass_len);
        creds->valid = strlen(creds->ssid) > 0;
    }

    nvs_close(handle);
    return creds->valid ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t wifi_manager_clear_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
    ESP_LOGI(TAG, "Credentials cleared");
    return ESP_OK;
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_wifi_state;
}

esp_err_t wifi_manager_get_info(wifi_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    *info = s_wifi_info;
    info->state = s_wifi_state;

    // Update RSSI if connected
    if (s_wifi_state == WIFI_STATE_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            info->rssi = ap_info.rssi;
            info->channel = ap_info.primary;
        }
    }

    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_wifi_state == WIFI_STATE_CONNECTED;
}

bool wifi_manager_is_ap_mode(void)
{
    return s_wifi_state == WIFI_STATE_AP_MODE;
}

void* wifi_manager_get_event_group(void)
{
    return (void*)s_wifi_event_group;
}
