/**
 * @file calibration_handlers.c
 * @brief HTTP handlers for calibration API endpoints
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "calibration.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

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

// GET /api/calibration/status
esp_err_t handle_calibration_status_get(httpd_req_t *req)
{
    calibration_status_t status;
    calibration_get_status(&status);

    ph_calibration_t ph_cal;
    ec_calibration_t ec_cal;
    calibration_get_ph(&ph_cal);
    calibration_get_ec(&ec_cal);

    cJSON *root = cJSON_CreateObject();

    cJSON *ph = cJSON_AddObjectToObject(root, "ph");
    cJSON_AddBoolToObject(ph, "calibrated", status.ph_calibrated);
    cJSON_AddBoolToObject(ph, "two_point", status.ph_two_point);
    cJSON_AddNumberToObject(ph, "sensitivity", ph_cal.sensitivity_mV_pH);
    cJSON_AddNumberToObject(ph, "offset", ph_cal.offset_mV);

    cJSON *ec = cJSON_AddObjectToObject(root, "ec");
    cJSON_AddBoolToObject(ec, "calibrated", status.ec_calibrated);
    cJSON_AddNumberToObject(ec, "cell_constant", ec_cal.cell_constant);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// GET /api/calibration/raw
esp_err_t handle_calibration_raw_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"raw\":{\"ugs_uV\":0,\"ec_nA\":0,\"ec_uV\":0}}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /api/calibration/ph/1point
esp_err_t handle_calibration_ph_1point(httpd_req_t *req)
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

    cJSON *ph_val = cJSON_GetObjectItem(json, "ph_value");
    cJSON *raw_uv = cJSON_GetObjectItem(json, "raw_uV");

    if (!cJSON_IsNumber(ph_val) || !cJSON_IsNumber(raw_uv)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ph_value or raw_uV");
        return ESP_FAIL;
    }

    esp_err_t ret = calibration_ph_1point((float)ph_val->valuedouble, (float)raw_uv->valuedouble);
    cJSON_Delete(json);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Calibration failed");
    }
    return ret;
}

// POST /api/calibration/ph/2point
esp_err_t handle_calibration_ph_2point(httpd_req_t *req)
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

    cJSON *ph_val = cJSON_GetObjectItem(json, "ph_value");
    cJSON *raw_uv = cJSON_GetObjectItem(json, "raw_uV");

    if (!cJSON_IsNumber(ph_val) || !cJSON_IsNumber(raw_uv)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing calibration values");
        return ESP_FAIL;
    }

    esp_err_t ret = calibration_ph_2point((float)ph_val->valuedouble, (float)raw_uv->valuedouble);
    cJSON_Delete(json);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Calibration failed");
    }
    return ret;
}

// POST /api/calibration/ec
esp_err_t handle_calibration_ec(httpd_req_t *req)
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

    cJSON *known_ec = cJSON_GetObjectItem(json, "known_ec_ms");
    cJSON *temp = cJSON_GetObjectItem(json, "temp_c");
    cJSON *raw_na = cJSON_GetObjectItem(json, "raw_nA");
    cJSON *raw_uv = cJSON_GetObjectItem(json, "raw_uV");

    if (!cJSON_IsNumber(known_ec) || !cJSON_IsNumber(raw_na) || !cJSON_IsNumber(raw_uv)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing calibration values");
        return ESP_FAIL;
    }

    float temp_c = cJSON_IsNumber(temp) ? (float)temp->valuedouble : 25.0f;

    esp_err_t ret = calibration_ec(
        (float)known_ec->valuedouble, temp_c,
        (float)raw_na->valuedouble, (float)raw_uv->valuedouble);
    cJSON_Delete(json);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Calibration failed");
    }
    return ret;
}

// POST /api/calibration/clear
esp_err_t handle_calibration_clear(httpd_req_t *req)
{
    esp_err_t ret = calibration_clear_all();

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Clear failed");
    }
    return ret;
}
