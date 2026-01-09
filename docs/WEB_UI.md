# Web UI Guide

## Overview

The Fish Tank Controller hosts a complete web interface accessible from any device on your network. No app installation required - just open a browser.

## Access Methods

- **mDNS Hostname:** `http://aquarium.local`
- **Direct IP:** `http://192.168.1.100` (check serial monitor for actual IP)
- **Provisioning Mode:** `http://192.168.4.1` (when in AP mode)

## Pages and Features

### Dashboard (Main Page)

**Live Sensor Monitoring:**
- Real-time display of primary sensors:
  - Temperature (°C)
  - ORP - Oxidation-Reduction Potential (mV)
  - pH
  - EC - Electrical Conductivity (mS/cm)

**Derived Water Quality Metrics:**
- **TDS** (Total Dissolved Solids) in ppm
- **CO₂** (Dissolved CO₂) in ppm with color-coded status
  - Green: 15-30 ppm (optimal)
  - Yellow: <15 ppm (low)
  - Red: >30 ppm (high)
- **NH₃ Ratio** (Toxic ammonia percentage)
- **Max DO** (Maximum dissolved oxygen saturation) in mg/L
- **Stocking Density** in cm/L with color-coded warnings
  - Green: <1 cm/L (lightly stocked)
  - Yellow: 1-2 cm/L (moderately stocked)
  - Red: >2 cm/L (heavily stocked)

**System Information:**
- WiFi connection status and signal strength
- MQTT connection status (if enabled)
- Calibration warnings for uncalibrated sensors
- Auto-refresh every 2 seconds

**Responsive Design:**
- Works on desktop, tablet, and mobile devices
- Dark/light theme toggle (synced across all pages)

### Charts Page

**Historical Data Visualization:**
- 288-point circular buffer (24 minutes at 5-second intervals)
- Real-time Chart.js visualization with time-based X-axis
- Individual charts for each metric with appropriate scaling

**View Toggle Buttons:**
- **All Metrics:** Show both primary sensors and derived metrics
- **Primary Sensors:** Temperature, ORP, pH, EC only
- **Derived Metrics:** TDS, CO₂, NH₃ ratio, max DO, stocking density only

**Data Export:**
- **Export CSV** - Download data in CSV format for Excel/Sheets
- **Export JSON** - Download data in JSON format for scripting
- Files include timestamp in filename (e.g., `aquarium-data-2026-01-08T12-34-56.csv`)
- Exports include all primary sensors and derived metrics

**Access:** `http://aquarium.local/charts`

### Calibration & Settings Page

**Tab-based interface with three sections:**

#### Sensor Calibration Tab
- Real-time raw sensor readings for reference during calibration
- **pH Calibration:**
  - 1-point calibration (offset only)
  - 2-point calibration (offset + slope, recommended)
  - Uses standard buffer solutions (pH 4.0, 7.0, 10.0)
- **EC Calibration:**
  - Cell constant determination
  - Uses known conductivity solutions (0.01M, 0.1M, or 1M KCl)
  - Automatic temperature compensation
- Calibration status indicators
- All calibrations persist in NVS (non-volatile storage)

#### Tank Settings Tab
- **Tank Configuration:**
  - Shape selection: Rectangle, Cube, Cylinder, Custom
  - Automatic volume calculation from dimensions
  - Manual volume override for irregular shapes
- **Water Chemistry Parameters:**
  - KH (Carbonate Hardness) in dKH - for CO₂ calculations
  - TAN (Total Ammonia Nitrogen) in ppm - for NH₃ calculations
  - TDS Conversion Factor (0.5-0.7) - freshwater vs saltwater
- **Fish Profile Management:**
  - Add up to 10 fish species
  - Track count and average length for each species
  - Automatic stocking density calculation
  - View total stocking length and density

#### MQTT Configuration Tab
- Enable/disable MQTT publishing
- Broker host/IP and port configuration
- Device ID (used in MQTT topics)
- Publish interval (milliseconds)
- Username/password authentication
- Home Assistant MQTT Discovery toggle
- Connection test button
- Real-time connection status

**Access:** `http://aquarium.local/calibration`

### WiFi Setup Page (Provisioning Mode Only)

Appears when device is in provisioning mode (no WiFi credentials stored).

- Scan for available WiFi networks
- Manual SSID entry option
- Password entry
- Save credentials and restart

