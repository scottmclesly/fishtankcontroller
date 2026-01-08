# Web Interface Guide

## Overview

The aquarium controller includes a built-in web interface for monitoring sensor readings and performing calibration. The interface is accessible via local network or WiFi Access Point.

## Accessing the Web Interface

### Normal Operation (Connected to WiFi)
- **mDNS:** `http://aquarium.local`
- **Direct IP:** `http://[device-ip-address]`

### Provisioning Mode (First Setup)
- **SSID:** `AquariumController` (or as configured)
- **Password:** `aquarium123` (or as configured)
- **URL:** `http://192.168.4.1`

## Pages

### 1. Dashboard (`/`)

The main monitoring interface showing real-time sensor readings.

#### Features:
- **Live sensor data** (auto-refreshes every 2 seconds)
  - Temperature (°C)
  - ORP (mV)
  - pH (calibrated or uncalibrated)
  - Conductivity (mS/cm, calibrated or uncalibrated)

- **Calibration status indicators**
  - Green "✓ Calibrated" badge when sensor is calibrated
  - Yellow "⚠ Uncalibrated" badge when calibration needed

- **WiFi status**
  - Connected network name
  - Device IP address
  - Signal strength (RSSI)

- **Quick links**
  - 🔬 Calibration - Navigate to calibration interface

#### Calibration Warning Banner
Displays when pH or EC sensors are uncalibrated with direct link to calibration page.

---

### 2. Calibration (`/calibration`)

Comprehensive sensor calibration interface with step-by-step guidance.

#### Current Sensor Readings Card
- Shows live raw and converted sensor values
- **Refresh button** to get latest readings
- Auto-populates calibration fields
- Updates every 5 seconds automatically

**Displayed values:**
- Temperature (°C and raw mC)
- ORP (mV and raw uV)
- pH Ugs voltage (mV and raw uV)
- EC current (nA)
- EC voltage (uV)
- EC resistance (Ω)

#### pH Calibration Card

**Status Indicator:**
- Shows current calibration status (calibrated/uncalibrated)
- Displays calibration type (1-point or 2-point)
- Shows sensitivity value when calibrated

**Procedure Instructions:**
Step-by-step guide for proper pH calibration procedure.

**1-Point Calibration Form:**
- Buffer pH selector (4.0, 7.0, 10.0)
- Measured Ugs voltage input (mV)
- "Calibrate pH (1-Point)" button

**2-Point Calibration Form:**
- Buffer 1 pH selector
- Measured Ugs 1 voltage input
- Buffer 2 pH selector
- Measured Ugs 2 voltage input
- "Calibrate pH (2-Point)" button

**Actions:**
- "Clear pH Calibration" button (with confirmation)

#### EC Calibration Card

**Status Indicator:**
- Shows current calibration status
- Displays cell constant when calibrated

**Procedure Instructions:**
Step-by-step guide for proper EC calibration procedure.

**Calibration Solution Reference:**
- Common solutions listed (0.01M, 0.1M, 1M KCl)
- Expected conductivity values @ 25°C

**Calibration Form:**
- Known conductivity input (mS/cm, default: 1.41)
- Solution temperature input (°C, auto-filled from sensor)
- Measured EC current (nA, auto-filled)
- Measured EC voltage (uV, auto-filled)
- "Calibrate EC" button

**Actions:**
- "Clear EC Calibration" button (with confirmation)

---

### 3. WiFi Setup (`/setup`)

WiFi provisioning interface (automatically shown in AP mode).

#### Features:
- SSID input field
- Password input field
- "Scan for Networks" button
- Network list with signal strength
- Click network to select
- Auto-restart after saving credentials

---

## API Endpoints

### Sensor Data
```
GET /api/sensors
```
Returns JSON with current sensor readings and WiFi status.

**Response:**
```json
{
  "timestamp": 123456,
  "valid": true,
  "temperature_c": 25.5,
  "orp_mv": 350.2,
  "ph": 7.42,
  "ec_ms_cm": 1.234,
  "wifi": {
    "ssid": "MyNetwork",
    "ip": "192.168.1.100",
    "rssi": -45
  }
}
```

### Calibration Status
```
GET /api/calibration/status
```
Returns current calibration data.

**Response:**
```json
{
  "ph": {
    "calibrated": true,
    "two_point": true,
    "point1_pH": 4.0,
    "point1_ugs_mV": 3155.908,
    "point2_pH": 7.0,
    "point2_ugs_mV": 2999.908,
    "sensitivity": 52.0,
    "timestamp": 1234567890
  },
  "ec": {
    "calibrated": true,
    "cell_constant": 1.4100,
    "solution": 1.41,
    "temp": 25.0,
    "timestamp": 1234567890
  }
}
```

### Raw Sensor Readings
```
GET /api/calibration/raw
```
Returns raw sensor values for calibration.

**Response:**
```json
{
  "valid": true,
  "temp_mC": 25500,
  "orp_uV": 350200,
  "ugs_uV": 2999908,
  "ec_nA": 66000,
  "ec_uV": 66000,
  "temp_C": 25.5,
  "orp_mV": 350.2,
  "ugs_mV": 2999.908,
  "ec_resistance_ohm": 1000.0
}
```

### Network Scan
```
GET /scan
```
Scans for available WiFi networks.

**Response:**
```json
{
  "networks": [
    {
      "ssid": "MyNetwork",
      "rssi": -45,
      "encryption": "Secured"
    },
    ...
  ]
}
```

### Save WiFi Credentials
```
POST /save-wifi
Parameters: ssid, password
```
Saves WiFi credentials and restarts device.

### Calibration Operations

**pH 1-Point:**
```
POST /api/calibration/ph/1point
Parameters: buffer_pH, measured_ugs_mV
```

