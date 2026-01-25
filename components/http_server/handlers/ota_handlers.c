/**
 * @file ota_handlers.c
 * @brief HTTP handlers for OTA update API endpoints
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "ota_manager.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ota_handlers";

// State string helper
static const char* ota_state_to_string(ota_state_t state)
{
    switch (state) {
        case OTA_STATE_IDLE: return "idle";
        case OTA_STATE_DOWNLOADING: return "downloading";
        case OTA_STATE_VERIFYING: return "verifying";
        case OTA_STATE_READY_TO_REBOOT: return "ready_to_reboot";
        case OTA_STATE_PENDING_VERIFY: return "pending_verify";
        case OTA_STATE_ERROR: return "error";
        default: return "unknown";
    }
}

// GET /api/ota/status
esp_err_t handle_ota_status_get(httpd_req_t *req)
{
    const esp_app_desc_t *app = esp_app_get_description();
    ota_status_t status;
    ota_manager_get_status(&status);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", app->version);
    cJSON_AddStringToObject(root, "project", app->project_name);
    cJSON_AddStringToObject(root, "idf_version", app->idf_ver);
    cJSON_AddStringToObject(root, "compile_date", app->date);
    cJSON_AddStringToObject(root, "compile_time", app->time);
    cJSON_AddStringToObject(root, "status", ota_state_to_string(status.state));
    cJSON_AddNumberToObject(root, "progress", status.progress_percent);
    cJSON_AddNumberToObject(root, "bytes_written", status.bytes_written);
    cJSON_AddNumberToObject(root, "total_bytes", status.total_bytes);
    cJSON_AddBoolToObject(root, "can_rollback", status.can_rollback);

    if (status.state == OTA_STATE_PENDING_VERIFY) {
        cJSON_AddNumberToObject(root, "rollback_remaining", ota_manager_get_rollback_remaining());
    }

    if (status.error_message[0] != '\0') {
        cJSON_AddStringToObject(root, "error", status.error_message);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/ota/update - Start OTA from URL
esp_err_t handle_ota_update_post(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    char *buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    int received = httpd_req_recv(req, buf, total_len);
    if (received != total_len) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Receive failed");
        return ESP_FAIL;
    }
    buf[total_len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *url = cJSON_GetObjectItem(json, "url");
    if (!cJSON_IsString(url)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing URL");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting OTA update from: %s", url->valuestring);
    esp_err_t ret = ota_manager_start_update(url->valuestring, NULL, NULL);
    cJSON_Delete(json);

    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start OTA");
        return ret;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true,\"message\":\"OTA update started\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /api/ota/upload - Direct binary upload
esp_err_t handle_ota_upload_post(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting OTA upload, size: %d", total_len);

    // Begin OTA session
    esp_err_t ret = ota_manager_begin_upload(total_len);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to begin OTA");
        return ret;
    }

    // Receive and write chunks
    char *buf = malloc(4096);
    if (!buf) {
        ota_manager_abort();
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    int remaining = total_len;
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, (remaining > 4096) ? 4096 : remaining);
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Receive error");
            free(buf);
            ota_manager_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }

        ret = ota_manager_write_chunk((uint8_t *)buf, recv_len);
        if (ret != ESP_OK) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            return ret;
        }

        remaining -= recv_len;
    }

    free(buf);

    // Finalize
    ret = ota_manager_end_upload();
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Verification failed");
        return ret;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true,\"message\":\"OTA upload complete. Reboot to apply.\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /api/ota/confirm - Confirm update (prevent rollback)
esp_err_t handle_ota_confirm_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Confirming OTA update");

    esp_err_t ret = ota_manager_confirm_update();
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Confirm failed");
        return ret;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true,\"message\":\"Update confirmed\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /api/ota/rollback - Rollback to previous firmware
esp_err_t handle_ota_rollback_post(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Rolling back firmware");

    ota_status_t status;
    ota_manager_get_status(&status);

    if (!status.can_rollback) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No rollback available");
        return ESP_FAIL;
    }

    // Send response before rollback (since it won't return)
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true,\"message\":\"Rolling back...\"}", HTTPD_RESP_USE_STRLEN);

    // Small delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(100));

    ota_manager_rollback();
    // This doesn't return if successful
    return ESP_OK;
}

// POST /api/ota/reboot - Reboot to apply update
esp_err_t handle_ota_reboot_post(httpd_req_t *req)
{
    ota_status_t status;
    ota_manager_get_status(&status);

    if (status.state != OTA_STATE_READY_TO_REBOOT) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not ready to reboot");
        return ESP_FAIL;
    }

    // Send response before reboot
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true,\"message\":\"Rebooting...\"}", HTTPD_RESP_USE_STRLEN);

    // Small delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(100));

    ota_manager_reboot();
    // This doesn't return
    return ESP_OK;
}
