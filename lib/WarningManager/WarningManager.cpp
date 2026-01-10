#include "WarningManager.h"

// NVS namespace and keys
const char* WarningManager::NVS_NAMESPACE = "warnings";
const char* WarningManager::KEY_TANK_TYPE = "tank_type";

WarningManager::WarningManager() {
    // Initialize with default profile
    profile.tank_type = FRESHWATER_COMMUNITY;
    loadFreshwaterCommunityDefaults();

    // Initialize sensor states
    sensorState.temperature.state = STATE_UNKNOWN;
    sensorState.ph.state = STATE_UNKNOWN;
    sensorState.nh3.state = STATE_UNKNOWN;
    sensorState.orp.state = STATE_UNKNOWN;
    sensorState.conductivity.state = STATE_UNKNOWN;
    sensorState.salinity.state = STATE_UNKNOWN;
    sensorState.dissolved_oxygen.state = STATE_UNKNOWN;
}

bool WarningManager::begin() {
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("WarningManager: Failed to open NVS namespace");
        return false;
    }

    loadFromNVS();
    preferences.end();

    Serial.println("WarningManager initialized");
    Serial.print("Tank type: ");
    Serial.println(getTankTypeString(profile.tank_type));

    return true;
}

// ========== Profile Management ==========

void WarningManager::setTankType(TankType type) {
    profile.tank_type = type;

    switch (type) {
        case FRESHWATER_COMMUNITY:
            loadFreshwaterCommunityDefaults();
            break;
        case FRESHWATER_PLANTED:
            loadFreshwaterPlantedDefaults();
            break;
        case SALTWATER_FISH_ONLY:
            loadSaltwaterFishOnlyDefaults();
            break;
        case REEF:
            loadReefDefaults();
            break;
        case CUSTOM_TANK:
            // Keep current thresholds
            break;
    }

    profile.timestamp = millis();
}

void WarningManager::resetToDefaults() {
    setTankType(profile.tank_type);
}

bool WarningManager::saveProfile() {
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        return false;
    }

    saveToNVS();
    preferences.end();

    return true;
}

bool WarningManager::loadProfile() {
    if (!preferences.begin(NVS_NAMESPACE, true)) {
        return false;
    }

    loadFromNVS();
    preferences.end();

    return true;
}

// ========== Default Profiles ==========

void WarningManager::loadFreshwaterCommunityDefaults() {
    // Temperature (°C)
    profile.temperature.warn_low = 20.0;
    profile.temperature.warn_high = 28.0;
    profile.temperature.crit_low = 18.0;
    profile.temperature.crit_high = 30.0;
    profile.temperature.delta_warn_per_hr = 2.0;

    // pH
    profile.ph.warn_low = 6.0;
    profile.ph.warn_high = 8.0;
    profile.ph.crit_low = 5.5;
    profile.ph.crit_high = 8.5;
    profile.ph.delta_warn_per_24h = 0.3;
    profile.ph.delta_crit_per_24h = 0.5;

    // NH3 (toxic ammonia ppm)
    profile.nh3.warn_high = 0.02;
    profile.nh3.crit_high = 0.05;

    // ORP (mV)
    profile.orp.warn_low = 200.0;
    profile.orp.warn_high = 400.0;
    profile.orp.crit_low = 180.0;
    profile.orp.crit_high = 450.0;

    // Conductivity (µS/cm)
    profile.conductivity.warn_low_us_cm = 100.0;
    profile.conductivity.warn_high_us_cm = 600.0;
    profile.conductivity.crit_low_us_cm = 50.0;
    profile.conductivity.crit_high_us_cm = 1200.0;

    // Salinity (not applicable for freshwater)
    profile.salinity.warn_low_psu = 0.0;
    profile.salinity.warn_high_psu = 1.0;
    profile.salinity.crit_low_psu = 0.0;
    profile.salinity.crit_high_psu = 2.0;

    // Dissolved Oxygen (mg/L)
    profile.dissolved_oxygen.warn_low = 6.0;
    profile.dissolved_oxygen.crit_low = 4.0;
}

