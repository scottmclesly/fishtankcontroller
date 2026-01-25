/**
 * @file calibration.c
 * @brief pH and EC Sensor Calibration Manager
 *
 * Handles calibration data storage and calculation for pH (1-point/2-point)
 * and EC (cell constant) calibration. Uses ESP-IDF NVS for persistence.
 */

#include "calibration.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <math.h>

static const char *TAG = "calibration";

// =============================================================================
// NVS Keys
// =============================================================================
#define NVS_KEY_PH_CALIBRATED   "ph_cal"
#define NVS_KEY_PH_TWO_POINT    "ph_2pt"
#define NVS_KEY_PH_P1_PH        "ph_p1_ph"
#define NVS_KEY_PH_P1_UV        "ph_p1_uv"
#define NVS_KEY_PH_P2_PH        "ph_p2_ph"
#define NVS_KEY_PH_P2_UV        "ph_p2_uv"
#define NVS_KEY_PH_SENS         "ph_sens"
#define NVS_KEY_PH_OFFSET       "ph_off"
#define NVS_KEY_PH_TS           "ph_ts"

#define NVS_KEY_EC_CALIBRATED   "ec_cal"
#define NVS_KEY_EC_SOLUTION     "ec_sol"
#define NVS_KEY_EC_TEMP         "ec_temp"
#define NVS_KEY_EC_NA           "ec_na"
#define NVS_KEY_EC_UV           "ec_uv"
#define NVS_KEY_EC_K            "ec_k"
#define NVS_KEY_EC_TS           "ec_ts"

// =============================================================================
// Static Variables
// =============================================================================
static nvs_handle_t s_nvs_handle = 0;
static ph_calibration_t s_ph_cal = {0};
static ec_calibration_t s_ec_cal = {0};
static bool s_initialized = false;

// =============================================================================
// Forward Declarations
// =============================================================================
static esp_err_t load_ph_calibration(void);
static esp_err_t save_ph_calibration(void);
static esp_err_t load_ec_calibration(void);
static esp_err_t save_ec_calibration(void);

// =============================================================================
// Initialization
// =============================================================================

esp_err_t calibration_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // Open NVS namespace
    esp_err_t ret = nvs_open(CALIBRATION_NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize defaults
    s_ph_cal.calibrated = false;
    s_ph_cal.sensitivity_mV_pH = CALIBRATION_DEFAULT_PH_SENS;

    s_ec_cal.calibrated = false;
    s_ec_cal.cell_constant = 1.0f;

    // Load stored calibration
    load_ph_calibration();
    load_ec_calibration();

    s_initialized = true;
    ESP_LOGI(TAG, "Calibration manager initialized");
    ESP_LOGI(TAG, "  pH: %s (%s)",
             s_ph_cal.calibrated ? "calibrated" : "uncalibrated",
             s_ph_cal.two_point ? "2-point" : "1-point");
    ESP_LOGI(TAG, "  EC: %s (K=%.4f)",
             s_ec_cal.calibrated ? "calibrated" : "uncalibrated",
             s_ec_cal.cell_constant);

    return ESP_OK;
}

// =============================================================================
// pH Calibration
// =============================================================================

esp_err_t calibration_ph_1point(float known_ph, float raw_ugs_uV)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "pH 1-point calibration: pH=%.2f, uV=%.0f", known_ph, raw_ugs_uV);

    s_ph_cal.calibrated = true;
    s_ph_cal.two_point = false;
    s_ph_cal.point1_ph = known_ph;
    s_ph_cal.point1_ugs_uV = raw_ugs_uV;
    s_ph_cal.sensitivity_mV_pH = CALIBRATION_DEFAULT_PH_SENS;
    s_ph_cal.offset_mV = raw_ugs_uV / 1000.0f;  // Convert to mV
    s_ph_cal.timestamp = time(NULL);

    return save_ph_calibration();
}

esp_err_t calibration_ph_2point(float known_ph, float raw_ugs_uV)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ph_cal.calibrated) {
        ESP_LOGE(TAG, "Must perform 1-point calibration first");
        return ESP_ERR_INVALID_STATE;
    }

    // Calculate sensitivity from two points
    float delta_ph = known_ph - s_ph_cal.point1_ph;
    float delta_uV = raw_ugs_uV - s_ph_cal.point1_ugs_uV;

    if (fabsf(delta_ph) < 0.1f) {
        ESP_LOGE(TAG, "pH values too close together");
        return ESP_ERR_INVALID_ARG;
    }

    float sensitivity_uV_pH = delta_uV / delta_ph;
    float sensitivity_mV_pH = sensitivity_uV_pH / 1000.0f;

    ESP_LOGI(TAG, "pH 2-point calibration: pH=%.2f, uV=%.0f", known_ph, raw_ugs_uV);
    ESP_LOGI(TAG, "  Calculated sensitivity: %.2f mV/pH", sensitivity_mV_pH);

    s_ph_cal.two_point = true;
    s_ph_cal.point2_ph = known_ph;
    s_ph_cal.point2_ugs_uV = raw_ugs_uV;
    s_ph_cal.sensitivity_mV_pH = sensitivity_mV_pH;
    s_ph_cal.timestamp = time(NULL);

    return save_ph_calibration();
}

