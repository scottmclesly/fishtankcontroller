# Configuration Guide

## Overview

The Fish Tank Controller offers comprehensive configuration through the web interface and persistent storage in ESP32 NVS (Non-Volatile Storage).

## Configuration Categories

### WiFi Configuration

**Initial Setup (Provisioning Mode):**
- Access provisioning portal at `http://192.168.4.1`
- Scan for networks or enter SSID manually
- Save credentials (stored in NVS)
- Device restarts and connects

**Network Settings:**
- **Hostname (mDNS):** `aquarium.local`
- **AP Mode SSID:** `AquariumSetup`
- **AP Mode Password:** `aquarium123`
- **AP Mode IP:** `192.168.4.1`

**Connection Behavior:**
- Up to 3 retry attempts with 10-second timeout each
- Automatic fallback to AP mode on failure
- Credentials persist across reboots

**Reset WiFi Credentials:**
- Method 1: Reflash firmware (clears NVS)
- Method 2: Reset command via serial (future feature)

See [Installation Guide](INSTALLATION.md) for detailed WiFi setup.

### Tank Configuration

Configure aquarium parameters for accurate derived metrics calculations.

**Access:** `http://aquarium.local/calibration` → **Tank Settings** tab

#### Tank Shape and Volume

**Shape options:**
- **Rectangle:** Standard rectangular tank
- **Cube:** Cube-shaped tank
- **Cylinder:** Cylindrical tank
- **Custom:** Irregular shape with manual volume entry

**Dimension inputs:**
- **Rectangle:** Length, width, height (cm)
- **Cube:** Side length (cm)
- **Cylinder:** Radius, height (cm)
- **Custom:** Manual volume entry (liters)

**Volume calculation:**
- Click **"Calculate Volume"** for automatic calculation
- Override with **Manual Volume** for irregular shapes
- Volume displayed in liters

**Example:**
```
Shape: Rectangle
Length: 60 cm
Width: 30 cm
Height: 40 cm
Calculated Volume: 72 L
```

#### Water Chemistry Parameters

**KH (Carbonate Hardness):**
- Units: dKH (degrees of carbonate hardness)
- Default: 4.0 dKH
- Typical freshwater: 3-8 dKH
- Typical marine: 7-12 dKH
- **Used for:** CO₂ calculation

**TAN (Total Ammonia Nitrogen):**
- Units: ppm (parts per million)
- Default: 0.0 ppm
- Measure with standard aquarium test kit
- **Used for:** Toxic ammonia (NH₃) calculation

**TDS Conversion Factor:**
- Range: 0.5 - 0.7
- Default: 0.64 (typical freshwater)
- 0.5 for pure solutions
- 0.64 for freshwater
- 0.7 for saltwater
- **Used for:** Converting EC to TDS

#### Fish Profile Management

Track fish stocking for automatic stocking density calculation.

**Add Fish:**
1. Enter species name (e.g., "Neon Tetra", "Angelfish")
2. Enter count (number of fish)
3. Enter average length in cm (use adult size)
4. Click **"Add Fish"**

**Manage Fish:**
- View total stocking length (sum of all fish lengths)
- View stocking density (cm of fish per liter)
- Remove individual fish entries
- Clear all fish profiles

**Stocking Guidelines:**
- **< 1 cm/L:** Lightly stocked (ideal)
- **1-2 cm/L:** Moderately stocked (good filtration required)
- **> 2 cm/L:** Heavily stocked (excellent filtration + frequent water changes)

**Example:**
```
Tank Volume: 100 L

Fish Profile:
- Neon Tetra: 10 × 3 cm = 30 cm
- Angelfish: 2 × 15 cm = 30 cm
- Corydoras: 5 × 5 cm = 25 cm

Total: 85 cm
Stocking Density: 0.85 cm/L (Lightly stocked)
```

**Storage:** All tank settings persist in NVS across reboots.

### MQTT Configuration

Configure MQTT broker connection for home automation integration.

**Access:** `http://aquarium.local/calibration` → **MQTT Configuration** tab

**Configuration options:**

| Setting | Description | Default | Required |
|---------|-------------|---------|----------|
| Enable MQTT | Toggle publishing on/off | Disabled | - |
| Broker Host | IP or hostname | `192.168.1.100` | Yes |
| Broker Port | MQTT port | `1883` | Yes |
| Device ID | Topic identifier | `aquarium` | Yes |
| Publish Interval | Milliseconds between publishes | `5000` | Yes |
| Username | Authentication username | (empty) | No |
| Password | Authentication password | (empty) | No |
| HA Discovery | Auto-register in Home Assistant | Enabled | No |

**Test Connection:**
- Click **"Test Connection"** before saving
- Verifies broker is reachable
- Validates credentials

**Connection Status:**
- Monitored in real-time on Dashboard and Charts pages
- Auto-reconnection on connection loss (5-second retry)

See [MQTT Guide](MQTT.md) for detailed MQTT setup and integration.

### Sensor Calibration

Configure pH and EC sensor calibration for accurate measurements.

**Access:** `http://aquarium.local/calibration` → **Sensor Calibration** tab

**pH Calibration:**
- 1-point (offset only)
- 2-point (offset + slope, recommended)
- Uses standard buffer solutions

**EC Calibration:**
- Cell constant determination
- Uses known conductivity solutions
- Automatic temperature compensation

