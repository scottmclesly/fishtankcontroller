/**
 * @file data_history.c
 * @brief Sensor Data History Buffer
 *
 * Circular buffer for storing historical sensor readings with export support.
 */

#include "data_history.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

static const char *TAG = "data_history";

static data_point_t s_buffer[DATA_HISTORY_SIZE] = {0};
static uint32_t s_head = 0;
static uint32_t s_count = 0;

esp_err_t data_history_init(void)
{
    ESP_LOGI(TAG, "Data history init (buffer size: %d)", DATA_HISTORY_SIZE);
    data_history_clear();
    return ESP_OK;
}

esp_err_t data_history_add(const data_point_t *point)
{
    if (!point) return ESP_ERR_INVALID_ARG;

    s_buffer[s_head] = *point;
    s_head = (s_head + 1) % DATA_HISTORY_SIZE;
    if (s_count < DATA_HISTORY_SIZE) {
        s_count++;
    }
    return ESP_OK;
}

esp_err_t data_history_get(uint32_t index, data_point_t *point)
{
    if (!point || index >= s_count) {
        return ESP_ERR_NOT_FOUND;
    }

    // Calculate actual position in circular buffer
    uint32_t start = (s_count < DATA_HISTORY_SIZE) ? 0 : s_head;
    uint32_t pos = (start + index) % DATA_HISTORY_SIZE;
    *point = s_buffer[pos];
    return ESP_OK;
}

esp_err_t data_history_get_latest(data_point_t *point)
{
    if (!point || s_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    uint32_t latest = (s_head == 0) ? DATA_HISTORY_SIZE - 1 : s_head - 1;
    *point = s_buffer[latest];
    return ESP_OK;
}

uint32_t data_history_count(void)
{
    return s_count;
}

void data_history_clear(void)
{
    memset(s_buffer, 0, sizeof(s_buffer));
    s_head = 0;
    s_count = 0;
}

esp_err_t data_history_get_stats(data_history_stats_t *stats)
{
    if (!stats) return ESP_ERR_INVALID_ARG;

    memset(stats, 0, sizeof(*stats));
    stats->total_samples = s_count;

    if (s_count == 0) {
        return ESP_OK;
    }

    // Initialize min/max
    stats->min_temp_c = FLT_MAX;
    stats->max_temp_c = -FLT_MAX;
    stats->min_ph = FLT_MAX;
    stats->max_ph = -FLT_MAX;

    float sum_temp = 0, sum_ph = 0, sum_orp = 0, sum_ec = 0;
    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < s_count; i++) {
        data_point_t point;
        if (data_history_get(i, &point) != ESP_OK) continue;

        // Track timestamps
        if (i == 0) {
            stats->oldest_timestamp = point.timestamp;
        }
        stats->newest_timestamp = point.timestamp;

        if (!point.valid) continue;

        valid_count++;

        // Accumulate sums
        sum_temp += point.temp_c;
        sum_ph += point.ph;
        sum_orp += point.orp_mv;
        sum_ec += point.ec_ms_cm;

        // Track min/max
        if (point.temp_c < stats->min_temp_c) stats->min_temp_c = point.temp_c;
        if (point.temp_c > stats->max_temp_c) stats->max_temp_c = point.temp_c;
        if (point.ph < stats->min_ph) stats->min_ph = point.ph;
        if (point.ph > stats->max_ph) stats->max_ph = point.ph;
    }

    stats->valid_samples = valid_count;

    // Calculate averages
    if (valid_count > 0) {
        stats->avg_temp_c = sum_temp / valid_count;
        stats->avg_ph = sum_ph / valid_count;
        stats->avg_orp_mv = sum_orp / valid_count;
        stats->avg_ec_ms_cm = sum_ec / valid_count;
    }

    // Reset min/max if no valid samples
    if (valid_count == 0) {
        stats->min_temp_c = 0;
        stats->max_temp_c = 0;
        stats->min_ph = 0;
        stats->max_ph = 0;
    }

    return ESP_OK;
}