float calibration_calculate_ph(float raw_ugs_uV)
{
    // Convert to mV
    float raw_mV = raw_ugs_uV / 1000.0f;

    if (!s_ph_cal.calibrated) {
        // Uncalibrated: assume pH 7.0 at 0mV
        return 7.0f + (raw_mV / CALIBRATION_DEFAULT_PH_SENS);
    }

    // Use calibration offset and sensitivity
    float cal_mV = s_ph_cal.point1_ugs_uV / 1000.0f;
    return s_ph_cal.point1_ph + (raw_mV - cal_mV) / s_ph_cal.sensitivity_mV_pH;
}

esp_err_t calibration_get_ph(ph_calibration_t *cal)
{
    if (cal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(cal, &s_ph_cal, sizeof(ph_calibration_t));
    return ESP_OK;
}

esp_err_t calibration_clear_ph(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Clearing pH calibration");

    s_ph_cal.calibrated = false;
    s_ph_cal.two_point = false;
    s_ph_cal.point1_ph = 7.0f;
    s_ph_cal.point1_ugs_uV = 0;
    s_ph_cal.point2_ph = 0;
    s_ph_cal.point2_ugs_uV = 0;
    s_ph_cal.sensitivity_mV_pH = CALIBRATION_DEFAULT_PH_SENS;
    s_ph_cal.offset_mV = 0;
    s_ph_cal.timestamp = 0;

    return save_ph_calibration();
}

// =============================================================================
// EC Calibration
// =============================================================================

esp_err_t calibration_ec(float known_ec_mS_cm, float temp_c,
                         float raw_ec_nA, float raw_ec_uV)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (fabsf(raw_ec_nA) < 0.001f) {
        ESP_LOGE(TAG, "Invalid EC measurement (current ~0)");
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate resistance: R = V / I
    // V in microvolts, I in nanoamps -> R in kOhms
    // Convert: uV / nA = kOhm
    float resistance_kOhm = raw_ec_uV / raw_ec_nA;
    float resistance_Ohm = resistance_kOhm * 1000.0f;

    // Calculate cell constant: K = R x EC
    // EC in S/cm = mS/cm / 1000
    float ec_S_cm = known_ec_mS_cm / 1000.0f;
    float cell_constant = resistance_Ohm * ec_S_cm;

    ESP_LOGI(TAG, "EC calibration: %.3f mS/cm @ %.1f C", known_ec_mS_cm, temp_c);
    ESP_LOGI(TAG, "  Raw: %.0f nA, %.0f uV", raw_ec_nA, raw_ec_uV);
    ESP_LOGI(TAG, "  Resistance: %.1f Ohm", resistance_Ohm);
    ESP_LOGI(TAG, "  Cell constant: %.4f /cm", cell_constant);

    s_ec_cal.calibrated = true;
    s_ec_cal.solution_ec_mS_cm = known_ec_mS_cm;
    s_ec_cal.solution_temp_c = temp_c;
    s_ec_cal.raw_ec_nA = raw_ec_nA;
    s_ec_cal.raw_ec_uV = raw_ec_uV;
    s_ec_cal.cell_constant = cell_constant;
    s_ec_cal.timestamp = time(NULL);

    return save_ec_calibration();
}

float calibration_calculate_ec(float raw_ec_nA, float raw_ec_uV)
{
    if (fabsf(raw_ec_nA) < 0.001f) {
        return 0.0f;
    }

    // Calculate resistance: R = V / I
    float resistance_kOhm = raw_ec_uV / raw_ec_nA;
    float resistance_Ohm = resistance_kOhm * 1000.0f;

    // Calculate EC: EC (S/cm) = K / R
    float cell_constant = s_ec_cal.calibrated ? s_ec_cal.cell_constant : 1.0f;
    float ec_S_cm = cell_constant / resistance_Ohm;

    // Convert to mS/cm
    return ec_S_cm * 1000.0f;
}

esp_err_t calibration_get_ec(ec_calibration_t *cal)
{
    if (cal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(cal, &s_ec_cal, sizeof(ec_calibration_t));
    return ESP_OK;
}

esp_err_t calibration_clear_ec(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Clearing EC calibration");

    s_ec_cal.calibrated = false;
    s_ec_cal.solution_ec_mS_cm = 0;
    s_ec_cal.solution_temp_c = 25.0f;
    s_ec_cal.raw_ec_nA = 0;
    s_ec_cal.raw_ec_uV = 0;
    s_ec_cal.cell_constant = 1.0f;
    s_ec_cal.timestamp = 0;

    return save_ec_calibration();
}

// =============================================================================
// General Functions
// =============================================================================

esp_err_t calibration_get_status(calibration_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    status->ph_calibrated = s_ph_cal.calibrated;
    status->ph_two_point = s_ph_cal.two_point;
    status->ph_timestamp = s_ph_cal.timestamp;
    status->ec_calibrated = s_ec_cal.calibrated;
    status->ec_timestamp = s_ec_cal.timestamp;

    return ESP_OK;
}

esp_err_t calibration_clear_all(void)
{
    esp_err_t ret = calibration_clear_ph();
    if (ret != ESP_OK) {
        return ret;
    }
    return calibration_clear_ec();
}

// =============================================================================
// NVS Storage Functions
// =============================================================================

static esp_err_t load_ph_calibration(void)
{
    uint8_t u8_val;
    float f_val;
    int64_t i64_val;

    if (nvs_get_u8(s_nvs_handle, NVS_KEY_PH_CALIBRATED, &u8_val) == ESP_OK) {
        s_ph_cal.calibrated = (u8_val != 0);
    }
    if (nvs_get_u8(s_nvs_handle, NVS_KEY_PH_TWO_POINT, &u8_val) == ESP_OK) {
        s_ph_cal.two_point = (u8_val != 0);
    }
    size_t size = sizeof(float);
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_PH_P1_PH, &f_val, &size) == ESP_OK) {
        s_ph_cal.point1_ph = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_PH_P1_UV, &f_val, &size) == ESP_OK) {
        s_ph_cal.point1_ugs_uV = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_PH_P2_PH, &f_val, &size) == ESP_OK) {
        s_ph_cal.point2_ph = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_PH_P2_UV, &f_val, &size) == ESP_OK) {
        s_ph_cal.point2_ugs_uV = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_PH_SENS, &f_val, &size) == ESP_OK) {
        s_ph_cal.sensitivity_mV_pH = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_PH_OFFSET, &f_val, &size) == ESP_OK) {
        s_ph_cal.offset_mV = f_val;
    }
    if (nvs_get_i64(s_nvs_handle, NVS_KEY_PH_TS, &i64_val) == ESP_OK) {
        s_ph_cal.timestamp = (time_t)i64_val;
    }

    return ESP_OK;
}

