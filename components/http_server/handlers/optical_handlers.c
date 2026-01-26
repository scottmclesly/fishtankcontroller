/**
 * @file optical_handlers.c
 * @brief HTTP handlers for optical sensor API endpoints
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "optical_sensor.h"
#include "warning_manager.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "optical_handlers";

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

// GET /api/optical/status
esp_err_t handle_optical_status_get(httpd_req_t *req)
{
    optical_status_t status;
    optical_sensor_get_status(&status);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "tsl2591_present", status.tsl2591_present);
    cJSON_AddBoolToObject(root, "ws2812b_initialized", status.ws2812b_initialized);
    cJSON_AddBoolToObject(root, "calibrated", status.calibrated);
    cJSON_AddBoolToObject(root, "has_dirty_reference", status.has_dirty_reference);
    cJSON_AddBoolToObject(root, "ready", optical_sensor_is_ready());

    cJSON_AddNumberToObject(root, "last_ntu", status.last_ntu);
    cJSON_AddNumberToObject(root, "last_doc_index", status.last_doc_index);
    cJSON_AddNumberToObject(root, "ntu_warning", status.ntu_warning_state);
    cJSON_AddNumberToObject(root, "doc_warning", status.doc_warning_state);

    cJSON_AddNumberToObject(root, "measurement_count", status.measurement_count);
    cJSON_AddNumberToObject(root, "high_ambient_count", status.high_ambient_count);
    cJSON_AddNumberToObject(root, "last_measurement_time", (double)status.last_measurement_time);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// GET /api/optical/reading
esp_err_t handle_optical_reading_get(httpd_req_t *req)
{
    optical_status_t status;
    optical_sensor_get_status(&status);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "ntu", status.last_ntu_raw);
    cJSON_AddNumberToObject(root, "ntu_filtered", status.last_ntu);
    cJSON_AddNumberToObject(root, "doc_index", status.last_doc_raw);
    cJSON_AddNumberToObject(root, "doc_filtered", status.last_doc_index);

    cJSON_AddNumberToObject(root, "ntu_warning", status.ntu_warning_state);
    cJSON_AddNumberToObject(root, "doc_warning", status.doc_warning_state);

    cJSON_AddBoolToObject(root, "valid", status.last_ntu >= 0);
    cJSON_AddNumberToObject(root, "timestamp", (double)status.last_measurement_time);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/optical/measure
esp_err_t handle_optical_measure_post(httpd_req_t *req)
{
    if (!optical_sensor_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Optical sensor not ready");
        return ESP_FAIL;
    }

    optical_measurement_t result;
    optical_err_t ret = optical_sensor_measure(&result);

    cJSON *root = cJSON_CreateObject();

    if (ret == OPTICAL_OK) {
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddNumberToObject(root, "ntu", result.ntu);
        cJSON_AddNumberToObject(root, "ntu_filtered", optical_sensor_get_filtered_ntu());
        cJSON_AddNumberToObject(root, "doc_index", result.doc_index);
        cJSON_AddNumberToObject(root, "doc_filtered", optical_sensor_get_filtered_doc());

        cJSON *backscatter = cJSON_AddObjectToObject(root, "backscatter");
        cJSON_AddNumberToObject(backscatter, "green", result.backscatter_green);
        cJSON_AddNumberToObject(backscatter, "blue", result.backscatter_blue);
        cJSON_AddNumberToObject(backscatter, "red", result.backscatter_red);

        cJSON_AddNumberToObject(root, "timestamp", (double)result.timestamp);
    } else if (ret == OPTICAL_ERR_HIGH_AMBIENT) {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "high_ambient_light");
        cJSON_AddStringToObject(root, "message", "Measurement aborted due to high ambient light");
    } else {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "measurement_failed");
        cJSON_AddNumberToObject(root, "error_code", ret);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// GET /api/optical/calibration
esp_err_t handle_optical_calibration_get(httpd_req_t *req)
{
    optical_calibration_t cal;
    optical_sensor_get_calibration(&cal);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "calibrated", cal.calibrated);

    if (cal.calibrated) {
        cJSON *clear = cJSON_AddObjectToObject(root, "clear");
        cJSON_AddNumberToObject(clear, "green", cal.clear_green);
        cJSON_AddNumberToObject(clear, "blue", cal.clear_blue);
        cJSON_AddNumberToObject(clear, "red", cal.clear_red);
        cJSON_AddNumberToObject(clear, "ratio", cal.clear_ratio);
        cJSON_AddNumberToObject(clear, "timestamp", (double)cal.clear_timestamp);
    }

    cJSON_AddBoolToObject(root, "has_dirty_reference", cal.has_dirty_reference);

    if (cal.has_dirty_reference) {
        cJSON *dirty = cJSON_AddObjectToObject(root, "dirty");
        cJSON_AddNumberToObject(dirty, "green", cal.dirty_green);
        cJSON_AddNumberToObject(dirty, "ratio", cal.dirty_ratio);
        cJSON_AddNumberToObject(dirty, "ntu_reference", cal.dirty_ntu_reference);
        cJSON_AddNumberToObject(dirty, "timestamp", (double)cal.dirty_timestamp);
    }

    // Include thresholds
    optical_thresholds_t thresh;
    warning_manager_get_optical_thresholds(&thresh);

    cJSON *thresholds = cJSON_AddObjectToObject(root, "thresholds");
    cJSON_AddNumberToObject(thresholds, "ntu_warn", thresh.ntu_warn);
    cJSON_AddNumberToObject(thresholds, "ntu_crit", thresh.ntu_crit);
    cJSON_AddNumberToObject(thresholds, "doc_warn", thresh.doc_warn);
    cJSON_AddNumberToObject(thresholds, "doc_crit", thresh.doc_crit);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/optical/calibrate/clear
esp_err_t handle_optical_calibrate_clear_post(httpd_req_t *req)
{
    if (!optical_sensor_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Optical sensor not ready");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting clear water calibration...");

    optical_err_t ret = optical_sensor_calibrate_clear();

    cJSON *root = cJSON_CreateObject();

    if (ret == OPTICAL_OK) {
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "message", "Clear water calibration saved");

        // Return the calibration values
        optical_calibration_t cal;
        optical_sensor_get_calibration(&cal);
        cJSON_AddNumberToObject(root, "clear_green", cal.clear_green);
        cJSON_AddNumberToObject(root, "clear_blue", cal.clear_blue);
        cJSON_AddNumberToObject(root, "clear_red", cal.clear_red);
        cJSON_AddNumberToObject(root, "clear_ratio", cal.clear_ratio);
    } else if (ret == OPTICAL_ERR_HIGH_AMBIENT) {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "high_ambient_light");
        cJSON_AddStringToObject(root, "message", "Calibration failed: high ambient light");
    } else {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "calibration_failed");
        cJSON_AddNumberToObject(root, "error_code", ret);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/optical/calibrate/dirty
esp_err_t handle_optical_calibrate_dirty_post(httpd_req_t *req)
{
    if (!optical_sensor_is_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Optical sensor not ready");
        return ESP_FAIL;
    }

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

    cJSON *ntu_ref = cJSON_GetObjectItem(json, "ntu_reference");
    float ntu_reference = 25.0f;  // Default if not provided

    if (cJSON_IsNumber(ntu_ref)) {
        ntu_reference = (float)ntu_ref->valuedouble;
    }
    cJSON_Delete(json);

    ESP_LOGI(TAG, "Starting dirty water calibration with NTU reference: %.1f", ntu_reference);

    optical_err_t ret = optical_sensor_calibrate_dirty(ntu_reference);

    cJSON *root = cJSON_CreateObject();

    if (ret == OPTICAL_OK) {
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "message", "Dirty water calibration saved");

        optical_calibration_t cal;
        optical_sensor_get_calibration(&cal);
        cJSON_AddNumberToObject(root, "dirty_green", cal.dirty_green);
        cJSON_AddNumberToObject(root, "dirty_ratio", cal.dirty_ratio);
        cJSON_AddNumberToObject(root, "ntu_reference", cal.dirty_ntu_reference);
    } else if (ret == OPTICAL_ERR_HIGH_AMBIENT) {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "high_ambient_light");
    } else {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "calibration_failed");
        cJSON_AddNumberToObject(root, "error_code", ret);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// DELETE /api/optical/calibration
esp_err_t handle_optical_calibration_delete(httpd_req_t *req)
{
    optical_err_t ret = optical_sensor_clear_calibration();

    cJSON *root = cJSON_CreateObject();

    if (ret == OPTICAL_OK) {
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "message", "Calibration cleared");
    } else {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "clear_failed");
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/optical/thresholds
esp_err_t handle_optical_thresholds_post(httpd_req_t *req)
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

    optical_thresholds_t thresh;
    warning_manager_get_optical_thresholds(&thresh);

    cJSON *item;
    if ((item = cJSON_GetObjectItem(json, "ntu_warn")) && cJSON_IsNumber(item)) {
        thresh.ntu_warn = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(json, "ntu_crit")) && cJSON_IsNumber(item)) {
        thresh.ntu_crit = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(json, "doc_warn")) && cJSON_IsNumber(item)) {
        thresh.doc_warn = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(json, "doc_crit")) && cJSON_IsNumber(item)) {
        thresh.doc_crit = (float)item->valuedouble;
    }

    cJSON_Delete(json);

    esp_err_t ret = warning_manager_set_optical_thresholds(&thresh);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save thresholds");
    }
    return ret;
}
