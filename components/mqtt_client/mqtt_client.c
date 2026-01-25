/**
 * @file mqtt_client.c
 * @brief MQTT Client with Home Assistant Discovery and TLS Support
 */

#include "mqtt_manager.h"
#include "mqtt_client.h"  // ESP-IDF MQTT client
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "mqtt_manager";

// ESP-IDF MQTT client handle
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static mqtt_config_t s_config = {0};
static mqtt_state_t s_state = MQTT_STATE_DISCONNECTED;
static char *s_ca_cert = NULL;
static size_t s_ca_cert_len = 0;

// Reconnection state
static int64_t s_last_reconnect_attempt = 0;
static uint32_t s_reconnect_delay_ms = MQTT_RECONNECT_TIMEOUT_MS;

// Topic buffer
static char s_topic_buf[128];
static char s_payload_buf[512];

// =============================================================================
// Helper Functions
// =============================================================================

static void generate_chip_id(char *chip_id)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(chip_id, MQTT_CHIP_ID_LEN, "%02X%02X%02X",
             mac[3], mac[4], mac[5]);
}

static void sanitize_device_id(char *out, const char *in, size_t max_len)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j < max_len - 1; i++) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') {
            out[j++] = c - 'A' + 'a';  // lowercase
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out[j++] = c;
        } else if (c == ' ' || c == '-') {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
}

static void build_topic(const char *suffix)
{
    char sanitized[MQTT_MAX_DEVICE_ID_LEN];
    sanitize_device_id(sanitized, s_config.device_id, sizeof(sanitized));
    snprintf(s_topic_buf, sizeof(s_topic_buf), "aquarium/%s-%s/%s",
             sanitized, s_config.chip_id, suffix);
}

// =============================================================================
// MQTT Event Handler
// =============================================================================

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to broker");
            s_state = MQTT_STATE_CONNECTED;
            s_reconnect_delay_ms = MQTT_RECONNECT_TIMEOUT_MS;

            // Publish HA Discovery on connect
            if (s_config.discovery_enabled) {
                mqtt_manager_publish_ha_discovery();
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from broker");
            s_state = MQTT_STATE_DISCONNECTED;
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Transport error: %s",
                         esp_err_to_name(event->error_handle->esp_transport_sock_errno));
            }
            s_state = MQTT_STATE_ERROR;
            break;

        default:
            break;
    }
}

// =============================================================================
// API Functions
// =============================================================================

esp_err_t mqtt_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT manager");

    // Generate chip ID from MAC
    generate_chip_id(s_config.chip_id);
    ESP_LOGI(TAG, "Chip ID: %s", s_config.chip_id);

    // Load config from NVS
    esp_err_t err = mqtt_manager_load_config(&s_config);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No stored config, using defaults");
        s_config.enabled = false;
        s_config.broker_port = MQTT_DEFAULT_PORT;
        s_config.publish_interval_ms = 5000;
        s_config.discovery_enabled = true;
        strcpy(s_config.device_id, "Aquarium");
    }

    return ESP_OK;
}

esp_err_t mqtt_manager_connect(void)
{
    if (!s_config.enabled || strlen(s_config.broker_host) == 0) {
        ESP_LOGW(TAG, "MQTT not configured");
        return ESP_ERR_INVALID_STATE;
    }

    // Build broker URI
    char uri[256];
    if (s_config.use_tls) {
        snprintf(uri, sizeof(uri), "mqtts://%s:%d",
                 s_config.broker_host, s_config.broker_port);
    } else {
        snprintf(uri, sizeof(uri), "mqtt://%s:%d",
                 s_config.broker_host, s_config.broker_port);
    }

    ESP_LOGI(TAG, "Connecting to %s", uri);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.username = s_config.username,
        .credentials.authentication.password = s_config.password,
        .buffer.size = MQTT_BUFFER_SIZE,
    };

    // Add TLS config if enabled
    if (s_config.use_tls && s_ca_cert && s_ca_cert_len > 0) {
        mqtt_cfg.broker.verification.certificate = s_ca_cert;
        mqtt_cfg.broker.verification.certificate_len = s_ca_cert_len;
    } else if (s_config.use_tls && !s_config.verify_server) {
        mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
    }

    // Create client if needed
    if (s_mqtt_client == NULL) {
        s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        if (s_mqtt_client == NULL) {
            ESP_LOGE(TAG, "Failed to create MQTT client");
            return ESP_FAIL;
        }
        esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                       mqtt_event_handler, NULL);
    }

    s_state = MQTT_STATE_CONNECTING;
    return esp_mqtt_client_start(s_mqtt_client);
}

