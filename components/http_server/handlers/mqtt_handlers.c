/**
 * @file mqtt_handlers.c
 * @brief HTTP handlers for MQTT configuration API endpoints
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "mqtt_manager.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "mqtt_handlers";

static char* read_post_data(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 1024) return NULL;

    char *buf = malloc(total_len + 1);
    if (!buf) return NULL;

    int received = httpd_req_recv(req, buf, total_len);
    if (received != total_len) {
        free(buf);
        return NULL;
    }
    buf[total_len] = '\0';
    return buf;
}

// GET /api/mqtt/config
esp_err_t handle_mqtt_config_get(httpd_req_t *req)
{
    const mqtt_config_t *cfg = mqtt_manager_get_config();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "enabled", cfg->enabled);
    cJSON_AddStringToObject(root, "broker_host", cfg->broker_host);
    cJSON_AddNumberToObject(root, "broker_port", cfg->broker_port);
    cJSON_AddStringToObject(root, "username", cfg->username);
    cJSON_AddStringToObject(root, "device_id", cfg->device_id);
    cJSON_AddStringToObject(root, "chip_id", cfg->chip_id);
    cJSON_AddNumberToObject(root, "publish_interval_ms", cfg->publish_interval_ms);
    cJSON_AddBoolToObject(root, "discovery_enabled", cfg->discovery_enabled);
    cJSON_AddBoolToObject(root, "use_tls", cfg->use_tls);
    cJSON_AddBoolToObject(root, "has_ca_cert", mqtt_manager_has_ca_cert());

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/mqtt/config
esp_err_t handle_mqtt_config_post(httpd_req_t *req)
{
    char *buf = read_post_data(req);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    mqtt_config_t cfg;
    mqtt_manager_load_config(&cfg);

    cJSON *item;
    if ((item = cJSON_GetObjectItem(json, "enabled")) && cJSON_IsBool(item))
        cfg.enabled = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItem(json, "broker_host")) && cJSON_IsString(item))
        strncpy(cfg.broker_host, item->valuestring, sizeof(cfg.broker_host) - 1);
    if ((item = cJSON_GetObjectItem(json, "broker_port")) && cJSON_IsNumber(item))
        cfg.broker_port = (uint16_t)item->valueint;
    if ((item = cJSON_GetObjectItem(json, "username")) && cJSON_IsString(item))
        strncpy(cfg.username, item->valuestring, sizeof(cfg.username) - 1);
    if ((item = cJSON_GetObjectItem(json, "password")) && cJSON_IsString(item))
        strncpy(cfg.password, item->valuestring, sizeof(cfg.password) - 1);
    if ((item = cJSON_GetObjectItem(json, "device_id")) && cJSON_IsString(item))
        strncpy(cfg.device_id, item->valuestring, sizeof(cfg.device_id) - 1);
    if ((item = cJSON_GetObjectItem(json, "publish_interval_ms")) && cJSON_IsNumber(item))
        cfg.publish_interval_ms = (uint16_t)item->valueint;
    if ((item = cJSON_GetObjectItem(json, "discovery_enabled")) && cJSON_IsBool(item))
        cfg.discovery_enabled = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItem(json, "use_tls")) && cJSON_IsBool(item))
        cfg.use_tls = cJSON_IsTrue(item);

    cJSON_Delete(json);

    esp_err_t ret = mqtt_manager_save_config(&cfg);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
    }
    return ret;
}

// GET /api/mqtt/status
esp_err_t handle_mqtt_status_get(httpd_req_t *req)
{
    mqtt_state_t state = mqtt_manager_get_state();
    const char *state_str;

    switch (state) {
        case MQTT_STATE_CONNECTED: state_str = "connected"; break;
        case MQTT_STATE_CONNECTING: state_str = "connecting"; break;
        case MQTT_STATE_ERROR: state_str = "error"; break;
        default: state_str = "disconnected"; break;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", state_str);
    cJSON_AddBoolToObject(root, "connected", state == MQTT_STATE_CONNECTED);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}