void WarningManager::loadFreshwaterPlantedDefaults() {
    // Similar to community but optimized for planted tanks
    loadFreshwaterCommunityDefaults();

    // Planted tanks benefit from slightly lower pH and higher conductivity
    profile.ph.warn_low = 6.0;
    profile.ph.warn_high = 7.5;
    profile.ph.crit_low = 5.5;
    profile.ph.crit_high = 8.0;

    // Higher conductivity acceptable for planted tanks
    profile.conductivity.warn_high_us_cm = 1000.0;
    profile.conductivity.crit_high_us_cm = 1500.0;
}

void WarningManager::loadSaltwaterFishOnlyDefaults() {
    // Temperature (°C)
    profile.temperature.warn_low = 22.0;
    profile.temperature.warn_high = 28.0;
    profile.temperature.crit_low = 20.0;
    profile.temperature.crit_high = 30.0;
    profile.temperature.delta_warn_per_hr = 1.5;

    // pH
    profile.ph.warn_low = 7.8;
    profile.ph.warn_high = 8.6;
    profile.ph.crit_low = 7.7;
    profile.ph.crit_high = 8.7;
    profile.ph.delta_warn_per_24h = 0.2;
    profile.ph.delta_crit_per_24h = 0.4;

    // NH3 (saltwater is more sensitive)
    profile.nh3.warn_high = 0.01;
    profile.nh3.crit_high = 0.02;

    // ORP (mV)
    profile.orp.warn_low = 250.0;
    profile.orp.warn_high = 450.0;
    profile.orp.crit_low = 220.0;
    profile.orp.crit_high = 480.0;

    // Conductivity (not primary for saltwater - use salinity)
    profile.conductivity.warn_low_us_cm = 40000.0;
    profile.conductivity.warn_high_us_cm = 60000.0;
    profile.conductivity.crit_low_us_cm = 35000.0;
    profile.conductivity.crit_high_us_cm = 65000.0;

    // Salinity (PSU)
    profile.salinity.warn_low_psu = 33.0;
    profile.salinity.warn_high_psu = 36.0;
    profile.salinity.crit_low_psu = 32.0;
    profile.salinity.crit_high_psu = 37.0;

    // Dissolved Oxygen (mg/L)
    profile.dissolved_oxygen.warn_low = 6.0;
    profile.dissolved_oxygen.crit_low = 4.0;
}

void WarningManager::loadReefDefaults() {
    // Similar to saltwater but more stringent
    loadSaltwaterFishOnlyDefaults();

    // Tighter temperature control for reef
    profile.temperature.warn_low = 24.0;
    profile.temperature.warn_high = 26.0;
    profile.temperature.crit_low = 22.0;
    profile.temperature.crit_high = 28.0;

    // Tighter pH range for reef
    profile.ph.warn_low = 8.1;
    profile.ph.warn_high = 8.4;
    profile.ph.crit_low = 7.9;
    profile.ph.crit_high = 8.6;

    // Higher ORP typical for reef systems
    profile.orp.warn_low = 300.0;
    profile.orp.warn_high = 450.0;
    profile.orp.crit_low = 250.0;
    profile.orp.crit_high = 500.0;

    // Tighter salinity range
    profile.salinity.warn_low_psu = 34.0;
    profile.salinity.warn_high_psu = 35.5;
    profile.salinity.crit_low_psu = 33.0;
    profile.salinity.crit_high_psu = 36.5;
}

// ========== Manual Threshold Setters ==========

void WarningManager::setTemperatureThresholds(float warn_low, float warn_high,
                                              float crit_low, float crit_high) {
    profile.temperature.warn_low = warn_low;
    profile.temperature.warn_high = warn_high;
    profile.temperature.crit_low = crit_low;
    profile.temperature.crit_high = crit_high;
    profile.tank_type = CUSTOM_TANK;
}