esp_err_t mqtt_manager_disconnect(void)
{
    if (s_mqtt_client) {
        esp_mqtt_client_stop(s_mqtt_client);
    }
    s_state = MQTT_STATE_DISCONNECTED;
    return ESP_OK;
}

bool mqtt_manager_is_connected(void)
{
    return s_state == MQTT_STATE_CONNECTED;
}

mqtt_state_t mqtt_manager_get_state(void)
{
    return s_state;
}

esp_err_t mqtt_manager_publish_sensor_data(const mqtt_sensor_data_t *data)
{
    if (!data || s_state != MQTT_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    // Publish individual topics
    build_topic("telemetry/temperature");
    snprintf(s_payload_buf, sizeof(s_payload_buf), "%.2f", data->temp_c);
    esp_mqtt_client_publish(s_mqtt_client, s_topic_buf, s_payload_buf, 0, 0, 0);

    build_topic("telemetry/orp");
    snprintf(s_payload_buf, sizeof(s_payload_buf), "%.1f", data->orp_mv);
    esp_mqtt_client_publish(s_mqtt_client, s_topic_buf, s_payload_buf, 0, 0, 0);

    build_topic("telemetry/ph");
    snprintf(s_payload_buf, sizeof(s_payload_buf), "%.2f", data->ph);
    esp_mqtt_client_publish(s_mqtt_client, s_topic_buf, s_payload_buf, 0, 0, 0);

    build_topic("telemetry/ec");
    snprintf(s_payload_buf, sizeof(s_payload_buf), "%.3f", data->ec_ms_cm);
    esp_mqtt_client_publish(s_mqtt_client, s_topic_buf, s_payload_buf, 0, 0, 0);

    // Publish combined JSON
    build_topic("telemetry/sensors");
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "temperature_c", data->temp_c);
    cJSON_AddNumberToObject(json, "orp_mv", data->orp_mv);
    cJSON_AddNumberToObject(json, "ph", data->ph);
    cJSON_AddNumberToObject(json, "ec_ms_cm", data->ec_ms_cm);
    cJSON_AddNumberToObject(json, "tds_ppm", data->tds_ppm);
    cJSON_AddNumberToObject(json, "co2_ppm", data->co2_ppm);
    cJSON_AddNumberToObject(json, "nh3_ppm", data->nh3_ppm);
    cJSON_AddBoolToObject(json, "valid", data->valid);

    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str) {
        esp_mqtt_client_publish(s_mqtt_client, s_topic_buf, json_str, 0, 0, 0);
        free(json_str);
    }
    cJSON_Delete(json);

    ESP_LOGD(TAG, "Published sensor data");
    return ESP_OK;
}

