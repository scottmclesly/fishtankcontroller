/**
 * @file warning_manager.c
 * @brief Parameter Warning and Alert Manager
 *
 * Evaluates sensor readings against configurable thresholds
 * with hysteresis to prevent warning state flicker.
 */

#include "warning_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <math.h>

static const char *TAG = "warning_manager";

// =============================================================================
// Internal State
// =============================================================================

static warning_thresholds_t s_thresholds = {0};
static warning_status_t s_status = {0};

// Metric state for hysteresis and rate-of-change tracking
typedef struct {
    warning_state_t state;
    float current_value;
    float previous_value;
    int64_t previous_timestamp_ms;
    bool has_history;
} metric_state_t;

static struct {
    metric_state_t temperature;
    metric_state_t ph;
    metric_state_t nh3;
    metric_state_t orp;
    metric_state_t ec;
    metric_state_t salinity;
    metric_state_t dissolved_oxygen;
} s_metric_states = {0};

// =============================================================================
// Default Threshold Profiles
// =============================================================================

static void load_freshwater_community_defaults(void)
{
    s_thresholds.tank_type = TANK_TYPE_FRESHWATER_COMMUNITY;

    // Temperature (°C)
    s_thresholds.temperature.warn_low = 20.0f;
    s_thresholds.temperature.warn_high = 28.0f;
    s_thresholds.temperature.crit_low = 18.0f;
    s_thresholds.temperature.crit_high = 30.0f;
    s_thresholds.temperature.rate_change_per_hour = 2.0f;

    // pH
    s_thresholds.ph.warn_low = 6.0f;
    s_thresholds.ph.warn_high = 8.0f;
    s_thresholds.ph.crit_low = 5.5f;
    s_thresholds.ph.crit_high = 8.5f;
    s_thresholds.ph.rate_change_per_hour = 0.0125f;  // 0.3 per 24h

    // NH3 (toxic ammonia ppm)
    s_thresholds.nh3.warn_high = 0.02f;
    s_thresholds.nh3.crit_high = 0.05f;

    // ORP (mV)
    s_thresholds.orp.warn_low = 200.0f;
    s_thresholds.orp.warn_high = 400.0f;
    s_thresholds.orp.crit_low = 180.0f;
    s_thresholds.orp.crit_high = 450.0f;
    s_thresholds.orp.rate_change_per_hour = 0.0f;  // Not typically monitored

    // Conductivity (µS/cm)
    s_thresholds.ec.warn_low = 100.0f;
    s_thresholds.ec.warn_high = 600.0f;
    s_thresholds.ec.crit_low = 50.0f;
    s_thresholds.ec.crit_high = 1200.0f;
    s_thresholds.ec.rate_change_per_hour = 0.0f;

    // Salinity (not applicable for freshwater)
    s_thresholds.salinity.warn_low = 0.0f;
    s_thresholds.salinity.warn_high = 1.0f;
    s_thresholds.salinity.crit_low = 0.0f;
    s_thresholds.salinity.crit_high = 2.0f;
    s_thresholds.salinity.rate_change_per_hour = 0.0f;

    // Dissolved Oxygen (mg/L) - only low thresholds matter
    s_thresholds.dissolved_oxygen.warn_low = 6.0f;
    s_thresholds.dissolved_oxygen.warn_high = 100.0f;  // No high limit
    s_thresholds.dissolved_oxygen.crit_low = 4.0f;
    s_thresholds.dissolved_oxygen.crit_high = 100.0f;
    s_thresholds.dissolved_oxygen.rate_change_per_hour = 0.0f;
}