esp_err_t data_history_export_csv(data_history_export_cb_t callback, void *user_data)
{
    if (!callback) return ESP_ERR_INVALID_ARG;

    // CSV header
    const char *header = "timestamp,temp_c,orp_mv,ph,ec_ms_cm,tds_ppm,co2_ppm,nh3_ppm,valid\n";
    esp_err_t ret = callback(header, strlen(header), user_data);
    if (ret != ESP_OK) return ret;

    // Export each data point
    char line[128];
    for (uint32_t i = 0; i < s_count; i++) {
        data_point_t point;
        if (data_history_get(i, &point) != ESP_OK) continue;

        int len = snprintf(line, sizeof(line),
            "%ld,%.2f,%.1f,%.3f,%.4f,%.1f,%.1f,%.4f,%d\n",
            (long)point.timestamp,
            (double)point.temp_c,
            (double)point.orp_mv,
            (double)point.ph,
            (double)point.ec_ms_cm,
            (double)point.tds_ppm,
            (double)point.co2_ppm,
            (double)point.nh3_ppm,
            point.valid ? 1 : 0);

        ret = callback(line, len, user_data);
        if (ret != ESP_OK) return ret;
    }

    return ESP_OK;
}

esp_err_t data_history_export_json(data_history_export_cb_t callback, void *user_data)
{
    if (!callback) return ESP_ERR_INVALID_ARG;

    // Start JSON array
    esp_err_t ret = callback("{\"history\":[", 12, user_data);
    if (ret != ESP_OK) return ret;

    // Export each data point
    char buf[256];
    for (uint32_t i = 0; i < s_count; i++) {
        data_point_t point;
        if (data_history_get(i, &point) != ESP_OK) continue;

        int len = snprintf(buf, sizeof(buf),
            "%s{\"ts\":%ld,\"temp\":%.2f,\"orp\":%.1f,\"ph\":%.3f,\"ec\":%.4f,"
            "\"tds\":%.1f,\"co2\":%.1f,\"nh3\":%.4f,\"valid\":%s}",
            i > 0 ? "," : "",
            (long)point.timestamp,
            (double)point.temp_c,
            (double)point.orp_mv,
            (double)point.ph,
            (double)point.ec_ms_cm,
            (double)point.tds_ppm,
            (double)point.co2_ppm,
            (double)point.nh3_ppm,
            point.valid ? "true" : "false");

        ret = callback(buf, len, user_data);
        if (ret != ESP_OK) return ret;
    }

    // Close JSON
    ret = callback("]}", 2, user_data);
    return ret;
}

esp_err_t data_history_to_json(char **json_out, size_t *len_out, uint32_t max_points)
{
    if (!json_out) return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON *history = cJSON_AddArrayToObject(root, "history");
    if (!history) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    // Add stats
    data_history_stats_t stats;
    if (data_history_get_stats(&stats) == ESP_OK) {
        cJSON *stats_obj = cJSON_AddObjectToObject(root, "stats");
        cJSON_AddNumberToObject(stats_obj, "total_samples", stats.total_samples);
        cJSON_AddNumberToObject(stats_obj, "valid_samples", stats.valid_samples);
        cJSON_AddNumberToObject(stats_obj, "avg_temp_c", stats.avg_temp_c);
        cJSON_AddNumberToObject(stats_obj, "avg_ph", stats.avg_ph);
        cJSON_AddNumberToObject(stats_obj, "min_temp_c", stats.min_temp_c);
        cJSON_AddNumberToObject(stats_obj, "max_temp_c", stats.max_temp_c);
    }

    // Determine how many points to include
    uint32_t points_to_include = s_count;
    if (max_points > 0 && max_points < s_count) {
        points_to_include = max_points;
    }

    // Add most recent points (skip oldest if limited)
    uint32_t start_idx = (points_to_include < s_count) ? (s_count - points_to_include) : 0;

    for (uint32_t i = start_idx; i < s_count; i++) {
        data_point_t point;
        if (data_history_get(i, &point) != ESP_OK) continue;

        cJSON *item = cJSON_CreateObject();
        if (!item) continue;

        cJSON_AddNumberToObject(item, "ts", (double)point.timestamp);
        cJSON_AddNumberToObject(item, "temp", point.temp_c);
        cJSON_AddNumberToObject(item, "orp", point.orp_mv);
        cJSON_AddNumberToObject(item, "ph", point.ph);
        cJSON_AddNumberToObject(item, "ec", point.ec_ms_cm);
        cJSON_AddNumberToObject(item, "tds", point.tds_ppm);
        cJSON_AddNumberToObject(item, "co2", point.co2_ppm);
        cJSON_AddNumberToObject(item, "nh3", point.nh3_ppm);
        cJSON_AddBoolToObject(item, "valid", point.valid);

        cJSON_AddItemToArray(history, item);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        return ESP_ERR_NO_MEM;
    }

    *json_out = json_str;
    if (len_out) {
        *len_out = strlen(json_str);
    }

    return ESP_OK;
}