void WarningManager::setPHThresholds(float warn_low, float warn_high,
                                    float crit_low, float crit_high) {
    profile.ph.warn_low = warn_low;
    profile.ph.warn_high = warn_high;
    profile.ph.crit_low = crit_low;
    profile.ph.crit_high = crit_high;
    profile.tank_type = CUSTOM_TANK;
}

void WarningManager::setNH3Thresholds(float warn_high, float crit_high) {
    profile.nh3.warn_high = warn_high;
    profile.nh3.crit_high = crit_high;
    profile.tank_type = CUSTOM_TANK;
}

void WarningManager::setORPThresholds(float warn_low, float warn_high,
                                     float crit_low, float crit_high) {
    profile.orp.warn_low = warn_low;
    profile.orp.warn_high = warn_high;
    profile.orp.crit_low = crit_low;
    profile.orp.crit_high = crit_high;
    profile.tank_type = CUSTOM_TANK;
}

void WarningManager::setConductivityThresholds(float warn_low, float warn_high,
                                               float crit_low, float crit_high) {
    profile.conductivity.warn_low_us_cm = warn_low;
    profile.conductivity.warn_high_us_cm = warn_high;
    profile.conductivity.crit_low_us_cm = crit_low;
    profile.conductivity.crit_high_us_cm = crit_high;
    profile.tank_type = CUSTOM_TANK;
}

void WarningManager::setSalinityThresholds(float warn_low, float warn_high,
                                          float crit_low, float crit_high) {
    profile.salinity.warn_low_psu = warn_low;
    profile.salinity.warn_high_psu = warn_high;
    profile.salinity.crit_low_psu = crit_low;
    profile.salinity.crit_high_psu = crit_high;
    profile.tank_type = CUSTOM_TANK;
}

void WarningManager::setDOThresholds(float warn_low, float crit_low) {
    profile.dissolved_oxygen.warn_low = warn_low;
    profile.dissolved_oxygen.crit_low = crit_low;
    profile.tank_type = CUSTOM_TANK;
}

void WarningManager::setTemperatureRateThreshold(float delta_warn_per_hr) {
    profile.temperature.delta_warn_per_hr = delta_warn_per_hr;
    profile.tank_type = CUSTOM_TANK;
}

void WarningManager::setPHRateThresholds(float delta_warn_per_24h, float delta_crit_per_24h) {
    profile.ph.delta_warn_per_24h = delta_warn_per_24h;
    profile.ph.delta_crit_per_24h = delta_crit_per_24h;
    profile.tank_type = CUSTOM_TANK;
}

// ========== Evaluation Functions ==========

WarningState WarningManager::evaluateTemperature(float temp_c) {
    return evaluateAbsolute(temp_c,
                           profile.temperature.warn_low,
                           profile.temperature.warn_high,
                           profile.temperature.crit_low,
                           profile.temperature.crit_high,
                           sensorState.temperature);
}

WarningState WarningManager::evaluatePH(float ph) {
    WarningState state = evaluateAbsolute(ph,
                                         profile.ph.warn_low,
                                         profile.ph.warn_high,
                                         profile.ph.crit_low,
                                         profile.ph.crit_high,
                                         sensorState.ph);

    // Check rate of change (24h threshold converted to per-second)
    if (checkRateOfChange(sensorState.ph, profile.ph.delta_warn_per_24h / 86400.0)) {
        if (state < STATE_WARNING) {
            state = STATE_WARNING;
        }
    }
    if (checkRateOfChange(sensorState.ph, profile.ph.delta_crit_per_24h / 86400.0)) {
        state = STATE_CRITICAL;
    }

    sensorState.ph.state = state;
    return state;
}

WarningState WarningManager::evaluateNH3(float nh3_ppm) {
    // NH3 only has upper thresholds (high is bad)
    return evaluateAbsoluteHighOnly(nh3_ppm,
                                    profile.nh3.warn_high,
                                    profile.nh3.crit_high,
                                    sensorState.nh3);
}

