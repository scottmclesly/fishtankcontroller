#include "CalibrationManager.h"

// NVS namespace and keys
const char* CalibrationManager::NVS_NAMESPACE = "calibration";
const char* CalibrationManager::KEY_PH_CALIBRATED = "ph_cal";
const char* CalibrationManager::KEY_PH_P1_PH = "ph_p1_ph";
const char* CalibrationManager::KEY_PH_P1_UGS = "ph_p1_ugs";
const char* CalibrationManager::KEY_PH_TWO_POINTS = "ph_2pt";
const char* CalibrationManager::KEY_PH_P2_PH = "ph_p2_ph";
const char* CalibrationManager::KEY_PH_P2_UGS = "ph_p2_ugs";
const char* CalibrationManager::KEY_PH_SENSITIVITY = "ph_sens";
const char* CalibrationManager::KEY_PH_TIMESTAMP = "ph_ts";
const char* CalibrationManager::KEY_EC_CALIBRATED = "ec_cal";
const char* CalibrationManager::KEY_EC_CELL_CONSTANT = "ec_k";
const char* CalibrationManager::KEY_EC_SOLUTION = "ec_sol";
const char* CalibrationManager::KEY_EC_TEMP = "ec_temp";
const char* CalibrationManager::KEY_EC_TIMESTAMP = "ec_ts";

CalibrationManager::CalibrationManager() {
    // Initialize with default uncalibrated state
    phCal.isCalibrated = false;
    phCal.point1_pH = 7.0;
    phCal.point1_ugs_mV = 0.0;
    phCal.hasTwoPoints = false;
    phCal.point2_pH = 0.0;
    phCal.point2_ugs_mV = 0.0;
    phCal.sensitivity_mV_pH = DEFAULT_PH_SENSITIVITY;
    phCal.timestamp = 0;

    ecCal.isCalibrated = false;
    ecCal.cellConstant_per_cm = DEFAULT_EC_CELL_CONSTANT;
    ecCal.cal_solution_mS_cm = 0.0;
    ecCal.cal_temp_C = 25.0;
    ecCal.timestamp = 0;
}

bool CalibrationManager::begin() {
    // Open NVS in read-write mode
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("ERROR: Failed to open NVS for calibration data");
        return false;
    }

    // Load existing calibration data
    loadPHCalibration();
    loadECCalibration();

    Serial.println("CalibrationManager initialized");
    Serial.println(getPHCalibrationInfo());
    Serial.println(getECCalibrationInfo());

    return true;
}

// ============================================================================
// pH Calibration Methods
// ============================================================================

bool CalibrationManager::calibratePH_1Point(float buffer_pH, float measured_ugs_mV) {
    Serial.println("\n=== pH 1-Point Calibration ===");
    Serial.print("Buffer pH: ");
    Serial.println(buffer_pH, 2);
    Serial.print("Measured Ugs: ");
    Serial.print(measured_ugs_mV, 2);
    Serial.println(" mV");

    phCal.isCalibrated = true;
    phCal.point1_pH = buffer_pH;
    phCal.point1_ugs_mV = measured_ugs_mV;
    phCal.hasTwoPoints = false;
    phCal.sensitivity_mV_pH = DEFAULT_PH_SENSITIVITY;  // Use default Nernstian slope
    phCal.timestamp = millis();

    savePHCalibration();

    Serial.println("✓ pH calibration saved (1-point, offset only)");
    Serial.print("  Using default sensitivity: ");
    Serial.print(phCal.sensitivity_mV_pH, 2);
    Serial.println(" mV/pH");

    return true;
}

bool CalibrationManager::calibratePH_2Point(float buffer1_pH, float measured1_ugs_mV,
                                             float buffer2_pH, float measured2_ugs_mV) {
    Serial.println("\n=== pH 2-Point Calibration ===");
    Serial.print("Buffer 1: pH ");
    Serial.print(buffer1_pH, 2);
    Serial.print(", Ugs ");
    Serial.print(measured1_ugs_mV, 2);
    Serial.println(" mV");
    Serial.print("Buffer 2: pH ");
    Serial.print(buffer2_pH, 2);
    Serial.print(", Ugs ");
    Serial.print(measured2_ugs_mV, 2);
    Serial.println(" mV");

    // Calculate actual sensitivity from two points
    float delta_pH = buffer2_pH - buffer1_pH;
    float delta_ugs_mV = measured2_ugs_mV - measured1_ugs_mV;

    if (abs(delta_pH) < 0.1) {
        Serial.println("ERROR: pH values too close together");
        return false;
    }

    float sensitivity = delta_ugs_mV / delta_pH;

    Serial.print("Calculated sensitivity: ");
    Serial.print(sensitivity, 2);
    Serial.println(" mV/pH");

    phCal.isCalibrated = true;
    phCal.point1_pH = buffer1_pH;
    phCal.point1_ugs_mV = measured1_ugs_mV;
    phCal.hasTwoPoints = true;
    phCal.point2_pH = buffer2_pH;
    phCal.point2_ugs_mV = measured2_ugs_mV;
    phCal.sensitivity_mV_pH = sensitivity;
    phCal.timestamp = millis();

    savePHCalibration();

    Serial.println("✓ pH calibration saved (2-point, offset + slope)");

    return true;
}

