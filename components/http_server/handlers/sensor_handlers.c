/**
 * @file sensor_handlers.c
 * @brief HTTP handlers for sensor API endpoints
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "sensor_handlers";

// External function from http_server.c
extern void http_server_get_sensor_data(float *temp_c, float *orp_mv, float *ph, float *ec_ms_cm,
                                         float *tds_ppm, float *co2_ppm, float *nh3_ppm, float *max_do_mg_l,
                                         uint8_t *warnings, bool *valid);

// GET /api/sensors
esp_err_t handle_sensors_get(httpd_req_t *req)
{
    float temp_c, orp_mv, ph, ec_ms_cm, tds_ppm, co2_ppm, nh3_ppm, max_do_mg_l;
    uint8_t warnings[6];
    bool valid;

    http_server_get_sensor_data(&temp_c, &orp_mv, &ph, &ec_ms_cm,
                                 &tds_ppm, &co2_ppm, &nh3_ppm, &max_do_mg_l,
                                 warnings, &valid);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temp_c", temp_c);
    cJSON_AddNumberToObject(root, "orp_mv", orp_mv);
    cJSON_AddNumberToObject(root, "ph", ph);
    cJSON_AddNumberToObject(root, "ec_ms_cm", ec_ms_cm);
    cJSON_AddNumberToObject(root, "tds_ppm", tds_ppm);
    cJSON_AddNumberToObject(root, "co2_ppm", co2_ppm);
    cJSON_AddNumberToObject(root, "nh3_ppm", nh3_ppm);
    cJSON_AddNumberToObject(root, "max_do_mg_l", max_do_mg_l);
    cJSON_AddBoolToObject(root, "valid", valid);

    cJSON *warn = cJSON_AddObjectToObject(root, "warnings");
    cJSON_AddNumberToObject(warn, "temp", warnings[0]);
    cJSON_AddNumberToObject(warn, "ph", warnings[1]);
    cJSON_AddNumberToObject(warn, "orp", warnings[2]);
    cJSON_AddNumberToObject(warn, "ec", warnings[3]);
    cJSON_AddNumberToObject(warn, "nh3", warnings[4]);
    cJSON_AddNumberToObject(warn, "do", warnings[5]);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// GET /api/history
esp_err_t handle_history_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"history\":[],\"count\":0}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
