/**
 * @file ota_manager.c
 * @brief OTA (Over-The-Air) Update Manager
 *
 * Handles firmware updates via HTTP download or direct upload with rollback protection.
 */

#include "ota_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "ota_manager";

// Firmware version - defined at compile time or here
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.1.0-espidf"
#endif

// State
static ota_status_t s_status = {
    .state = OTA_STATE_IDLE,
    .progress_percent = 0
};

static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_update_partition = NULL;
static TimerHandle_t s_rollback_timer = NULL;
static uint32_t s_rollback_remaining = 0;
static ota_progress_cb_t s_progress_cb = NULL;
static void *s_progress_user_data = NULL;

// Forward declarations
static void rollback_timer_callback(TimerHandle_t xTimer);
static void ota_download_task(void *pvParameters);

esp_err_t ota_manager_init(void)
{
    ESP_LOGI(TAG, "OTA manager init");

    strncpy(s_status.current_version, FIRMWARE_VERSION, OTA_MAX_VERSION_LEN - 1);

    // Check if we're running from OTA partition and need to confirm
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    ESP_LOGI(TAG, "Running partition: %s @ 0x%lx",
             running->label, (unsigned long)running->address);

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "OTA update pending verification - will auto-rollback in %d seconds",
                     OTA_ROLLBACK_TIMEOUT_S);
            s_status.state = OTA_STATE_PENDING_VERIFY;

            // Create rollback timer
            s_rollback_remaining = OTA_ROLLBACK_TIMEOUT_S;
            s_rollback_timer = xTimerCreate("ota_rollback",
                                            pdMS_TO_TICKS(1000),
                                            pdTRUE,
                                            NULL,
                                            rollback_timer_callback);
            if (s_rollback_timer) {
                xTimerStart(s_rollback_timer, 0);
            }
        }
    }

    // Check if we can rollback
    const esp_partition_t *other = esp_ota_get_next_update_partition(running);
    s_status.can_rollback = (other != NULL);

    return ESP_OK;
}

static void rollback_timer_callback(TimerHandle_t xTimer)
{
    if (s_rollback_remaining > 0) {
        s_rollback_remaining--;
        if (s_rollback_remaining == 0) {
            ESP_LOGW(TAG, "Rollback timeout - reverting to previous firmware");
            esp_ota_mark_app_invalid_rollback_and_reboot();
        } else if (s_rollback_remaining % 60 == 0) {
            ESP_LOGW(TAG, "Rollback in %lu seconds - call confirm to prevent",
                     (unsigned long)s_rollback_remaining);
        }
    }
}

esp_err_t ota_manager_get_status(ota_status_t *status)
{
    if (status) {
        *status = s_status;
    }
    return ESP_OK;
}

// OTA download task
static char s_ota_url[OTA_MAX_URL_LEN] = {0};