void CalibrationManager::clearPHCalibration() {
    Serial.println("Clearing pH calibration...");

    phCal.isCalibrated = false;
    phCal.point1_pH = 7.0;
    phCal.point1_ugs_mV = 0.0;
    phCal.hasTwoPoints = false;
    phCal.sensitivity_mV_pH = DEFAULT_PH_SENSITIVITY;
    phCal.timestamp = 0;

    savePHCalibration();

    Serial.println("✓ pH calibration cleared");
}

float CalibrationManager::calculatePH(float measured_ugs_mV) const {
    if (!phCal.isCalibrated) {
        // Uncalibrated - assume pH 7.0 at 0mV
        return 7.0 + (measured_ugs_mV / DEFAULT_PH_SENSITIVITY);
    }

    // Use calibrated offset and sensitivity
    // Formula: pH = buffer_pH + (measured_ugs_mV - buffer_ugs_mV) / sensitivity
    return phCal.point1_pH + (measured_ugs_mV - phCal.point1_ugs_mV) / phCal.sensitivity_mV_pH;
}

// ============================================================================
// EC Calibration Methods
// ============================================================================

bool CalibrationManager::calibrateEC(float known_conductivity_mS_cm, float temperature_C,
                                      int32_t measured_ec_nA, int32_t measured_ec_uV) {
    Serial.println("\n=== EC Calibration ===");
    Serial.print("Known solution: ");
    Serial.print(known_conductivity_mS_cm, 3);
    Serial.print(" mS/cm @ ");
    Serial.print(temperature_C, 1);
    Serial.println(" °C");
    Serial.print("Measured: ");
    Serial.print(measured_ec_nA);
    Serial.print(" nA, ");
    Serial.print(measured_ec_uV);
    Serial.println(" uV");

    if (measured_ec_nA == 0) {
        Serial.println("ERROR: Invalid EC measurement (current = 0)");
        return false;
    }

    // Calculate resistance: R = V / I
    float resistance_ohm = (float)measured_ec_uV / (float)measured_ec_nA;

    Serial.print("Calculated resistance: ");
    Serial.print(resistance_ohm, 1);
    Serial.println(" Ohm");

    // Calculate cell constant: K = R * EC
    // where EC is in S/cm, so convert from mS/cm
    float cellConstant = resistance_ohm * (known_conductivity_mS_cm / 1000.0);

    Serial.print("Calculated cell constant: ");
    Serial.print(cellConstant, 4);
    Serial.println(" /cm");

    ecCal.isCalibrated = true;
    ecCal.cellConstant_per_cm = cellConstant;
    ecCal.cal_solution_mS_cm = known_conductivity_mS_cm;
    ecCal.cal_temp_C = temperature_C;
    ecCal.timestamp = millis();

    saveECCalibration();

    Serial.println("✓ EC calibration saved");

    return true;
}

void CalibrationManager::clearECCalibration() {
    Serial.println("Clearing EC calibration...");

    ecCal.isCalibrated = false;
    ecCal.cellConstant_per_cm = DEFAULT_EC_CELL_CONSTANT;
    ecCal.cal_solution_mS_cm = 0.0;
    ecCal.cal_temp_C = 25.0;
    ecCal.timestamp = 0;

    saveECCalibration();

    Serial.println("✓ EC calibration cleared");
}

float CalibrationManager::calculateEC(int32_t ec_nA, int32_t ec_uV, float temperature_C) const {
    if (ec_nA == 0) return 0.0;

    // Calculate resistance: R = V / I
    float resistance_ohm = (float)ec_uV / (float)ec_nA;

    // Calculate conductivity: EC (S/cm) = K / R
    // Convert to mS/cm: * 1000
    float cellConstant = ecCal.isCalibrated ? ecCal.cellConstant_per_cm : DEFAULT_EC_CELL_CONSTANT;
    float ec_mS_cm = (cellConstant / resistance_ohm) * 1000.0;

    // Note: Temperature compensation could be added here
    // For now, we just return the raw calculated value
    // Temperature coefficient is typically ~2%/°C for most solutions

    return ec_mS_cm;
}

