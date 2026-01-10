#ifndef WARNING_MANAGER_H
#define WARNING_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * WarningManager - Manages aquarium parameter warning thresholds and evaluation
 *
 * Provides species-aware thresholds with visual warning states:
 * - NORMAL: Parameter within safe range
 * - WARNING: Parameter approaching unsafe levels (yellow alert)
 * - CRITICAL: Parameter at dangerous levels (red alert)
 *
 * Features:
 * - Tank type-specific default profiles (freshwater, saltwater, reef)
 * - Absolute threshold checking
 * - Rate-of-change monitoring
 * - Hysteresis to prevent warning flicker
 * - Persistent configuration in NVS
 */

// Tank type enumeration
enum TankType {
    FRESHWATER_COMMUNITY = 0,
    FRESHWATER_PLANTED = 1,
    SALTWATER_FISH_ONLY = 2,
    REEF = 3,
    CUSTOM_TANK = 4
};

// Warning state enumeration
enum WarningState {
    STATE_UNKNOWN = 0,    // No data or sensor invalid
    STATE_NORMAL = 1,     // Within safe range
    STATE_WARNING = 2,    // Approaching unsafe levels
    STATE_CRITICAL = 3    // Dangerous levels
};

// Temperature thresholds (°C)
struct TemperatureThresholds {
    float warn_low;
    float warn_high;
    float crit_low;
    float crit_high;
    float delta_warn_per_hr;  // Rate of change warning
};

// pH thresholds
struct PHThresholds {
    float warn_low;
    float warn_high;
    float crit_low;
    float crit_high;
    float delta_warn_per_24h;  // Rate of change warning
    float delta_crit_per_24h;  // Rate of change critical
};

// NH3 (toxic ammonia) thresholds (ppm)
struct NH3Thresholds {
    float warn_high;
    float crit_high;
};

// ORP thresholds (mV)
struct ORPThresholds {
    float warn_low;
    float warn_high;
    float crit_low;
    float crit_high;
};

// Conductivity/TDS thresholds
struct ConductivityThresholds {
    float warn_low_us_cm;   // µS/cm for freshwater
    float warn_high_us_cm;
    float crit_low_us_cm;
    float crit_high_us_cm;
};

// Salinity thresholds (PSU - for saltwater)
struct SalinityThresholds {
    float warn_low_psu;
    float warn_high_psu;
    float crit_low_psu;
    float crit_high_psu;
};

// Dissolved Oxygen thresholds (mg/L)
struct DOThresholds {
    float warn_low;
    float crit_low;
};

// Complete warning profile
struct WarningProfile {
    TankType tank_type;
    TemperatureThresholds temperature;
    PHThresholds ph;
    NH3Thresholds nh3;
    ORPThresholds orp;
    ConductivityThresholds conductivity;
    SalinityThresholds salinity;
    DOThresholds dissolved_oxygen;
    unsigned long timestamp;
};

// Current metric state with history for rate-of-change
struct MetricState {
    WarningState state;
    float current_value;
    float previous_value;
    unsigned long previous_timestamp;
    bool has_history;
};

// Complete sensor state
struct SensorWarningState {
    MetricState temperature;
    MetricState ph;
    MetricState nh3;
    MetricState orp;
    MetricState conductivity;
    MetricState salinity;
    MetricState dissolved_oxygen;
};

class WarningManager {
public:
    WarningManager();

    // Initialize (call in setup)
    bool begin();

    // Profile management
    void setTankType(TankType type);
    TankType getTankType() const { return profile.tank_type; }
    WarningProfile getProfile() const { return profile; }
    bool saveProfile();
    bool loadProfile();
    void resetToDefaults();

    // Manual threshold setters (for custom profiles)
    void setTemperatureThresholds(float warn_low, float warn_high, float crit_low, float crit_high);
    void setPHThresholds(float warn_low, float warn_high, float crit_low, float crit_high);
    void setNH3Thresholds(float warn_high, float crit_high);
    void setORPThresholds(float warn_low, float warn_high, float crit_low, float crit_high);
    void setConductivityThresholds(float warn_low, float warn_high, float crit_low, float crit_high);
    void setSalinityThresholds(float warn_low, float warn_high, float crit_low, float crit_high);
    void setDOThresholds(float warn_low, float crit_low);

    // Rate-of-change threshold setters
    void setTemperatureRateThreshold(float delta_warn_per_hr);
    void setPHRateThresholds(float delta_warn_per_24h, float delta_crit_per_24h);

    // Evaluation functions
    WarningState evaluateTemperature(float temp_c);
    WarningState evaluatePH(float ph);
    WarningState evaluateNH3(float nh3_ppm);
    WarningState evaluateORP(float orp_mv);
    WarningState evaluateConductivity(float ec_us_cm);
    WarningState evaluateSalinity(float salinity_psu);
    WarningState evaluateDO(float do_mg_l);

    // Get current sensor states
    SensorWarningState getSensorState() const { return sensorState; }

    // Get warning counts
    int getWarningCount() const;
    int getCriticalCount() const;

    // Utility functions
    String getStateString(WarningState state) const;
    String getStateColor(WarningState state) const;
    String getTankTypeString(TankType type) const;

private:
    Preferences preferences;
    WarningProfile profile;
    SensorWarningState sensorState;

    // Hysteresis factor (5% to prevent oscillation)
    static constexpr float HYSTERESIS_FACTOR = 0.05;

    // NVS keys
    static const char* NVS_NAMESPACE;
    static const char* KEY_TANK_TYPE;

    // Default profiles
    void loadFreshwaterCommunityDefaults();
    void loadFreshwaterPlantedDefaults();
    void loadSaltwaterFishOnlyDefaults();
    void loadReefDefaults();

    // Evaluation helpers
    WarningState evaluateAbsolute(float value, float warn_low, float warn_high,
                                  float crit_low, float crit_high, MetricState& state);
    WarningState evaluateAbsoluteHighOnly(float value, float warn_high, float crit_high,
                                          MetricState& state);
    WarningState evaluateAbsoluteLowOnly(float value, float warn_low, float crit_low,
                                         MetricState& state);
    bool checkRateOfChange(MetricState& state, float delta_threshold_per_sec);

    // Storage methods
    void saveToNVS();
    void loadFromNVS();
};

#endif // WARNING_MANAGER_H
