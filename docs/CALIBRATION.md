# Sensor Calibration Guide

## Overview

The POET sensor requires calibration for accurate pH and EC (electrical conductivity) measurements. This guide explains the calibration procedure and how to use the web-based calibration interface.

## Calibration Theory

### pH Calibration

The POET pH sensor uses an ISFET (Ion-Selective Field-Effect Transistor) with a Nernstian response of approximately **52 mV/pH**. The sensor measures the gate-source voltage (Ugs) which varies with pH.

**Formula:** `pH = buffer_pH + (sample_Ugs_mV - buffer_Ugs_mV) / sensitivity`

#### 1-Point Calibration (Offset Only)
- Determines the pH offset at one known buffer solution
- Uses default Nernstian sensitivity (52 mV/pH)
- Good for routine calibration maintenance
- **Recommended buffer:** pH 7.0

#### 2-Point Calibration (Offset + Slope)
- Determines both offset and actual probe sensitivity
- Accounts for probe aging and temperature effects
- Provides best accuracy across full pH range
- **Recommended buffers:** pH 4.0 and pH 7.0 (or pH 7.0 and pH 10.0)

**Note:** Each probe has unique offset and sensitivity that may drift over time. Regular recalibration is recommended.

### EC Calibration

The EC sensor uses a 4-electrode Kelvin measurement to determine solution conductivity. Calibration determines the **cell constant (K)** in units of /cm.

**Measurement:** The sensor returns current (nA) and voltage (uV), from which resistance is calculated:
- `R = V / I`

**Calibration:** In a known conductivity solution:
- `K = R × EC_known`

**Measurement:** After calibration:
- `EC = K / R × 1000` (mS/cm)

#### Common Calibration Solutions

| Solution | Conductivity @ 25°C | Use Case |
|----------|---------------------|----------|
| 0.01M KCl | 1.41 mS/cm | Freshwater aquariums |
| 0.1M KCl | 12.88 mS/cm | Brackish water |
| 1M KCl | 111.9 mS/cm | Saltwater, high conductivity |

**Temperature Coefficient:** Conductivity changes ~2%/°C. Always measure and record solution temperature during calibration.

## Calibration Procedure

### Accessing the Calibration Interface

1. Connect to your aquarium controller:
   - **Local network:** `http://aquarium.local/calibration`
   - **Direct IP:** `http://[device-ip]/calibration`
   - **From dashboard:** Click the "🔬 Calibration" link

### pH Calibration Steps

#### Materials Needed
- pH buffer solutions (pH 4.0, 7.0, and/or 10.0)
- Distilled water for rinsing
- Soft tissue for drying

#### Procedure

1. **Prepare the sensor:**
   - Rinse the pH sensor with distilled water
   - Gently pat dry with soft tissue (do not rub ISFET surface)

2. **First buffer measurement:**
   - Immerse sensor completely in first buffer solution (e.g., pH 7.0)
   - Wait 1-2 minutes for reading to stabilize
   - Click "🔄 Refresh Readings" on calibration page
   - Note the **Ugs (mV)** value displayed

3. **Enter calibration data:**
   - For **1-point calibration:**
     - Select buffer pH from dropdown (4.0, 7.0, or 10.0)
     - Enter measured Ugs voltage
     - Click "Calibrate pH (1-Point)"

   - For **2-point calibration** (recommended):
     - Select first buffer pH and enter its Ugs voltage
     - Rinse sensor with distilled water and pat dry
     - Immerse in second buffer solution (at least 3 pH units apart)
     - Wait 1-2 minutes and refresh readings
     - Select second buffer pH and enter its Ugs voltage
     - Click "Calibrate pH (2-Point)"

4. **Verify calibration:**
   - Status should show "✓ CALIBRATED"
   - Sensitivity should be displayed (typically 50-54 mV/pH)
   - Test in known buffer to verify accuracy

### EC Calibration Steps

#### Materials Needed
- Known conductivity calibration solution (e.g., 0.01M KCl = 1.41 mS/cm)
- Accurate thermometer
- Distilled water for rinsing
- Soft tissue for drying

#### Procedure

1. **Prepare the sensor:**
   - Rinse the EC sensor with distilled water
   - Gently pat dry with soft tissue

2. **Calibration solution measurement:**
   - Immerse sensor completely in calibration solution
   - Measure solution temperature accurately
   - Wait 1-2 minutes for reading to stabilize
   - Click "🔄 Refresh Readings" on calibration page
   - EC fields will auto-populate with current measurements

3. **Enter calibration data:**
   - Verify/enter known conductivity (e.g., 1.41 mS/cm)
   - Verify/enter measured temperature
   - Verify EC current (nA) and voltage (uV) values
   - Click "Calibrate EC"

4. **Verify calibration:**
   - Status should show "✓ CALIBRATED"
   - Cell constant should be displayed (typically 0.5-2.0 /cm)
   - Expected value for typical probe: ~1.0-1.5 /cm

## Data Storage

Calibration data is stored in ESP32 Non-Volatile Storage (NVS) and persists across power cycles and firmware updates.

### Stored Data