See [Calibration Guide](CALIBRATION.md) for detailed procedures.

### Sensor Polling Rate

**Current:** Fixed 5-second interval (hardcoded)

**Location:** `src/main.cpp` - main loop delay

**Planned:** User-configurable via web interface

**Tradeoffs:**
- **Faster (1-2 seconds):** More responsive, higher load, more data points
- **Slower (10-30 seconds):** Lower load, fewer data points, less battery use

### Theme Settings

**Dark/Light Mode:**
- Toggle available on all web pages
- Preference stored in browser localStorage
- Synchronized across all pages
- Persists across sessions

**How to change:**
- Click theme toggle button (moon/sun icon) on any page

## Data Storage and Persistence

### Non-Volatile Storage (NVS)

All configuration stored in ESP32 NVS flash partition:

**Persists across:**
- ✓ Reboots
- ✓ Power cycles
- ✓ Firmware updates (if NVS not erased)

**Stored data:**
- WiFi credentials (SSID, password)
- MQTT configuration (all settings)
- pH calibration (offset, slope)
- EC calibration (cell constant)
- Tank configuration (shape, dimensions, volume)
- Water chemistry parameters (KH, TAN, TDS factor)
- Fish profiles (species, count, length)

### Clearing Configuration

**Clear all NVS data:**
```bash
# Reflash firmware with erase
pio run -t erase
pio run -t upload
```

**Selective clearing:**
- Planned: Reset buttons for individual categories (WiFi, MQTT, calibration, etc.)

## Historical Data Configuration

### Data Buffer

**Current settings:**
- **Buffer size:** 288 data points
- **Interval:** 5 seconds
- **Total history:** 24 minutes

**Storage:** RAM (circular buffer, lost on reboot)

**Data points include:**
- Primary sensors (temperature, ORP, pH, EC)
- Derived metrics (TDS, CO₂, NH₃, DO, stocking)
- Timestamp
- Validity flag

### Planned Long-term Logging

**Future features:**
- Flash storage (weeks/months of data)
- SD card support (unlimited storage)
- Remote database integration (InfluxDB, etc.)

## Console Configuration

**Access:** Serial monitor at 115200 baud

**Available commands:**
- `help` or `?` - Show help
- `status` - System status
- `dump` or `csv` - Export data in CSV
- `dump json` or `json` - Export data in JSON
- `clear` - Clear terminal

**Planned commands:**
- `wifi reset` - Clear WiFi credentials
- `mqtt reset` - Clear MQTT config
- `cal reset` - Clear calibration data
- `config show` - Display all configuration

See README console commands section for details.

## Network Configuration

### mDNS Settings

**Hostname:** `aquarium` (hardcoded)
**Access URL:** `http://aquarium.local`

**Planned:** User-configurable hostname

### AP Mode Settings

**SSID:** `AquariumSetup` (hardcoded)
**Password:** `aquarium123` (hardcoded)
**IP:** `192.168.4.1` (hardcoded)

**Planned:** Customizable AP credentials

### Port Configuration

**Web Server:** Port 80 (HTTP, hardcoded)

**Planned:**
- Configurable web server port
- HTTPS support (port 443)

## Planned Configuration Options

### Output Control
- GPIO pin mapping for relays/drivers
- Safety default states (on boot/reset)
- Interlock configuration (mutual exclusion)
- PWM configuration for dimmers

### Alerts and Notifications
- Threshold configuration (pH, temp, EC, ORP)
- Alert hysteresis settings
- Notification methods (MQTT, email, push)

### Automation
- Scheduled events (lights, feeding, water changes)
- Conditional rules (if-then logic)
- Safety interlocks

### Advanced Settings
- Sampling rate (sensor polling interval)
- Data retention period
- TLS/SSL for MQTT
- OTA update configuration

## Configuration Best Practices

### Initial Setup Checklist
1. ✓ Connect to WiFi network
2. ✓ Configure tank dimensions and volume
3. ✓ Set water chemistry parameters (KH, TAN)
4. ✓ Add fish profile for stocking calculation
5. ✓ Calibrate pH sensor (2-point recommended)
6. ✓ Calibrate EC sensor (cell constant)
7. ✓ Configure MQTT (if using home automation)
8. ✓ Test MQTT connection
9. ✓ Verify dashboard displays all metrics

### Periodic Maintenance
- **Monthly:** Check pH calibration (1-point)
- **Quarterly:** Check EC calibration
- **As needed:** Update fish profile when stocking changes
- **After water changes:** Verify derived metrics are reasonable

### Backup Configuration
- **Manual export:** Copy settings from web interface
- **Planned:** Export/import configuration via JSON file

## Troubleshooting Configuration

### Configuration Not Saving
- Check serial monitor for NVS write errors
- Verify sufficient free NVS space
- Try reflash if NVS corrupted

### Settings Reset After Reboot
- NVS may be corrupted
- Reflash firmware with erase: `pio run -t erase`
- Reconfigure all settings

### Can't Access Configuration Pages
- Verify WiFi connection
- Try direct IP instead of mDNS
- Check browser console for errors
- Clear browser cache

## Next Steps

- [Calibration Guide](CALIBRATION.md) - Calibrate sensors
- [MQTT Setup](MQTT.md) - Configure home automation
- [Web UI Guide](WEB_UI.md) - Use web interface
- [Troubleshooting](TROUBLESHOOTING.md) - Solve common issues