**Access:** `http://192.168.4.1/setup` (only when in AP mode)

## API Endpoints

### Sensor Data
- `GET /api/sensors` - Current sensor readings (JSON)
- `GET /api/metrics/derived` - Current derived metrics (JSON)
- `GET /api/history` - Historical data (288 points, all metrics)

### Data Export
- `GET /api/export/csv` - Export all data in CSV format
- `GET /api/export/json` - Export all data in JSON format

### Tank Configuration
- `GET /api/settings/tank` - Get tank configuration
- `POST /api/settings/tank` - Save tank configuration

### Fish Profiles
- `GET /api/settings/fish` - Get fish profile list
- `POST /api/settings/fish/add` - Add fish to profile
- `POST /api/settings/fish/remove` - Remove fish by index
- `POST /api/settings/fish/clear` - Clear all fish profiles

### MQTT Configuration
- `GET /api/mqtt/config` - Get MQTT settings
- `POST /api/mqtt/config` - Save MQTT settings
- `GET /api/mqtt/status` - Get connection status

### WiFi Provisioning
- `GET /scan` - Scan for WiFi networks
- `POST /save-wifi` - Save WiFi credentials

### Pages
- `GET /` - Main dashboard (or provisioning page if in AP mode)
- `GET /charts` - Historical data charts
- `GET /calibration` - Calibration and settings interface
- `GET /setup` - WiFi provisioning page

## Example API Responses

### GET /api/sensors
```json
{
  "temperature_c": 24.5,
  "orp_mv": 250.3,
  "ph": 7.2,
  "ec_ms_cm": 1.41,
  "valid": true,
  "ph_calibrated": true,
  "ec_calibrated": true
}
```

### GET /api/metrics/derived
```json
{
  "tds_ppm": 903.4,
  "co2_ppm": 18.5,
  "nh3_ratio": 0.015,
  "nh3_ppm": 0.0045,
  "max_do_mg_l": 8.24,
  "stocking_density": 1.25,
  "valid": true
}
```

### GET /api/history
```json
{
  "count": 288,
  "interval": 5,
  "data": [
    {
      "timestamp": 1736339400,
      "temp_c": 24.5,
      "orp_mv": 250.3,
      "ph": 7.2,
      "ec_ms_cm": 1.41,
      "tds_ppm": 903.4,
      "co2_ppm": 18.5,
      "nh3_ratio": 0.015,
      "nh3_ppm": 0.0045,
      "max_do": 8.24,
      "stocking": 1.25,
      "valid": true
    }
  ]
}
```

## Theme Support

**Dark and Light Modes:**
- Toggle button available on all pages
- Preference stored in browser localStorage
- Synced across all pages

**Color Schemes:**
- Dark mode: Dark backgrounds with light text (reduces eye strain)
- Light mode: Light backgrounds with dark text (better for bright environments)

## Planned Features

- WebSocket live updates (currently using HTTP polling)
- Output control toggles (manual relay/driver control)
- Alert threshold configuration
- Configurable sampling rate
- Device settings management
- Multi-device support

## Browser Compatibility

**Tested and working:**
- Chrome/Chromium (desktop and mobile)
- Safari (macOS and iOS)
- Firefox (desktop and mobile)
- Edge (desktop)

**Minimum requirements:**
- JavaScript enabled
- LocalStorage support (for theme preference)
- Fetch API support (modern browsers)

## Mobile Optimization

- Responsive layout adapts to screen size
- Touch-friendly controls
- Readable text sizes on small screens
- Charts resize appropriately
- Tables scroll horizontally on narrow screens

## Performance

**Page Load Times:**
- Dashboard: < 1 second (minimal JavaScript)
- Charts: 1-2 seconds (Chart.js library loading)
- Export: Instant (browser download)

**Update Frequency:**
- Dashboard: Auto-refresh every 2 seconds (configurable in code)
- Charts: Real-time updates when viewing
- MQTT status: Updates with sensor readings

**Data Usage:**
- Minimal bandwidth (JSON payloads < 1KB)
- No external dependencies (no CDN, all assets served locally)
- Works fully offline (no internet required, only local WiFi)

## Next Steps

- [Calibration Guide](CALIBRATION.md) - Calibrate sensors for accurate readings
- [MQTT Setup](MQTT.md) - Integrate with Home Assistant
- [Configuration](CONFIGURATION.md) - Advanced configuration options