// ============================================================================
// Utility Methods
// ============================================================================

String CalibrationManager::getPHCalibrationInfo() const {
    String info = "pH Calibration: ";
    if (phCal.isCalibrated) {
        info += "CALIBRATED (";
        info += phCal.hasTwoPoints ? "2-point" : "1-point";
        info += ")\n";
        info += "  Point 1: pH " + String(phCal.point1_pH, 2) + " @ " + String(phCal.point1_ugs_mV, 2) + " mV\n";
        if (phCal.hasTwoPoints) {
            info += "  Point 2: pH " + String(phCal.point2_pH, 2) + " @ " + String(phCal.point2_ugs_mV, 2) + " mV\n";
        }
        info += "  Sensitivity: " + String(phCal.sensitivity_mV_pH, 2) + " mV/pH";
    } else {
        info += "NOT CALIBRATED (using defaults)";
    }
    return info;
}

String CalibrationManager::getECCalibrationInfo() const {
    String info = "EC Calibration: ";
    if (ecCal.isCalibrated) {
        info += "CALIBRATED\n";
        info += "  Cell constant: " + String(ecCal.cellConstant_per_cm, 4) + " /cm\n";
        info += "  Solution: " + String(ecCal.cal_solution_mS_cm, 3) + " mS/cm @ " + String(ecCal.cal_temp_C, 1) + " °C";
    } else {
        info += "NOT CALIBRATED (using default K = 1.0 /cm)";
    }
    return info;
}

// ============================================================================
// Storage Methods
// ============================================================================

void CalibrationManager::savePHCalibration() {
    preferences.putBool(KEY_PH_CALIBRATED, phCal.isCalibrated);
    preferences.putFloat(KEY_PH_P1_PH, phCal.point1_pH);
    preferences.putFloat(KEY_PH_P1_UGS, phCal.point1_ugs_mV);
    preferences.putBool(KEY_PH_TWO_POINTS, phCal.hasTwoPoints);
    preferences.putFloat(KEY_PH_P2_PH, phCal.point2_pH);
    preferences.putFloat(KEY_PH_P2_UGS, phCal.point2_ugs_mV);
    preferences.putFloat(KEY_PH_SENSITIVITY, phCal.sensitivity_mV_pH);
    preferences.putULong(KEY_PH_TIMESTAMP, phCal.timestamp);
}

void CalibrationManager::loadPHCalibration() {
    phCal.isCalibrated = preferences.getBool(KEY_PH_CALIBRATED, false);
    phCal.point1_pH = preferences.getFloat(KEY_PH_P1_PH, 7.0);
    phCal.point1_ugs_mV = preferences.getFloat(KEY_PH_P1_UGS, 0.0);
    phCal.hasTwoPoints = preferences.getBool(KEY_PH_TWO_POINTS, false);
    phCal.point2_pH = preferences.getFloat(KEY_PH_P2_PH, 0.0);
    phCal.point2_ugs_mV = preferences.getFloat(KEY_PH_P2_UGS, 0.0);
    phCal.sensitivity_mV_pH = preferences.getFloat(KEY_PH_SENSITIVITY, DEFAULT_PH_SENSITIVITY);
    phCal.timestamp = preferences.getULong(KEY_PH_TIMESTAMP, 0);
}

void CalibrationManager::saveECCalibration() {
    preferences.putBool(KEY_EC_CALIBRATED, ecCal.isCalibrated);
    preferences.putFloat(KEY_EC_CELL_CONSTANT, ecCal.cellConstant_per_cm);
    preferences.putFloat(KEY_EC_SOLUTION, ecCal.cal_solution_mS_cm);
    preferences.putFloat(KEY_EC_TEMP, ecCal.cal_temp_C);
    preferences.putULong(KEY_EC_TIMESTAMP, ecCal.timestamp);
}

void CalibrationManager::loadECCalibration() {
    ecCal.isCalibrated = preferences.getBool(KEY_EC_CALIBRATED, false);
    ecCal.cellConstant_per_cm = preferences.getFloat(KEY_EC_CELL_CONSTANT, DEFAULT_EC_CELL_CONSTANT);
    ecCal.cal_solution_mS_cm = preferences.getFloat(KEY_EC_SOLUTION, 0.0);
    ecCal.cal_temp_C = preferences.getFloat(KEY_EC_TEMP, 25.0);
    ecCal.timestamp = preferences.getULong(KEY_EC_TIMESTAMP, 0);
}