static void load_freshwater_planted_defaults(void)
{
    // Start with community defaults
    load_freshwater_community_defaults();
    s_thresholds.tank_type = TANK_TYPE_FRESHWATER_PLANTED;

    // Planted tanks benefit from slightly lower pH
    s_thresholds.ph.warn_low = 6.0f;
    s_thresholds.ph.warn_high = 7.5f;
    s_thresholds.ph.crit_low = 5.5f;
    s_thresholds.ph.crit_high = 8.0f;

    // Higher conductivity acceptable for planted tanks
    s_thresholds.ec.warn_high = 1000.0f;
    s_thresholds.ec.crit_high = 1500.0f;
}

static void load_saltwater_fish_only_defaults(void)
{
    s_thresholds.tank_type = TANK_TYPE_SALTWATER_FISH_ONLY;

    // Temperature (°C)
    s_thresholds.temperature.warn_low = 22.0f;
    s_thresholds.temperature.warn_high = 28.0f;
    s_thresholds.temperature.crit_low = 20.0f;
    s_thresholds.temperature.crit_high = 30.0f;
    s_thresholds.temperature.rate_change_per_hour = 1.5f;

    // pH
    s_thresholds.ph.warn_low = 7.8f;
    s_thresholds.ph.warn_high = 8.6f;
    s_thresholds.ph.crit_low = 7.7f;
    s_thresholds.ph.crit_high = 8.7f;
    s_thresholds.ph.rate_change_per_hour = 0.0083f;  // 0.2 per 24h

    // NH3 (saltwater is more sensitive)
    s_thresholds.nh3.warn_high = 0.01f;
    s_thresholds.nh3.crit_high = 0.02f;

    // ORP (mV)
    s_thresholds.orp.warn_low = 250.0f;
    s_thresholds.orp.warn_high = 450.0f;
    s_thresholds.orp.crit_low = 220.0f;
    s_thresholds.orp.crit_high = 480.0f;
    s_thresholds.orp.rate_change_per_hour = 0.0f;

    // Conductivity (µS/cm) - high for saltwater
    s_thresholds.ec.warn_low = 40000.0f;
    s_thresholds.ec.warn_high = 60000.0f;
    s_thresholds.ec.crit_low = 35000.0f;
    s_thresholds.ec.crit_high = 65000.0f;
    s_thresholds.ec.rate_change_per_hour = 0.0f;

    // Salinity (PSU)
    s_thresholds.salinity.warn_low = 33.0f;
    s_thresholds.salinity.warn_high = 36.0f;
    s_thresholds.salinity.crit_low = 32.0f;
    s_thresholds.salinity.crit_high = 37.0f;
    s_thresholds.salinity.rate_change_per_hour = 0.0f;

    // Dissolved Oxygen (mg/L)
    s_thresholds.dissolved_oxygen.warn_low = 6.0f;
    s_thresholds.dissolved_oxygen.warn_high = 100.0f;
    s_thresholds.dissolved_oxygen.crit_low = 4.0f;
    s_thresholds.dissolved_oxygen.crit_high = 100.0f;
    s_thresholds.dissolved_oxygen.rate_change_per_hour = 0.0f;
}

static void load_reef_defaults(void)
{
    // Start with saltwater defaults
    load_saltwater_fish_only_defaults();
    s_thresholds.tank_type = TANK_TYPE_SALTWATER_REEF;

    // Tighter temperature control for reef
    s_thresholds.temperature.warn_low = 24.0f;
    s_thresholds.temperature.warn_high = 26.0f;
    s_thresholds.temperature.crit_low = 22.0f;
    s_thresholds.temperature.crit_high = 28.0f;

    // Tighter pH range for reef
    s_thresholds.ph.warn_low = 8.1f;
    s_thresholds.ph.warn_high = 8.4f;
    s_thresholds.ph.crit_low = 7.9f;
    s_thresholds.ph.crit_high = 8.6f;

    // Higher ORP typical for reef systems
    s_thresholds.orp.warn_low = 300.0f;
    s_thresholds.orp.warn_high = 450.0f;
    s_thresholds.orp.crit_low = 250.0f;
    s_thresholds.orp.crit_high = 500.0f;

    // Tighter salinity range
    s_thresholds.salinity.warn_low = 34.0f;
    s_thresholds.salinity.warn_high = 35.5f;
    s_thresholds.salinity.crit_low = 33.0f;
    s_thresholds.salinity.crit_high = 36.5f;
}

