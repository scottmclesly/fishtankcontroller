/**
 * @file data_history.h
 * @brief Sensor Data History Buffer
 *
 * Circular buffer for storing historical sensor readings.
 * Supports export to CSV and JSON formats.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================
#define DATA_HISTORY_SIZE           288     // 24 hours at 5-minute intervals, or 24 min at 5-sec
#define DATA_HISTORY_INTERVAL_MS    5000    // 5 seconds between samples

// =============================================================================
// Data Point Structure
// =============================================================================
typedef struct {
    time_t timestamp;
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    float tds_ppm;
    float co2_ppm;
    float nh3_ppm;
    bool valid;
} data_point_t;

// =============================================================================
// History Statistics
// =============================================================================
typedef struct {
    uint32_t total_samples;
    uint32_t valid_samples;
    time_t oldest_timestamp;
    time_t newest_timestamp;

    // Averages
    float avg_temp_c;
    float avg_ph;
    float avg_orp_mv;
    float avg_ec_ms_cm;

    // Min/Max
    float min_temp_c;
    float max_temp_c;
    float min_ph;
    float max_ph;
} data_history_stats_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize data history buffer
 * @return ESP_OK on success
 */
esp_err_t data_history_init(void);

/**
 * @brief Add a data point to history
 *
 * If buffer is full, oldest entry is overwritten.
 *
 * @param point Pointer to data point
 * @return ESP_OK on success
 */
esp_err_t data_history_add(const data_point_t *point);

/**
 * @brief Get data point by index (0 = oldest)
 * @param index Index into buffer
 * @param point Pointer to receive data
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if index invalid
 */
esp_err_t data_history_get(uint32_t index, data_point_t *point);

/**
 * @brief Get most recent data point
 * @param point Pointer to receive data
 * @return ESP_OK on success
 */
esp_err_t data_history_get_latest(data_point_t *point);

/**
 * @brief Get number of data points in buffer
 * @return Number of points
 */
uint32_t data_history_count(void);

/**
 * @brief Clear all history
 */
void data_history_clear(void);

/**
 * @brief Calculate statistics from history
 * @param stats Pointer to receive statistics
 * @return ESP_OK on success
 */
esp_err_t data_history_get_stats(data_history_stats_t *stats);

// -----------------------------------------------------------------------------
// Export Functions
// -----------------------------------------------------------------------------

/**
 * @brief Export callback function type
 *
 * Called for each chunk of exported data.
 *
 * @param data Chunk of data
 * @param len Length of data
 * @param user_data User context
 * @return ESP_OK to continue, other to abort
 */
typedef esp_err_t (*data_history_export_cb_t)(const char *data, size_t len, void *user_data);

/**
 * @brief Export history to CSV format
 * @param callback Callback function for output
 * @param user_data User context passed to callback
 * @return ESP_OK on success
 */
esp_err_t data_history_export_csv(data_history_export_cb_t callback, void *user_data);

/**
 * @brief Export history to JSON format
 * @param callback Callback function for output
 * @param user_data User context passed to callback
 * @return ESP_OK on success
 */
esp_err_t data_history_export_json(data_history_export_cb_t callback, void *user_data);

/**
 * @brief Get history as JSON string (for API response)
 *
 * Allocates buffer that must be freed by caller.
 *
 * @param json_out Pointer to receive JSON string
 * @param len_out Pointer to receive length
 * @param max_points Maximum points to include (0 = all)
 * @return ESP_OK on success
 */
esp_err_t data_history_to_json(char **json_out, size_t *len_out, uint32_t max_points);

#ifdef __cplusplus
}
#endif
