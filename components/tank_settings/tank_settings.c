/**
 * @file tank_settings.c
 * @brief Tank Configuration and Fish Profile Manager
 *
 * TODO: Implement in Phase 5
 */

#include "tank_settings.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "tank_settings";

static tank_settings_t s_settings = {
    .shape = TANK_SHAPE_RECTANGLE,
    .tds_conversion_factor = 0.64f,
    .fish_profile_count = 0
};

esp_err_t tank_settings_init(void)
{
    ESP_LOGI(TAG, "Tank settings init - TODO: implement NVS loading");
    return ESP_OK;
}

esp_err_t tank_settings_get(tank_settings_t *settings)
{
    if (settings) {
        *settings = s_settings;
    }
    return ESP_OK;
}

esp_err_t tank_settings_save(const tank_settings_t *settings)
{
    if (settings) {
        s_settings = *settings;
    }
    return ESP_OK;
}

esp_err_t tank_settings_reset(void)
{
    memset(&s_settings, 0, sizeof(s_settings));
    s_settings.tds_conversion_factor = 0.64f;
    return ESP_OK;
}

float tank_settings_calculate_volume(tank_shape_t shape, const tank_dimensions_t *dims)
{
    if (!dims) return 0.0f;

    float volume_cm3 = 0.0f;
    switch (shape) {
        case TANK_SHAPE_RECTANGLE:
            volume_cm3 = dims->length_cm * dims->width_cm * dims->height_cm;
            break;
        case TANK_SHAPE_CUBE:
            volume_cm3 = dims->length_cm * dims->length_cm * dims->length_cm;
            break;
        case TANK_SHAPE_CYLINDER:
            volume_cm3 = M_PI * dims->radius_cm * dims->radius_cm * dims->height_cm;
            break;
        default:
            return 0.0f;
    }
    return volume_cm3 / 1000.0f;  // cm³ to liters
}

float tank_settings_get_volume(void)
{
    if (s_settings.manual_volume_liters > 0.0f) {
        return s_settings.manual_volume_liters;
    }
    return s_settings.calculated_volume_liters;
}

float tank_settings_get_total_fish_length(void)
{
    float total = 0.0f;
    for (uint8_t i = 0; i < s_settings.fish_profile_count; i++) {
        total += s_settings.fish_profiles[i].count * s_settings.fish_profiles[i].avg_length_cm;
    }
    return total;
}

float tank_settings_get_kh(void)
{
    return s_settings.manual_kh_dkh;
}

float tank_settings_get_tan(void)
{
    return s_settings.manual_tan_ppm;
}

float tank_settings_get_tds_factor(void)
{
    return s_settings.tds_conversion_factor;
}

esp_err_t tank_settings_add_fish(const char *species, uint8_t count, float avg_length)
{
    if (s_settings.fish_profile_count >= TANK_MAX_FISH_PROFILES) {
        return ESP_ERR_NO_MEM;
    }
    fish_profile_t *profile = &s_settings.fish_profiles[s_settings.fish_profile_count];
    strncpy(profile->species, species, TANK_MAX_SPECIES_NAME_LEN - 1);
    profile->count = count;
    profile->avg_length_cm = avg_length;
    s_settings.fish_profile_count++;
    return ESP_OK;
}

esp_err_t tank_settings_remove_fish(uint8_t index)
{
    if (index >= s_settings.fish_profile_count) {
        return ESP_ERR_INVALID_ARG;
    }
    // Shift remaining profiles down
    for (uint8_t i = index; i < s_settings.fish_profile_count - 1; i++) {
        s_settings.fish_profiles[i] = s_settings.fish_profiles[i + 1];
    }
    s_settings.fish_profile_count--;
    return ESP_OK;
}

esp_err_t tank_settings_clear_fish(void)
{
    s_settings.fish_profile_count = 0;
    return ESP_OK;
}