// =============================================================================
// NVS Storage
// =============================================================================

static esp_err_t save_thresholds_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WARNING_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Save tank type
    err = nvs_set_u8(handle, "tank_type", (uint8_t)s_thresholds.tank_type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save tank_type: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Save complete thresholds as blob
    err = nvs_set_blob(handle, "thresholds", &s_thresholds, sizeof(s_thresholds));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save thresholds blob: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Thresholds saved to NVS");
    }
    return err;
}

static esp_err_t load_thresholds_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WARNING_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No stored thresholds, using defaults");
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Load thresholds blob
    size_t required_size = sizeof(s_thresholds);
    err = nvs_get_blob(handle, "thresholds", &s_thresholds, &required_size);
    if (err != ESP_OK || required_size != sizeof(s_thresholds)) {
        ESP_LOGW(TAG, "Stored thresholds invalid or size mismatch, using defaults");
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded thresholds from NVS, tank_type=%d", s_thresholds.tank_type);
    return ESP_OK;
}

// =============================================================================
// Evaluation Helpers
// =============================================================================

static int64_t get_time_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static warning_state_t evaluate_range(float value, const threshold_range_t *thresh,
                                      metric_state_t *state)
{
    // Update history
    if (state->has_history) {
        state->previous_value = state->current_value;
        state->previous_timestamp_ms = get_time_ms();
    }
    state->current_value = value;
    state->has_history = true;

    // Calculate hysteresis bands
    float hyst_factor = WARNING_HYSTERESIS_PERCENT / 100.0f;
    float hyst_low = (thresh->warn_low - thresh->crit_low) * hyst_factor;
    float hyst_high = (thresh->crit_high - thresh->warn_high) * hyst_factor;

    warning_state_t new_state = WARNING_STATE_NORMAL;

    // Check critical thresholds first
    if (value <= thresh->crit_low) {
        new_state = WARNING_STATE_CRITICAL;
    } else if (value >= thresh->crit_high) {
        new_state = WARNING_STATE_CRITICAL;
    }
    // Check warning thresholds
    else if (value <= thresh->warn_low) {
        new_state = WARNING_STATE_WARNING;
    } else if (value >= thresh->warn_high) {
        new_state = WARNING_STATE_WARNING;
    }
    // Apply hysteresis if transitioning from warning/critical to normal
    else if (state->state >= WARNING_STATE_WARNING) {
        if (value < (thresh->warn_low + hyst_low) ||
            value > (thresh->warn_high - hyst_high)) {
            new_state = state->state;  // Stay in current state
        }
    }

    state->state = new_state;
    return new_state;
}

static warning_state_t evaluate_high_only(float value, const threshold_high_only_t *thresh,
                                          metric_state_t *state)
{
    // Update history
    if (state->has_history) {
        state->previous_value = state->current_value;
        state->previous_timestamp_ms = get_time_ms();
    }
    state->current_value = value;
    state->has_history = true;

    warning_state_t new_state = WARNING_STATE_NORMAL;

    if (value >= thresh->crit_high) {
        new_state = WARNING_STATE_CRITICAL;
    } else if (value >= thresh->warn_high) {
        new_state = WARNING_STATE_WARNING;
    }
    // Apply hysteresis
    else if (state->state >= WARNING_STATE_WARNING) {
        float hyst = (thresh->crit_high - thresh->warn_high) * (WARNING_HYSTERESIS_PERCENT / 100.0f);
        if (value > (thresh->warn_high - hyst)) {
            new_state = state->state;
        }
    }

    state->state = new_state;
    return new_state;
}