esp_err_t mqtt_manager_publish_ha_discovery(void)
{
    if (s_state != MQTT_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    char sanitized[MQTT_MAX_DEVICE_ID_LEN];
    sanitize_device_id(sanitized, s_config.device_id, sizeof(sanitized));

    char unique_id[64];
    char state_topic[128];

    // Define sensors: name, unit, icon, device_class
    const struct {
        const char *name;
        const char *suffix;
        const char *unit;
        const char *device_class;
        const char *icon;
    } sensors[] = {
        {"Temperature", "temperature", "°C", "temperature", NULL},
        {"ORP", "orp", "mV", "voltage", "mdi:flash"},
        {"pH", "ph", "", NULL, "mdi:water"},
        {"EC", "ec", "mS/cm", NULL, "mdi:flash-circle"},
    };

    for (size_t i = 0; i < sizeof(sensors) / sizeof(sensors[0]); i++) {
        snprintf(unique_id, sizeof(unique_id), "%s_%s_%s",
                 sanitized, s_config.chip_id, sensors[i].suffix);

        snprintf(state_topic, sizeof(state_topic), "aquarium/%s-%s/telemetry/%s",
                 sanitized, s_config.chip_id, sensors[i].suffix);

        snprintf(s_topic_buf, sizeof(s_topic_buf),
                 "homeassistant/sensor/%s-%s/%s/config",
                 sanitized, s_config.chip_id, sensors[i].suffix);

        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "name", sensors[i].name);
        cJSON_AddStringToObject(json, "unique_id", unique_id);
        cJSON_AddStringToObject(json, "state_topic", state_topic);
        if (sensors[i].unit[0]) {
            cJSON_AddStringToObject(json, "unit_of_measurement", sensors[i].unit);
        }
        if (sensors[i].device_class) {
            cJSON_AddStringToObject(json, "device_class", sensors[i].device_class);
        }
        if (sensors[i].icon) {
            cJSON_AddStringToObject(json, "icon", sensors[i].icon);
        }

        // Device info
        cJSON *device = cJSON_AddObjectToObject(json, "device");
        cJSON *identifiers = cJSON_AddArrayToObject(device, "identifiers");
        char dev_id[64];
        snprintf(dev_id, sizeof(dev_id), "fishtank_%s_%s", sanitized, s_config.chip_id);
        cJSON_AddItemToArray(identifiers, cJSON_CreateString(dev_id));
        cJSON_AddStringToObject(device, "name", s_config.device_id);
        cJSON_AddStringToObject(device, "manufacturer", "DIY");
        cJSON_AddStringToObject(device, "model", "Fishtank Controller");

        char *json_str = cJSON_PrintUnformatted(json);
        if (json_str) {
            esp_mqtt_client_publish(s_mqtt_client, s_topic_buf, json_str, 0, 1, 1);
            free(json_str);
        }
        cJSON_Delete(json);
    }

    ESP_LOGI(TAG, "Published HA Discovery messages");
    return ESP_OK;
}

esp_err_t mqtt_manager_save_config(const mqtt_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(MQTT_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    // Save as blob (preserve chip_id from current config)
    mqtt_config_t save_cfg = *config;
    strncpy(save_cfg.chip_id, s_config.chip_id, sizeof(save_cfg.chip_id));

    err = nvs_set_blob(handle, "config", &save_cfg, sizeof(save_cfg));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        memcpy(&s_config, &save_cfg, sizeof(s_config));
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Config saved");
    return err;
}

esp_err_t mqtt_manager_load_config(mqtt_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(MQTT_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t size = sizeof(mqtt_config_t);
    err = nvs_get_blob(handle, "config", config, &size);
    nvs_close(handle);

    // Restore chip_id (it's derived, not stored)
    generate_chip_id(config->chip_id);

    return err;
}

const mqtt_config_t* mqtt_manager_get_config(void)
{
    return &s_config;
}

esp_err_t mqtt_manager_set_ca_cert(const char *cert, size_t len)
{
    if (s_ca_cert) {
        free(s_ca_cert);
    }
    s_ca_cert = malloc(len + 1);
    if (!s_ca_cert) return ESP_ERR_NO_MEM;

    memcpy(s_ca_cert, cert, len);
    s_ca_cert[len] = '\0';
    s_ca_cert_len = len;

    // Save to NVS
    nvs_handle_t handle;
    esp_err_t err = nvs_open(MQTT_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_blob(handle, "ca_cert", cert, len);
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "CA certificate set (%d bytes)", (int)len);
    return ESP_OK;
}

esp_err_t mqtt_manager_clear_ca_cert(void)
{
    if (s_ca_cert) {
        free(s_ca_cert);
        s_ca_cert = NULL;
        s_ca_cert_len = 0;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(MQTT_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_key(handle, "ca_cert");
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "CA certificate cleared");
    return ESP_OK;
}

bool mqtt_manager_has_ca_cert(void)
{
    return s_ca_cert != NULL && s_ca_cert_len > 0;
}

void mqtt_manager_loop(void)
{
    if (!s_config.enabled) return;

    // Handle reconnection with exponential backoff
    if (s_state == MQTT_STATE_DISCONNECTED || s_state == MQTT_STATE_ERROR) {
        int64_t now = esp_timer_get_time() / 1000;
        if (now - s_last_reconnect_attempt >= s_reconnect_delay_ms) {
            s_last_reconnect_attempt = now;
            ESP_LOGI(TAG, "Attempting reconnection...");

            if (mqtt_manager_connect() != ESP_OK) {
                // Exponential backoff
                s_reconnect_delay_ms = s_reconnect_delay_ms * 2;
                if (s_reconnect_delay_ms > MQTT_MAX_RECONNECT_MS) {
                    s_reconnect_delay_ms = MQTT_MAX_RECONNECT_MS;
                }
            }
        }
    }
}