WarningState WarningManager::evaluateORP(float orp_mv) {
    return evaluateAbsolute(orp_mv,
                           profile.orp.warn_low,
                           profile.orp.warn_high,
                           profile.orp.crit_low,
                           profile.orp.crit_high,
                           sensorState.orp);
}

WarningState WarningManager::evaluateConductivity(float ec_us_cm) {
    return evaluateAbsolute(ec_us_cm,
                           profile.conductivity.warn_low_us_cm,
                           profile.conductivity.warn_high_us_cm,
                           profile.conductivity.crit_low_us_cm,
                           profile.conductivity.crit_high_us_cm,
                           sensorState.conductivity);
}

WarningState WarningManager::evaluateSalinity(float salinity_psu) {
    return evaluateAbsolute(salinity_psu,
                           profile.salinity.warn_low_psu,
                           profile.salinity.warn_high_psu,
                           profile.salinity.crit_low_psu,
                           profile.salinity.crit_high_psu,
                           sensorState.salinity);
}

WarningState WarningManager::evaluateDO(float do_mg_l) {
    // DO only has lower thresholds (low is bad)
    return evaluateAbsoluteLowOnly(do_mg_l,
                                   profile.dissolved_oxygen.warn_low,
                                   profile.dissolved_oxygen.crit_low,
                                   sensorState.dissolved_oxygen);
}

// ========== Evaluation Helpers ==========

WarningState WarningManager::evaluateAbsolute(float value, float warn_low, float warn_high,
                                              float crit_low, float crit_high,
                                              MetricState& state) {
    // Update history
    if (state.has_history) {
        state.previous_value = state.current_value;
        state.previous_timestamp = millis();
    }
    state.current_value = value;
    state.has_history = true;

    // Calculate hysteresis bands
    float hyst_low = (warn_low - crit_low) * HYSTERESIS_FACTOR;
    float hyst_high = (crit_high - warn_high) * HYSTERESIS_FACTOR;

    WarningState newState = STATE_NORMAL;

    // Check critical thresholds first
    if (value <= crit_low) {
        newState = STATE_CRITICAL;
    } else if (value >= crit_high) {
        newState = STATE_CRITICAL;
    }
    // Check warning thresholds
    else if (value <= warn_low) {
        newState = STATE_WARNING;
    } else if (value >= warn_high) {
        newState = STATE_WARNING;
    }
    // Apply hysteresis if transitioning from warning/critical to normal
    else if (state.state >= STATE_WARNING) {
        if (value < (warn_low + hyst_low) || value > (warn_high - hyst_high)) {
            newState = state.state; // Stay in current state
        }
    }

    state.state = newState;
    return newState;
}

WarningState WarningManager::evaluateAbsoluteHighOnly(float value, float warn_high,
                                                      float crit_high, MetricState& state) {
    // Update history
    if (state.has_history) {
        state.previous_value = state.current_value;
        state.previous_timestamp = millis();
    }
    state.current_value = value;
    state.has_history = true;

    WarningState newState = STATE_NORMAL;

    if (value >= crit_high) {
        newState = STATE_CRITICAL;
    } else if (value >= warn_high) {
        newState = STATE_WARNING;
    }
    // Apply hysteresis
    else if (state.state >= STATE_WARNING) {
        float hyst = (crit_high - warn_high) * HYSTERESIS_FACTOR;
        if (value > (warn_high - hyst)) {
            newState = state.state;
        }
    }

    state.state = newState;
    return newState;
}

WarningState WarningManager::evaluateAbsoluteLowOnly(float value, float warn_low,
                                                     float crit_low, MetricState& state) {
    // Update history
    if (state.has_history) {
        state.previous_value = state.current_value;
        state.previous_timestamp = millis();
    }
    state.current_value = value;
    state.has_history = true;

    WarningState newState = STATE_NORMAL;

    if (value <= crit_low) {
        newState = STATE_CRITICAL;
    } else if (value <= warn_low) {
        newState = STATE_WARNING;
    }
    // Apply hysteresis
    else if (state.state >= STATE_WARNING) {
        float hyst = (warn_low - crit_low) * HYSTERESIS_FACTOR;
        if (value < (warn_low + hyst)) {
            newState = state.state;
        }
    }

    state.state = newState;
    return newState;
}

