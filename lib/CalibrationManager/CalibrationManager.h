#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * CalibrationManager - Manages pH and EC sensor calibration data
 *
 * pH Calibration:
 * - Supports 1-point (offset only) or 2-point (offset + slope) calibration
 * - Default sensitivity: 52 mV/pH (Nernstian response)
 * - Calibration using standard buffer solutions (pH 4.0, 7.0, 10.0)
 *
 * EC Calibration:
 * - Determines cell constant using known conductivity solution
 * - Typically uses 0.01M KCl solution (1.41 mS/cm @ 25°C)
 * - Cell constant stored in units of /cm
 *
 * Data persisted to ESP32 NVS (Non-Volatile Storage)
 */
class CalibrationManager {
public:
    struct PHCalibration {
        bool isCalibrated;
        float point1_pH;           // First calibration point pH value
        float point1_ugs_mV;       // Measured Ugs voltage at first point (mV)
        bool hasTwoPoints;         // Whether 2-point calibration is active
        float point2_pH;           // Second calibration point pH value
        float point2_ugs_mV;       // Measured Ugs voltage at second point (mV)
        float sensitivity_mV_pH;   // Calculated or default sensitivity (mV/pH)
        unsigned long timestamp;   // Last calibration timestamp
    };

    struct ECCalibration {
        bool isCalibrated;
        float cellConstant_per_cm; // Cell constant in /cm
        float cal_solution_mS_cm;  // Known solution conductivity (mS/cm @ 25°C)
        float cal_temp_C;          // Temperature during calibration
        unsigned long timestamp;   // Last calibration timestamp
    };

    CalibrationManager();

    // Initialize (call in setup)
    bool begin();

    // pH Calibration Methods
    bool calibratePH_1Point(float buffer_pH, float measured_ugs_mV);
    bool calibratePH_2Point(float buffer1_pH, float measured1_ugs_mV,
                            float buffer2_pH, float measured2_ugs_mV);
    void clearPHCalibration();
    PHCalibration getPHCalibration() const { return phCal; }
    float calculatePH(float measured_ugs_mV) const;

    // EC Calibration Methods
    bool calibrateEC(float known_conductivity_mS_cm, float temperature_C,
                     int32_t measured_ec_nA, int32_t measured_ec_uV);
    void clearECCalibration();
    ECCalibration getECCalibration() const { return ecCal; }
    float calculateEC(int32_t ec_nA, int32_t ec_uV, float temperature_C = 25.0) const;

    // Utility methods
    bool hasValidPHCalibration() const { return phCal.isCalibrated; }
    bool hasValidECCalibration() const { return ecCal.isCalibrated; }
    String getPHCalibrationInfo() const;
    String getECCalibrationInfo() const;

private:
    Preferences preferences;
    PHCalibration phCal;
    ECCalibration ecCal;

    // NVS keys
    static const char* NVS_NAMESPACE;
    static const char* KEY_PH_CALIBRATED;
    static const char* KEY_PH_P1_PH;
    static const char* KEY_PH_P1_UGS;
    static const char* KEY_PH_TWO_POINTS;
    static const char* KEY_PH_P2_PH;
    static const char* KEY_PH_P2_UGS;
    static const char* KEY_PH_SENSITIVITY;
    static const char* KEY_PH_TIMESTAMP;
    static const char* KEY_EC_CALIBRATED;
    static const char* KEY_EC_CELL_CONSTANT;
    static const char* KEY_EC_SOLUTION;
    static const char* KEY_EC_TEMP;
    static const char* KEY_EC_TIMESTAMP;

    // Default values
    static constexpr float DEFAULT_PH_SENSITIVITY = 52.0;  // mV/pH (Nernstian)
    static constexpr float DEFAULT_EC_CELL_CONSTANT = 1.0; // /cm

    // Storage methods
    void savePHCalibration();
    void loadPHCalibration();
    void saveECCalibration();
    void loadECCalibration();
};

#endif // CALIBRATION_MANAGER_H