**pH Calibration:**
- Calibration status (calibrated/uncalibrated)
- Number of points (1 or 2)
- Buffer pH values
- Measured Ugs voltages
- Calculated sensitivity (mV/pH)
- Calibration timestamp

**EC Calibration:**
- Calibration status (calibrated/uncalibrated)
- Cell constant (K, in /cm)
- Calibration solution conductivity
- Calibration temperature
- Calibration timestamp

## Calibration Maintenance

### When to Calibrate

**Initial Setup:**
- Calibrate both pH and EC sensors before first use

**Routine Maintenance:**
- **pH:** Recalibrate every 2-4 weeks for critical applications, monthly for monitoring
- **EC:** Recalibrate every 1-3 months, or when values seem incorrect

**After Events:**
- Probe cleaning or maintenance
- Extended dry storage
- Significant temperature changes in application
- Suspected measurement drift

### Calibration Best Practices

1. **Use fresh buffer solutions:**
   - Replace buffers every 3-6 months
   - Discard buffer after multiple uses (contamination risk)
   - Store buffers in sealed containers

2. **Temperature considerations:**
   - Calibrate at or near operating temperature
   - Allow buffers to equilibrate to room temperature
   - Record temperature during EC calibration

3. **Sensor care:**
   - Never let pH sensor dry out completely (store wet)
   - Keep EC electrodes clean and free from deposits
   - Handle ISFET surface carefully (no rubbing)

4. **Verification:**
   - Test calibration in a second known buffer
   - Compare readings to reference meter if available

## API Reference

### REST Endpoints

All calibration operations are available via REST API:

#### Get Calibration Status
```
GET /api/calibration/status
```
Returns current calibration data for pH and EC sensors.

#### Get Raw Readings
```
GET /api/calibration/raw
```
Returns current raw sensor readings (mC, uV, nA) and converted values.

#### pH 1-Point Calibration
```
POST /api/calibration/ph/1point
Parameters:
  - buffer_pH: float (4.0, 7.0, or 10.0)
  - measured_ugs_mV: float
```

#### pH 2-Point Calibration
```
POST /api/calibration/ph/2point
Parameters:
  - buffer1_pH: float
  - measured1_ugs_mV: float
  - buffer2_pH: float
  - measured2_ugs_mV: float
```

#### EC Calibration
```
POST /api/calibration/ec
Parameters:
  - known_conductivity: float (mS/cm)
  - temperature: float (°C)
  - measured_ec_nA: int32
  - measured_ec_uV: int32
```

#### Clear Calibrations
```
POST /api/calibration/ph/clear
POST /api/calibration/ec/clear
```

## Troubleshooting

### pH Calibration Issues

**Problem:** Sensitivity far from 52 mV/pH (e.g., <40 or >60)
- **Cause:** Probe aging, damage, or temperature effects
- **Solution:** Clean probe, try recalibration, consider probe replacement if >3 years old

**Problem:** Readings drift quickly
- **Cause:** Probe contamination or dehydration
- **Solution:** Clean probe, ensure proper storage in storage solution

**Problem:** Large offset (>±200 mV from expected)
- **Cause:** Probe reference drift, contamination
- **Solution:** Clean probe, recalibrate, verify buffer quality

### EC Calibration Issues

**Problem:** Cell constant far from expected (~1.0 /cm)
- **Cause:** Electrode fouling or damaged probe
- **Solution:** Clean electrodes thoroughly, check for damage

**Problem:** Unstable readings
- **Cause:** Air bubbles, incomplete sensor immersion, or stirring
- **Solution:** Ensure complete immersion, allow to settle, avoid movement

**Problem:** Temperature compensation not working
- **Cause:** Incorrect temperature entry
- **Solution:** Verify temperature measurement, enter accurate value

## Implementation Details

### Files

- **`lib/CalibrationManager/CalibrationManager.h`** - Calibration manager header
- **`lib/CalibrationManager/CalibrationManager.cpp`** - Calibration logic and NVS storage
- **`lib/WebServer/WebServer.h`** - Web server with calibration endpoints
- **`lib/WebServer/WebServer.cpp`** - Calibration page and API handlers
- **`src/main.cpp`** - Integration with main firmware

### Formulas Used

**pH Calculation:**
```cpp
pH = buffer_pH + (measured_ugs_mV - buffer_ugs_mV) / sensitivity_mV_pH
```

**2-Point Sensitivity Calculation:**
```cpp
sensitivity = (ugs2_mV - ugs1_mV) / (pH2 - pH1)
```

**EC Cell Constant Calculation:**
```cpp
R = ec_uV / ec_nA  // Resistance in Ohms
K = R × (conductivity_mS_cm / 1000.0)  // Cell constant in /cm
```

**EC Measurement:**
```cpp
R = ec_uV / ec_nA
EC_mS_cm = (K / R) × 1000.0
```

## References

- **POET Sensor Documentation:** `docs/Sonetron ConeFET I2C protocol.pdf`
- **Nernst Equation:** Theoretical basis for pH electrode response
- **Conductivity Theory:** 4-electrode Kelvin measurement principles
- **ESP32 Preferences Library:** Non-volatile storage implementation