bool WarningManager::checkRateOfChange(MetricState& state, float delta_threshold_per_sec) {
    if (!state.has_history) {
        return false;
    }

    unsigned long time_diff_ms = millis() - state.previous_timestamp;
    if (time_diff_ms == 0) {
        return false;
    }

    float time_diff_sec = time_diff_ms / 1000.0;
    float value_diff = abs(state.current_value - state.previous_value);
    float rate = value_diff / time_diff_sec;

    return rate > delta_threshold_per_sec;
}

// ========== Warning Counts ==========

int WarningManager::getWarningCount() const {
    int count = 0;
    if (sensorState.temperature.state == STATE_WARNING) count++;
    if (sensorState.ph.state == STATE_WARNING) count++;
    if (sensorState.nh3.state == STATE_WARNING) count++;
    if (sensorState.orp.state == STATE_WARNING) count++;
    if (sensorState.conductivity.state == STATE_WARNING) count++;
    if (sensorState.salinity.state == STATE_WARNING) count++;
    if (sensorState.dissolved_oxygen.state == STATE_WARNING) count++;
    return count;
}

int WarningManager::getCriticalCount() const {
    int count = 0;
    if (sensorState.temperature.state == STATE_CRITICAL) count++;
    if (sensorState.ph.state == STATE_CRITICAL) count++;
    if (sensorState.nh3.state == STATE_CRITICAL) count++;
    if (sensorState.orp.state == STATE_CRITICAL) count++;
    if (sensorState.conductivity.state == STATE_CRITICAL) count++;
    if (sensorState.salinity.state == STATE_CRITICAL) count++;
    if (sensorState.dissolved_oxygen.state == STATE_CRITICAL) count++;
    return count;
}

// ========== Utility Functions ==========

String WarningManager::getStateString(WarningState state) const {
    switch (state) {
        case STATE_UNKNOWN: return "UNKNOWN";
        case STATE_NORMAL: return "NORMAL";
        case STATE_WARNING: return "WARNING";
        case STATE_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

String WarningManager::getStateColor(WarningState state) const {
    switch (state) {
        case STATE_UNKNOWN: return "#808080";  // Gray
        case STATE_NORMAL: return "#00FF00";    // Green
        case STATE_WARNING: return "#FFA500";   // Orange/Yellow
        case STATE_CRITICAL: return "#FF0000";  // Red
        default: return "#808080";
    }
}

String WarningManager::getTankTypeString(TankType type) const {
    switch (type) {
        case FRESHWATER_COMMUNITY: return "Freshwater Community";
        case FRESHWATER_PLANTED: return "Freshwater Planted";
        case SALTWATER_FISH_ONLY: return "Saltwater Fish-Only";
        case REEF: return "Reef";
        case CUSTOM_TANK: return "Custom";
        default: return "Unknown";
    }
}

// ========== Storage Functions ==========

void WarningManager::saveToNVS() {
    preferences.putUChar(KEY_TANK_TYPE, (uint8_t)profile.tank_type);
    // Store all thresholds as a blob for simplicity
    preferences.putBytes("profile", &profile, sizeof(WarningProfile));
}

void WarningManager::loadFromNVS() {
    if (preferences.isKey(KEY_TANK_TYPE)) {
        uint8_t type = preferences.getUChar(KEY_TANK_TYPE, FRESHWATER_COMMUNITY);

        // Try to load stored profile
        size_t len = preferences.getBytesLength("profile");
        if (len == sizeof(WarningProfile)) {
            preferences.getBytes("profile", &profile, sizeof(WarningProfile));
        } else {
            // No stored profile, load defaults
            setTankType((TankType)type);
        }
    } else {
        // First run, use defaults
        loadFreshwaterCommunityDefaults();
    }
}