static void ota_download_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting OTA download from: %s", s_ota_url);

    s_status.state = OTA_STATE_DOWNLOADING;
    s_status.progress_percent = 0;
    s_status.bytes_written = 0;

    esp_http_client_config_t config = {
        .url = s_ota_url,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        snprintf(s_status.error_message, sizeof(s_status.error_message),
                 "Begin failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }

    // Get image size if available
    int image_size = esp_https_ota_get_image_size(https_ota_handle);
    s_status.total_bytes = (image_size > 0) ? image_size : 0;

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        s_status.bytes_written = esp_https_ota_get_image_len_read(https_ota_handle);
        if (s_status.total_bytes > 0) {
            s_status.progress_percent = (s_status.bytes_written * 100) / s_status.total_bytes;
        }

        if (s_progress_cb) {
            s_progress_cb(s_status.progress_percent, s_progress_user_data);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA download failed: %s", esp_err_to_name(err));
        snprintf(s_status.error_message, sizeof(s_status.error_message),
                 "Download failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        esp_https_ota_abort(https_ota_handle);
        vTaskDelete(NULL);
        return;
    }

    // Verify and finish
    s_status.state = OTA_STATE_VERIFYING;
    ESP_LOGI(TAG, "OTA download complete, verifying...");

    if (!esp_https_ota_is_complete_data_received(https_ota_handle)) {
        ESP_LOGE(TAG, "Incomplete OTA data received");
        snprintf(s_status.error_message, sizeof(s_status.error_message),
                 "Incomplete data");
        s_status.state = OTA_STATE_ERROR;
        esp_https_ota_abort(https_ota_handle);
        vTaskDelete(NULL);
        return;
    }

    err = esp_https_ota_finish(https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(err));
        snprintf(s_status.error_message, sizeof(s_status.error_message),
                 "Finish failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "OTA update successful - ready to reboot");
    s_status.state = OTA_STATE_READY_TO_REBOOT;
    s_status.progress_percent = 100;

    vTaskDelete(NULL);
}

esp_err_t ota_manager_start_update(const char *url, ota_progress_cb_t progress_cb, void *user_data)
{
    if (!url || strlen(url) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_status.state != OTA_STATE_IDLE && s_status.state != OTA_STATE_ERROR) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(s_ota_url, url, OTA_MAX_URL_LEN - 1);
    s_progress_cb = progress_cb;
    s_progress_user_data = user_data;

    // Clear previous error
    s_status.error_message[0] = '\0';

    // Start download task
    BaseType_t ret = xTaskCreate(ota_download_task, "ota_download", 8192, NULL, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t ota_manager_begin_upload(size_t total_size)
{
    if (s_status.state != OTA_STATE_IDLE && s_status.state != OTA_STATE_ERROR) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Beginning OTA upload, size: %zu", total_size);

    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Writing to partition: %s @ 0x%lx",
             s_update_partition->label, (unsigned long)s_update_partition->address);

    esp_err_t err = esp_ota_begin(s_update_partition, total_size, &s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    s_status.state = OTA_STATE_DOWNLOADING;
    s_status.total_bytes = total_size;
    s_status.bytes_written = 0;
    s_status.progress_percent = 0;
    s_status.error_message[0] = '\0';

    return ESP_OK;
}

esp_err_t ota_manager_write_chunk(const uint8_t *data, size_t len)
{
    if (s_status.state != OTA_STATE_DOWNLOADING) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = esp_ota_write(s_ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        snprintf(s_status.error_message, sizeof(s_status.error_message),
                 "Write failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        esp_ota_abort(s_ota_handle);
        return err;
    }

    s_status.bytes_written += len;
    if (s_status.total_bytes > 0) {
        s_status.progress_percent = (s_status.bytes_written * 100) / s_status.total_bytes;
    }

    return ESP_OK;
}

esp_err_t ota_manager_end_upload(void)
{
    if (s_status.state != OTA_STATE_DOWNLOADING) {
        return ESP_ERR_INVALID_STATE;
    }

    s_status.state = OTA_STATE_VERIFYING;
    ESP_LOGI(TAG, "Finalizing OTA upload...");

    esp_err_t err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        snprintf(s_status.error_message, sizeof(s_status.error_message),
                 "Verify failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        return err;
    }

    err = esp_ota_set_boot_partition(s_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        snprintf(s_status.error_message, sizeof(s_status.error_message),
                 "Set boot failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        return err;
    }

    ESP_LOGI(TAG, "OTA upload successful - ready to reboot");
    s_status.state = OTA_STATE_READY_TO_REBOOT;
    s_status.progress_percent = 100;

    return ESP_OK;
}

esp_err_t ota_manager_abort(void)
{
    ESP_LOGW(TAG, "Aborting OTA");

    if (s_status.state == OTA_STATE_DOWNLOADING && s_ota_handle) {
        esp_ota_abort(s_ota_handle);
        s_ota_handle = 0;
    }

    s_status.state = OTA_STATE_IDLE;
    s_status.progress_percent = 0;
    s_status.bytes_written = 0;
    s_status.total_bytes = 0;

    return ESP_OK;
}

esp_err_t ota_manager_confirm_update(void)
{
    ESP_LOGI(TAG, "Confirming OTA update");

    // Stop rollback timer
    if (s_rollback_timer) {
        xTimerStop(s_rollback_timer, 0);
        xTimerDelete(s_rollback_timer, 0);
        s_rollback_timer = NULL;
    }
    s_rollback_remaining = 0;

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        s_status.state = OTA_STATE_IDLE;
        ESP_LOGI(TAG, "Update confirmed - rollback disabled");
    }
    return err;
}

esp_err_t ota_manager_rollback(void)
{
    ESP_LOGW(TAG, "Rolling back to previous firmware");

    // Stop rollback timer
    if (s_rollback_timer) {
        xTimerStop(s_rollback_timer, 0);
        xTimerDelete(s_rollback_timer, 0);
        s_rollback_timer = NULL;
    }

    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    // This function doesn't return if successful
    return err;
}

void ota_manager_reboot(void)
{
    if (s_status.state == OTA_STATE_READY_TO_REBOOT) {
        ESP_LOGI(TAG, "Rebooting to apply OTA update");
        vTaskDelay(pdMS_TO_TICKS(100));  // Allow logs to flush
        esp_restart();
    } else {
        ESP_LOGW(TAG, "Cannot reboot - OTA not ready");
    }
}

const char* ota_manager_get_version(void)
{
    return s_status.current_version;
}

bool ota_manager_is_pending_verify(void)
{
    return s_status.state == OTA_STATE_PENDING_VERIFY;
}

uint32_t ota_manager_get_rollback_remaining(void)
{
    return s_rollback_remaining;
}