static esp_err_t save_ph_calibration(void)
{
    esp_err_t ret;

    ret = nvs_set_u8(s_nvs_handle, NVS_KEY_PH_CALIBRATED, s_ph_cal.calibrated ? 1 : 0);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(s_nvs_handle, NVS_KEY_PH_TWO_POINT, s_ph_cal.two_point ? 1 : 0);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_PH_P1_PH, &s_ph_cal.point1_ph, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_PH_P1_UV, &s_ph_cal.point1_ugs_uV, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_PH_P2_PH, &s_ph_cal.point2_ph, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_PH_P2_UV, &s_ph_cal.point2_ugs_uV, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_PH_SENS, &s_ph_cal.sensitivity_mV_pH, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_PH_OFFSET, &s_ph_cal.offset_mV, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_i64(s_nvs_handle, NVS_KEY_PH_TS, (int64_t)s_ph_cal.timestamp);
    if (ret != ESP_OK) return ret;

    return nvs_commit(s_nvs_handle);
}

static esp_err_t load_ec_calibration(void)
{
    uint8_t u8_val;
    float f_val;
    int64_t i64_val;

    if (nvs_get_u8(s_nvs_handle, NVS_KEY_EC_CALIBRATED, &u8_val) == ESP_OK) {
        s_ec_cal.calibrated = (u8_val != 0);
    }
    size_t size = sizeof(float);
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_EC_SOLUTION, &f_val, &size) == ESP_OK) {
        s_ec_cal.solution_ec_mS_cm = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_EC_TEMP, &f_val, &size) == ESP_OK) {
        s_ec_cal.solution_temp_c = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_EC_NA, &f_val, &size) == ESP_OK) {
        s_ec_cal.raw_ec_nA = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_EC_UV, &f_val, &size) == ESP_OK) {
        s_ec_cal.raw_ec_uV = f_val;
    }
    if (nvs_get_blob(s_nvs_handle, NVS_KEY_EC_K, &f_val, &size) == ESP_OK) {
        s_ec_cal.cell_constant = f_val;
    }
    if (nvs_get_i64(s_nvs_handle, NVS_KEY_EC_TS, &i64_val) == ESP_OK) {
        s_ec_cal.timestamp = (time_t)i64_val;
    }

    return ESP_OK;
}

static esp_err_t save_ec_calibration(void)
{
    esp_err_t ret;

    ret = nvs_set_u8(s_nvs_handle, NVS_KEY_EC_CALIBRATED, s_ec_cal.calibrated ? 1 : 0);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_EC_SOLUTION, &s_ec_cal.solution_ec_mS_cm, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_EC_TEMP, &s_ec_cal.solution_temp_c, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_EC_NA, &s_ec_cal.raw_ec_nA, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_EC_UV, &s_ec_cal.raw_ec_uV, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(s_nvs_handle, NVS_KEY_EC_K, &s_ec_cal.cell_constant, sizeof(float));
    if (ret != ESP_OK) return ret;

    ret = nvs_set_i64(s_nvs_handle, NVS_KEY_EC_TS, (int64_t)s_ec_cal.timestamp);
    if (ret != ESP_OK) return ret;

    return nvs_commit(s_nvs_handle);
}