static warning_state_t evaluate_low_only(float value, float warn_low, float crit_low,
                                         metric_state_t *state)
{
    // Update history
    if (state->has_history) {
        state->previous_value = state->current_value;
        state->previous_timestamp_ms = get_time_ms();
    }
    state->current_value = value;
    state->has_history = true;

    warning_state_t new_state = WARNING_STATE_NORMAL;

    if (value <= crit_low) {
        new_state = WARNING_STATE_CRITICAL;
    } else if (value <= warn_low) {
        new_state = WARNING_STATE_WARNING;
    }
    // Apply hysteresis
    else if (state->state >= WARNING_STATE_WARNING) {
        float hyst = (warn_low - crit_low) * (WARNING_HYSTERESIS_PERCENT / 100.0f);
        if (value < (warn_low + hyst)) {
            new_state = state->state;
        }
    }

    state->state = new_state;
    return new_state;
}

// =============================================================================
// API Functions
// =============================================================================

esp_err_t warning_manager_init(void)
{
    // Try to load from NVS
    esp_err_t err = load_thresholds_from_nvs();
    if (err != ESP_OK) {
        // No stored thresholds, use defaults
        load_freshwater_community_defaults();
    }

    // Reset all warning states to unknown
    warning_manager_reset_states();

    ESP_LOGI(TAG, "Warning manager initialized, tank_type=%d", s_thresholds.tank_type);
    return ESP_OK;
}

esp_err_t warning_manager_set_tank_type(tank_type_t type)
{
    switch (type) {
        case TANK_TYPE_FRESHWATER_COMMUNITY:
            load_freshwater_community_defaults();
            break;
        case TANK_TYPE_FRESHWATER_PLANTED:
            load_freshwater_planted_defaults();
            break;
        case TANK_TYPE_SALTWATER_FISH_ONLY:
            load_saltwater_fish_only_defaults();
            break;
        case TANK_TYPE_SALTWATER_REEF:
            load_reef_defaults();
            break;
        case TANK_TYPE_CUSTOM:
            // Keep current thresholds, just update type
            s_thresholds.tank_type = TANK_TYPE_CUSTOM;
            break;
        default:
            ESP_LOGW(TAG, "Unknown tank type %d, using freshwater community", type);
            load_freshwater_community_defaults();
            break;
    }

    // Save to NVS
    save_thresholds_to_nvs();

    ESP_LOGI(TAG, "Tank type set to %d", s_thresholds.tank_type);
    return ESP_OK;
}

tank_type_t warning_manager_get_tank_type(void)
{
    return s_thresholds.tank_type;
}

esp_err_t warning_manager_get_thresholds(warning_thresholds_t *thresholds)
{
    if (!thresholds) {
        return ESP_ERR_INVALID_ARG;
    }
    *thresholds = s_thresholds;
    return ESP_OK;
}

esp_err_t warning_manager_set_thresholds(const warning_thresholds_t *thresholds)
{
    if (!thresholds) {
        return ESP_ERR_INVALID_ARG;
    }
    s_thresholds = *thresholds;
    s_thresholds.tank_type = TANK_TYPE_CUSTOM;  // Mark as custom when manually set

    // Save to NVS
    save_thresholds_to_nvs();

    ESP_LOGI(TAG, "Custom thresholds set and saved");
    return ESP_OK;
}

warning_state_t warning_manager_evaluate_temp(float temp_c)
{
    warning_state_t state = evaluate_range(temp_c, &s_thresholds.temperature,
                                           &s_metric_states.temperature);
    s_status.temperature = state;

    // Calculate rate of change for status reporting
    if (s_metric_states.temperature.has_history) {
        int64_t now = get_time_ms();
        int64_t elapsed_ms = now - s_metric_states.temperature.previous_timestamp_ms;
        if (elapsed_ms > 0) {
            float delta = fabsf(s_metric_states.temperature.current_value -
                               s_metric_states.temperature.previous_value);
            // Convert to per-hour rate
            s_status.temp_rate_per_hour = delta * (3600000.0f / (float)elapsed_ms);
        }
    }

    return state;
}

