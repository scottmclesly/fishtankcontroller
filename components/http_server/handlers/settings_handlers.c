/**
 * @file settings_handlers.c
 * @brief HTTP handlers for tank settings API endpoints
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "tank_settings.h"
#include "warning_manager.h"
#include "wifi_manager.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "settings_handlers";

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

// GET /api/settings/tank
esp_err_t handle_settings_tank_get(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "volume_liters", tank_settings_get_volume());
    cJSON_AddNumberToObject(root, "kh_dkh", tank_settings_get_kh());
    cJSON_AddNumberToObject(root, "tan_ppm", tank_settings_get_tan());
    cJSON_AddNumberToObject(root, "tds_factor", tank_settings_get_tds_factor());
    cJSON_AddNumberToObject(root, "fish_length_cm", tank_settings_get_total_fish_length());

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/settings/tank
esp_err_t handle_settings_tank_post(httpd_req_t *req)
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

    // Get current settings, modify, then save
    tank_settings_t settings;
    tank_settings_get(&settings);

    cJSON *item;
    if ((item = cJSON_GetObjectItem(json, "volume_liters")) && cJSON_IsNumber(item))
        settings.manual_volume_liters = (float)item->valuedouble;
    if ((item = cJSON_GetObjectItem(json, "kh_dkh")) && cJSON_IsNumber(item))
        settings.manual_kh_dkh = (float)item->valuedouble;
    if ((item = cJSON_GetObjectItem(json, "tan_ppm")) && cJSON_IsNumber(item))
        settings.manual_tan_ppm = (float)item->valuedouble;
    if ((item = cJSON_GetObjectItem(json, "tds_factor")) && cJSON_IsNumber(item))
        settings.tds_conversion_factor = (float)item->valuedouble;

    cJSON_Delete(json);

    esp_err_t ret = tank_settings_save(&settings);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
        return ret;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// GET /api/settings/warnings
esp_err_t handle_settings_warnings_get(httpd_req_t *req)
{
    warning_thresholds_t thresh;
    warning_manager_get_thresholds(&thresh);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "tank_type", thresh.tank_type);

    cJSON *temp = cJSON_AddObjectToObject(root, "temperature");
    cJSON_AddNumberToObject(temp, "warn_low", thresh.temperature.warn_low);
    cJSON_AddNumberToObject(temp, "warn_high", thresh.temperature.warn_high);
    cJSON_AddNumberToObject(temp, "crit_low", thresh.temperature.crit_low);
    cJSON_AddNumberToObject(temp, "crit_high", thresh.temperature.crit_high);

    cJSON *ph = cJSON_AddObjectToObject(root, "ph");
    cJSON_AddNumberToObject(ph, "warn_low", thresh.ph.warn_low);
    cJSON_AddNumberToObject(ph, "warn_high", thresh.ph.warn_high);
    cJSON_AddNumberToObject(ph, "crit_low", thresh.ph.crit_low);
    cJSON_AddNumberToObject(ph, "crit_high", thresh.ph.crit_high);

    cJSON *nh3 = cJSON_AddObjectToObject(root, "nh3");
    cJSON_AddNumberToObject(nh3, "warn_high", thresh.nh3.warn_high);
    cJSON_AddNumberToObject(nh3, "crit_high", thresh.nh3.crit_high);

    cJSON *orp = cJSON_AddObjectToObject(root, "orp");
    cJSON_AddNumberToObject(orp, "warn_low", thresh.orp.warn_low);
    cJSON_AddNumberToObject(orp, "warn_high", thresh.orp.warn_high);
    cJSON_AddNumberToObject(orp, "crit_low", thresh.orp.crit_low);
    cJSON_AddNumberToObject(orp, "crit_high", thresh.orp.crit_high);

    cJSON *ec = cJSON_AddObjectToObject(root, "ec");
    cJSON_AddNumberToObject(ec, "warn_low", thresh.ec.warn_low);
    cJSON_AddNumberToObject(ec, "warn_high", thresh.ec.warn_high);
    cJSON_AddNumberToObject(ec, "crit_low", thresh.ec.crit_low);
    cJSON_AddNumberToObject(ec, "crit_high", thresh.ec.crit_high);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/settings/warnings
esp_err_t handle_settings_warnings_post(httpd_req_t *req)
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

    // Check for tank_type preset
    cJSON *tank_type = cJSON_GetObjectItem(json, "tank_type");
    if (tank_type && cJSON_IsNumber(tank_type)) {
        warning_manager_set_tank_type((tank_type_t)tank_type->valueint);
    }

    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// GET /api/wifi/config
esp_err_t handle_wifi_config_get(httpd_req_t *req)
{
    wifi_info_t info;
    wifi_manager_get_info(&info);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", info.ssid);
    cJSON_AddStringToObject(root, "ip", info.ip_addr);
    cJSON_AddNumberToObject(root, "rssi", info.rssi);
    cJSON_AddBoolToObject(root, "connected", wifi_manager_is_connected());
    cJSON_AddBoolToObject(root, "ap_mode", wifi_manager_is_ap_mode());

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/wifi/config
esp_err_t handle_wifi_config_post(httpd_req_t *req)
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

    cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
    cJSON *password = cJSON_GetObjectItem(json, "password");

    if (!cJSON_IsString(ssid)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }

    const char *pass = cJSON_IsString(password) ? password->valuestring : "";
    esp_err_t ret = wifi_manager_save_credentials(ssid->valuestring, pass);
    cJSON_Delete(json);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true,\"message\":\"Credentials saved. Reboot to connect.\"}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
    }
    return ret;
}

// GET /api/wifi/scan
esp_err_t handle_wifi_scan_get(httpd_req_t *req)
{
    // Simple scan placeholder - full implementation needs async scan
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"networks\":[]}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
