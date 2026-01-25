/**
 * @file ota_manager.h
 * @brief OTA (Over-The-Air) Update Manager
 *
 * Handles firmware updates with rollback protection.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define OTA_ROLLBACK_TIMEOUT_S      300     // 5 minutes to confirm update
#define OTA_MAX_URL_LEN             256
#define OTA_MAX_VERSION_LEN         32

// =============================================================================
// OTA State Enumeration
// =============================================================================
typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_READY_TO_REBOOT,
    OTA_STATE_PENDING_VERIFY,       // After reboot, waiting for confirmation
    OTA_STATE_ERROR
} ota_state_t;

// =============================================================================
// OTA Status Structure
// =============================================================================
typedef struct {
    ota_state_t state;
    int progress_percent;           // 0-100 during download
    char error_message[64];
    char current_version[OTA_MAX_VERSION_LEN];
    char new_version[OTA_MAX_VERSION_LEN];
    bool can_rollback;              // Previous version available
    uint32_t bytes_written;
    uint32_t total_bytes;
} ota_status_t;

// =============================================================================
// OTA Progress Callback
// =============================================================================
typedef void (*ota_progress_cb_t)(int percent, void *user_data);

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize OTA manager
 *
 * Checks boot state and starts rollback timer if needed.
 *
 * @return ESP_OK on success
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Get current OTA status
 * @param status Pointer to status structure
 * @return ESP_OK on success
 */
esp_err_t ota_manager_get_status(ota_status_t *status);

/**
 * @brief Start OTA update from URL
 *
 * Downloads and installs firmware in background task.
 *
 * @param url URL to firmware binary
 * @param progress_cb Progress callback (optional)
 * @param user_data User data for callback
 * @return ESP_OK if download started
 */
esp_err_t ota_manager_start_update(const char *url, ota_progress_cb_t progress_cb, void *user_data);

/**
 * @brief Start OTA update from uploaded data
 *
 * For direct binary upload via HTTP POST.
 *
 * @param data Firmware data
 * @param len Data length
 * @return ESP_OK on success
 */
esp_err_t ota_manager_write_chunk(const uint8_t *data, size_t len);

/**
 * @brief Begin OTA upload session
 * @param total_size Expected total size
 * @return ESP_OK on success
 */
esp_err_t ota_manager_begin_upload(size_t total_size);

/**
 * @brief End OTA upload session
 * @return ESP_OK on success
 */
esp_err_t ota_manager_end_upload(void);

/**
 * @brief Abort current OTA operation
 * @return ESP_OK on success
 */
esp_err_t ota_manager_abort(void);

/**
 * @brief Confirm current firmware is working
 *
 * Call this after successful boot to prevent automatic rollback.
 *
 * @return ESP_OK on success
 */
esp_err_t ota_manager_confirm_update(void);

/**
 * @brief Rollback to previous firmware
 * @return ESP_OK on success (will reboot)
 */
esp_err_t ota_manager_rollback(void);

/**
 * @brief Reboot to apply update
 *
 * Only valid when state is OTA_STATE_READY_TO_REBOOT.
 */
void ota_manager_reboot(void);

/**
 * @brief Get current firmware version
 * @return Version string
 */
const char* ota_manager_get_version(void);

/**
 * @brief Check if update is pending verification
 * @return true if waiting for confirmation
 */
bool ota_manager_is_pending_verify(void);

/**
 * @brief Get remaining time until automatic rollback
 * @return Seconds remaining, or 0 if not pending
 */
uint32_t ota_manager_get_rollback_remaining(void);

#ifdef __cplusplus
}
#endif