warning_state_t warning_manager_evaluate_ph(float ph)
{
    warning_state_t state = evaluate_range(ph, &s_thresholds.ph, &s_metric_states.ph);
    s_status.ph = state;

    // Calculate 24h rate for status reporting
    if (s_metric_states.ph.has_history) {
        int64_t now = get_time_ms();
        int64_t elapsed_ms = now - s_metric_states.ph.previous_timestamp_ms;
        if (elapsed_ms > 0) {
            float delta = fabsf(s_metric_states.ph.current_value -
                               s_metric_states.ph.previous_value);
            // Convert to per-24h rate
            s_status.ph_rate_per_24h = delta * (86400000.0f / (float)elapsed_ms);
        }
    }

    return state;
}

warning_state_t warning_manager_evaluate_nh3(float nh3_ppm)
{
    warning_state_t state = evaluate_high_only(nh3_ppm, &s_thresholds.nh3,
                                               &s_metric_states.nh3);
    s_status.nh3 = state;
    return state;
}

warning_state_t warning_manager_evaluate_orp(float orp_mv)
{
    warning_state_t state = evaluate_range(orp_mv, &s_thresholds.orp,
                                           &s_metric_states.orp);
    s_status.orp = state;
    return state;
}

warning_state_t warning_manager_evaluate_ec(float ec_us_cm)
{
    warning_state_t state = evaluate_range(ec_us_cm, &s_thresholds.ec,
                                           &s_metric_states.ec);
    s_status.ec = state;
    return state;
}

warning_state_t warning_manager_evaluate_do(float do_mg_l)
{
    // DO only cares about low values (low oxygen is bad)
    warning_state_t state = evaluate_low_only(do_mg_l,
                                              s_thresholds.dissolved_oxygen.warn_low,
                                              s_thresholds.dissolved_oxygen.crit_low,
                                              &s_metric_states.dissolved_oxygen);
    s_status.dissolved_oxygen = state;
    return state;
}

void warning_manager_evaluate_all(float temp_c, float ph, float nh3_ppm,
                                  float orp_mv, float ec_us_cm, float do_mg_l)
{
    warning_manager_evaluate_temp(temp_c);
    warning_manager_evaluate_ph(ph);
    warning_manager_evaluate_nh3(nh3_ppm);
    warning_manager_evaluate_orp(orp_mv);
    warning_manager_evaluate_ec(ec_us_cm);
    warning_manager_evaluate_do(do_mg_l);
}

esp_err_t warning_manager_get_status(warning_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    *status = s_status;
    return ESP_OK;
}

void warning_manager_reset_states(void)
{
    s_status.temperature = WARNING_STATE_UNKNOWN;
    s_status.ph = WARNING_STATE_UNKNOWN;
    s_status.nh3 = WARNING_STATE_UNKNOWN;
    s_status.orp = WARNING_STATE_UNKNOWN;
    s_status.ec = WARNING_STATE_UNKNOWN;
    s_status.salinity = WARNING_STATE_UNKNOWN;
    s_status.dissolved_oxygen = WARNING_STATE_UNKNOWN;
    s_status.temp_rate_per_hour = 0.0f;
    s_status.ph_rate_per_24h = 0.0f;

    // Reset metric states
    memset(&s_metric_states, 0, sizeof(s_metric_states));
}

const char* warning_state_to_string(warning_state_t state)
{
    switch (state) {
        case WARNING_STATE_UNKNOWN:  return "UNKNOWN";
        case WARNING_STATE_NORMAL:   return "NORMAL";
        case WARNING_STATE_WARNING:  return "WARNING";
        case WARNING_STATE_CRITICAL: return "CRITICAL";
        default:                     return "UNKNOWN";
    }
}