**pH 2-Point:**
```
POST /api/calibration/ph/2point
Parameters: buffer1_pH, measured1_ugs_mV, buffer2_pH, measured2_ugs_mV
```

**EC Calibration:**
```
POST /api/calibration/ec
Parameters: known_conductivity, temperature, measured_ec_nA, measured_ec_uV
```

**Clear Calibrations:**
```
POST /api/calibration/ph/clear
POST /api/calibration/ec/clear
```

---

## User Interface Features

### Auto-Refresh
- Dashboard sensor readings refresh every 2 seconds
- Calibration page readings refresh every 5 seconds
- No page reload required

### Responsive Design
- Mobile-friendly layout
- Touch-friendly buttons and inputs
- Adapts to screen size (cards stack vertically on narrow screens)

### Visual Feedback
- Success messages (green) for successful operations
- Error messages (red) for failures
- Warning messages (yellow) for uncalibrated sensors
- Status indicators with color coding
- Auto-dismiss messages after 5 seconds

### Color Scheme
- Primary: Ocean blue (#006494)
- Background: Light blue (#f0f8ff)
- Success: Green (#d4edda)
- Warning: Yellow (#fff3cd)
- Error: Red (#f8d7da)
- Info: Light blue (#d1ecf1)

### Accessibility
- Clear labels for all inputs
- High contrast text
- Large touch targets
- Confirmation dialogs for destructive actions

---

## Usage Examples

### Example 1: Initial Setup

1. Power on device
2. Connect to "AquariumController" WiFi network
3. Navigate to `http://192.168.4.1`
4. Click "Scan for Networks"
5. Select your network from the list
6. Enter password
7. Click "Connect to WiFi"
8. Wait for device to restart
9. Access via `http://aquarium.local`

### Example 2: pH Calibration (2-Point)

1. Navigate to `http://aquarium.local/calibration`
2. Place sensor in pH 4.0 buffer
3. Wait 1-2 minutes
4. Click "🔄 Refresh Readings"
5. Note the Ugs voltage
6. In "2-Point Calibration" section:
   - Select "pH 4.0" for Buffer 1
   - Enter the Ugs voltage in "Measured Ugs 1" field
7. Rinse sensor and place in pH 7.0 buffer
8. Wait 1-2 minutes
9. Click "🔄 Refresh Readings"
10. Note the new Ugs voltage
11. In "2-Point Calibration" section:
    - Select "pH 7.0" for Buffer 2
    - Enter the Ugs voltage in "Measured Ugs 2" field
12. Click "Calibrate pH (2-Point)"
13. Verify status shows "✓ CALIBRATED (2-point)"

### Example 3: EC Calibration

1. Navigate to `http://aquarium.local/calibration`
2. Prepare 0.01M KCl solution (1.41 mS/cm @ 25°C)
3. Measure solution temperature
4. Place sensor in solution
5. Wait 1-2 minutes
6. Click "🔄 Refresh Readings"
7. Fields auto-populate with current readings
8. Verify "Known Conductivity" shows 1.41 mS/cm
9. Verify "Solution Temperature" matches measured temp
10. Click "Calibrate EC"
11. Verify status shows "✓ CALIBRATED"
12. Note the cell constant displayed

### Example 4: Monitoring from Dashboard

1. Navigate to `http://aquarium.local`
2. View real-time readings (auto-refresh every 2s)
3. Check calibration status badges on pH and EC cards
4. Monitor WiFi connection status
5. Dashboard requires no user interaction - just observe

---

## Troubleshooting

### Cannot Access Web Interface

**Problem:** Cannot reach `http://aquarium.local`
- Check device is powered on and connected to WiFi
- Try direct IP address instead
- Check mDNS is working: `ping aquarium.local`
- Verify you're on the same network

**Problem:** Connection times out
- Device may be in AP mode (first setup)
- Connect to "AquariumController" WiFi and try `http://192.168.4.1`
- Check device serial output for actual IP address

### Calibration Issues

**Problem:** "Refresh Readings" shows old data
- Sensor may not be reading correctly
- Check I2C connections
- Verify sensor is powered
- Check serial output for error messages

**Problem:** Calibration fails
- Verify all required fields are filled
- Check values are reasonable (pH 0-14, conductivity > 0)
- For 2-point pH: ensure buffers are at least 3 pH units apart
- Check serial output for detailed error messages

### WiFi Issues

**Problem:** Cannot connect to device AP
- Verify AP credentials match those configured
- Check device is in AP mode (LED indicator if present)
- Try forgetting network and reconnecting
- Restart device

**Problem:** Device won't connect to WiFi
- Verify SSID and password are correct
- Check 2.4GHz WiFi is available (ESP32-C3 doesn't support 5GHz)
- Ensure network is not hidden
- Check router MAC filtering

---

## Security Considerations

### Current Implementation
- No authentication required
- HTTP only (no HTTPS)
- Local network access only

### Best Practices
- Keep device on isolated VLAN if possible
- Use strong WiFi password
- Monitor network for unauthorized access
- Consider implementing authentication for production use

### Future Enhancements
- User authentication
- HTTPS support
- API token-based access
- Access logging

---

## Performance

### Typical Response Times
- Page load: 200-500ms
- API requests: 50-200ms
- Sensor refresh: 2.8s (full measurement cycle)

### Browser Compatibility
- Modern browsers (Chrome, Firefox, Safari, Edge)
- Mobile browsers (iOS Safari, Android Chrome)
- JavaScript required for dynamic features

### Network Requirements
- Minimal bandwidth (~1KB/s for auto-refresh)
- Works on standard WiFi (2.4GHz)
- Low latency required for good user experience (<100ms)
