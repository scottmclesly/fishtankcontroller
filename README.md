# Fish Tank Controller

Wireless aquarium controller (freshwater / saltwater) built around the **Sentron POET pH / ORP / EC / Temperature over I2C** sensor, plus additional sensors and relay/driver outputs for tank equipment.

> **Status:** Working prototype - WiFi connectivity, sensor monitoring, and web UI operational

---

## Table of contents
- [Goals](#goals)
- [System overview](#system-overview)
- [Key features](#key-features)
- [Architecture](#architecture)
- [Hardware](#hardware)
- [Firmware](#firmware)
- [Web UI](#web-ui)
- [Mobile/Desktop UI (Flutter)](#mobiledesktop-ui-flutter)
- [Data + integrations](#data--integrations)
- [Installation](#installation)
- [Configuration](#configuration)
- [Console Commands](#console-commands)
- [Calibration](#calibration)
- [Safety + electrical notes](#safety--electrical-notes)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

---

## Goals
- Provide reliable sensing + control for aquarium life-support systems.
- Offer a simple, cross-platform UI for monitoring and control.
- Publish raw and normalized telemetry to standard integrations (e.g., MQTT).

Non-goals (for now):
- Cloud-first dependencies (the system should work fully offline).
- Overly complex automation “AI” logic.

---

## System overview
This project consists of:
- **Controller hardware**: ESP32 MCU + POET I2C sensor + additional sensors + relay/driver outputs.
- **Firmware**: sensor acquisition, control logic, REST/WebSocket APIs, MQTT publishing.
- **UI**: on-device Web UI and optional Flutter app.
- **Integrations**: MQTT (Home Assistant, Node-RED, etc.).

---

## Key features
- Multi-sensor water quality monitoring (POET: pH / ORP / EC / temperature).
- Expandable sensor stack (TBD).
- Output control for tank devices (heater, lights, pumps, dosing, etc.)
- Schedules and safety interlocks (TBD).
- Local-first operation with optional integration publishers.

---

## Architecture
### Recommended transports (best-practice)
This project is **local-first**:
- **Device ↔ UI (Web + Flutter):**
  - **HTTP REST** for configuration, status snapshots, and CRUD (simple + debuggable)
  - **WebSocket** for live telemetry + output state changes (low-latency, efficient)
- **Device ↔ Ecosystem (Home Assistant, Node-RED, etc.):**
  - **MQTT** for telemetry + commands, with **Home Assistant MQTT Discovery** for automatic entity creation

Why this split:
- REST/WebSocket keeps the UI fast and predictable without requiring a broker.
- MQTT is the standard integration “bus” and scales well across multiple controllers.

### High-level
- **MCU firmware**
  - I2C sensor drivers
  - Control outputs + failsafes
  - Rules engine (optional)
  - REST + WebSocket API
  - MQTT client (optional but recommended)
  - On-device Web UI hosting
- **Web UI**
  - Runs in any browser (desktop/tablet/phone)
  - Works even when the controller has no internet
- **Flutter UI**
  - Cross-platform dashboard + setup + calibration workflows
  - Uses the same REST/WebSocket API as the Web UI
- **Integrations**
  - MQTT topics (raw and normalized)
  - Home Assistant via MQTT Discovery

### Proposed module layout
```
/firmware
  /src
  /lib
  platformio.ini
/app
  /lib
  pubspec.yaml
/web
  /static         # static HTML/CSS/JS served by the MCU
/docs
/hardware
/scripts
```

---

## Hardware
### Locked-in components
- **Primary water-quality sensor**: Sentron POET pH/ORP/EC/Temp over I2C
- **Controller MCU**: ESP32 family (chosen below)

### Recommended ESP32 “flavour”
Pick one of these based on how heavy your UI/telemetry needs get:
- **ESP32-S3 (recommended default):** best all-around for a controller that may host a richer Web UI and handle multiple sensors.
  - Typical module: **ESP32-S3-WROOM-1** (consider PSRAM variant if available)
- **ESP32-C3:** good low-cost option if the on-device UI is minimal and you don’t expect heavy concurrency.
- **ESP32-C6:** good if you specifically want newer radios and are comfortable with the ecosystem maturity tradeoffs.

If you want a safe default today: **ESP32-S3**.

### Planned additions (placeholders)
- Temperature probe(s): TBD
- Water level: TBD
- Leak detection: TBD
- Flow / pressure: TBD
- Output drivers: relays / MOSFETs / SSRs (TBD)

### I2C notes (POET)
- I2C 7-bit main address: `0x1F`
- Bus speed: up to 400 kHz
- Sensor logic levels: **3.3V max** on SDA/SCL (use level shifting if needed)
- External pull-ups recommended

### Power + enclosure
- Tank-side wiring plan: TBD
- Enclosure / ingress rating: TBD

---

## Firmware
### Responsibilities
- Read POET measurements (temp, ORP, pH potential, EC current/excitation).
- Convert raw measurements into user-facing units (°C, ORP mV, pH, EC mS/cm).
- Publish telemetry.
- Control outputs (manual + automated) with watchdog/failsafe behavior.

### Sensor acquisition (POET)
- A measurement is started by writing a **command byte** to `0x1F`.
- Command bits (LSB → MSB):
  - bit0: temperature
  - bit1: ORP
  - bit2: pH
  - bit3: EC
  - bit4-7: reserved (0)
- Reads return a **variable-length reply** of 32-bit little-endian signed integers.

### Build/run
- **Build system:** PlatformIO
- **Framework:**
  - **Arduino framework** (current implementation for rapid prototyping)
  - **ESP-IDF (future)** for long-term robustness, networking, and OTA

**Build commands:**
```bash
# Build firmware
pio run

# Build and upload to device
pio run -t upload

# Monitor serial output
pio device monitor

# Build + upload + monitor (common workflow)
pio run -t upload && pio device monitor
```

**Current target:** `seeed_xiao_esp32c3` (ESP32-C3 board)

---

## Web UI
The controller hosts a Web UI so you can manage it from any device on the same network (phone/tablet/laptop) without installing anything.

### Current Implementation
The device now includes a fully functional web interface with:

**Live Sensor Dashboard:**
- Real-time display of Temperature (°C), ORP (mV), pH, and EC (mS/cm)
- **Derived water quality metrics:**
  - Total Dissolved Solids (TDS) in ppm
  - Dissolved CO₂ concentration in ppm with color-coded status
  - Toxic ammonia (NH₃) ratio as percentage
  - Maximum dissolved oxygen saturation in mg/L
  - Stocking density in cm of fish per liter with color-coded warnings
- Auto-refresh every 2 seconds via background polling
- Responsive design works on desktop, tablet, and mobile
- Visual warnings for uncalibrated sensors
- WiFi connection status and signal strength
- **MQTT connection status indicator** showing real-time broker connectivity

**Sensor Calibration & Settings Interface:**
- **Tab-based navigation** with three sections:
  - **Sensor Calibration:** pH calibration (1-point or 2-point with buffer solutions), EC calibration (cell constant determination)
  - **Tank Settings:** Aquarium configuration for derived metrics calculations
  - **MQTT Configuration:** Broker setup and connection management
- **Tank Settings configuration:**
  - Tank shape selection (rectangle, cube, cylinder, custom)
  - Dimension inputs with automatic volume calculation
  - Manual volume override for custom shapes
  - Water chemistry parameters (KH, TAN, TDS conversion factor)
  - Fish profile management (up to 10 species with count and average length)
  - Total stocking calculation display
- Real-time raw sensor readings display
- Calibration status tracking with NVS persistence
- All settings persist across reboots in NVS

**Historical Data & Charts:**
- 288-point circular buffer (24 minutes at 5-second intervals)
- **Includes all primary sensors and derived metrics** in historical tracking
- Real-time chart visualization with Chart.js
- **View toggle buttons** to switch between:
  - All Metrics (primary sensors + derived metrics)
  - Primary Sensors only (temperature, ORP, pH, EC)
  - Derived Metrics only (TDS, CO₂, NH₃ ratio, max DO, stocking density)
- Individual charts for each metric with appropriate scaling and units
- Data export in CSV and JSON formats (includes derived metrics)
- **MQTT status monitoring** on charts page

**WiFi Provisioning:**
- Automatic fallback to AP mode when no credentials stored or connection fails
- Network scanning to discover available WiFi networks
- Simple web-based credential entry
- Credentials stored in ESP32 NVS (non-volatile storage)
- Automatic retry logic with configurable timeout

**API Endpoints:**
- `GET /` - Main dashboard (sensor view or provisioning based on mode)
- `GET /api/sensors` - JSON sensor data for programmatic access
- `GET /api/history` - Historical sensor data (288-point buffer, includes derived metrics)
- `GET /api/export/csv` - Export data in CSV format (includes derived metrics)
- `GET /api/export/json` - Export data in JSON format (includes derived metrics)
- `GET /calibration` - Sensor calibration interface with tank settings and MQTT config tabs
- `GET /charts` - Historical data visualization with derived metrics
- `GET /setup` - Provisioning configuration page
- `POST /save-wifi` - Save WiFi credentials
- `GET /scan` - Scan for available networks
- `GET /api/mqtt/config` - Get MQTT configuration
- `POST /api/mqtt/config` - Save MQTT configuration
- `GET /api/mqtt/status` - Get MQTT connection status
- **Derived Metrics & Tank Settings:**
  - `GET /api/metrics/derived` - Get all derived water quality metrics
  - `GET /api/settings/tank` - Get tank configuration settings
  - `POST /api/settings/tank` - Save tank configuration
  - `GET /api/settings/fish` - Get fish profile list
  - `POST /api/settings/fish/add` - Add fish to profile
  - `POST /api/settings/fish/remove` - Remove fish by index
  - `POST /api/settings/fish/clear` - Clear all fish profiles

### Access Methods
- **mDNS:** `http://aquarium.local` (when connected to WiFi)
- **Direct IP:** `http://<device-ip>` (shown in serial output)
- **Provisioning AP:** `http://192.168.4.1` (when in AP mode)

### Provisioning Process
**First boot or WiFi failure:**
1. Device creates access point:
   - **SSID:** `AquariumSetup`
   - **Password:** `aquarium123`
2. Connect to this AP with phone/laptop
3. Navigate to `http://192.168.4.1`
4. Scan for networks or manually enter credentials
5. Submit credentials - device saves and restarts
6. Device connects to your WiFi network
7. Access via `http://aquarium.local` or device IP

**Credential Storage:**
- WiFi credentials stored in ESP32 NVS (persistent across reboots)
- Up to 3 connection retry attempts with 10-second timeout per attempt
- Graceful fallback to AP mode on connection failure

### Future Scope
- Output controls (manual override)
- Config pages (MQTT, sampling rate, alarms)
- Calibration workflows (pH / EC)
- WebSocket for live updates (currently using polling)


---

---

## Mobile/Desktop UI (Flutter)
### Primary screens
- Dashboard (live readings + status)
- Controls (toggles / setpoints)
- Calibration workflows
- Alerts + history
- Device settings (Wi‑Fi, MQTT, sampling rate, etc.)

### UI stack
- Flutter (iOS/Android/Desktop)
- **Transport:** HTTP REST + WebSocket to the controller (same API the Web UI uses)

## Data + integrations
### MQTT ✅ (IMPLEMENTED)
MQTT is the preferred "gateway" into **Home Assistant** and other automation stacks.

**Features:**
- ✅ **Fully implemented** MQTT client with automatic reconnection
- ✅ **Home Assistant MQTT Discovery** - entities appear automatically
- ✅ Configurable broker host, port, and authentication
- ✅ Configurable device ID and publish intervals
- ✅ Persistent configuration stored in NVS
- ✅ Web-based configuration interface with connection testing
- ✅ Real-time status indicators on Dashboard and Charts pages
- Allows multiple consumers (HA + Node-RED + your own logger) without coupling

**Base topic**: `aquarium/<device_id>/...`

**Published Topics:**

*Primary Sensors:*
- `aquarium/<device_id>/telemetry/temperature` - Temperature in °C
- `aquarium/<device_id>/telemetry/orp` - ORP in mV
- `aquarium/<device_id>/telemetry/ph` - pH value
- `aquarium/<device_id>/telemetry/ec` - EC in mS/cm

*Derived Metrics:*
- `aquarium/<device_id>/telemetry/tds` - Total Dissolved Solids in ppm
- `aquarium/<device_id>/telemetry/co2` - Dissolved CO₂ in ppm
- `aquarium/<device_id>/telemetry/nh3_ratio` - Toxic ammonia ratio as percentage
- `aquarium/<device_id>/telemetry/nh3_ppm` - Toxic ammonia (NH₃) in ppm
- `aquarium/<device_id>/telemetry/max_do` - Maximum dissolved oxygen in mg/L
- `aquarium/<device_id>/telemetry/stocking` - Stocking density in cm/L

*Combined Payload:*
- `aquarium/<device_id>/telemetry/sensors` - Combined JSON payload (all sensors + derived metrics)

**Home Assistant Discovery Topics** (when enabled):

*Primary Sensors:*
- `homeassistant/sensor/<device_id>/temperature/config`
- `homeassistant/sensor/<device_id>/orp/config`
- `homeassistant/sensor/<device_id>/ph/config`
- `homeassistant/sensor/<device_id>/ec/config`

*Derived Metrics:*
- `homeassistant/sensor/<device_id>/tds/config`
- `homeassistant/sensor/<device_id>/co2/config`
- `homeassistant/sensor/<device_id>/nh3_ratio/config`
- `homeassistant/sensor/<device_id>/nh3_ppm/config`
- `homeassistant/sensor/<device_id>/max_do/config`
- `homeassistant/sensor/<device_id>/stocking/config`

### MQTT Configuration

**Via Web Interface:**
1. Navigate to `http://aquarium.local/calibration`
2. Click the **MQTT Configuration** tab
3. Configure settings:
   - **Enable MQTT Publishing** - Toggle to enable/disable
   - **Broker Host/IP** - Your MQTT broker address (e.g., `192.168.1.100`, `homeassistant.local`)
   - **Broker Port** - Default `1883` (standard MQTT)
   - **Device ID** - Unique identifier (e.g., `aquarium`, `reef_tank`, `freshwater1`)
   - **Publish Interval** - How often to publish data in milliseconds (default: 5000ms)
   - **Username/Password** - Optional authentication credentials
   - **Home Assistant MQTT Discovery** - Toggle to auto-register sensors in HA
4. Click **Save MQTT Configuration**
5. Use **Test Connection** to verify settings
6. Monitor connection status on Dashboard or Charts page

**Via Configuration File** (stored in NVS):
- Configuration persists across reboots
- Modify via web interface or clear NVS to reset

### Data model
Combined sensor telemetry payload (`aquarium/<device_id>/telemetry/sensors`):
```json
{
  "temperature_c": 25.5,
  "orp_mv": 350.2,
  "ph": 7.8,
  "ec_ms_cm": 1.234,
  "tds_ppm": 791.2,
  "co2_ppm": 18.5,
  "nh3_ratio": 0.015,
  "nh3_ppm": 0.0045,
  "max_do_mg_l": 8.24,
  "stocking_density": 1.25,
  "valid": true,
  "timestamp": 123456789
}
```

Individual sensor topics publish simple float values for easy integration.

### Derived Metrics Calculations

The system calculates several derived water quality metrics from sensor readings and tank configuration:

**Total Dissolved Solids (TDS):**
- Formula: `TDS (ppm) = EC (μS/cm) × conversion factor`
- Default conversion factor: 0.64 (configurable for freshwater/saltwater)
- Indicates overall dissolved mineral content

**Dissolved CO₂:**
- Formula: `CO₂ (ppm) = 3.0 × KH × 10^(7.0 - pH)`
- Based on carbonate equilibrium equation
- Color coding: Green (15-30 ppm optimal), Yellow (<15 ppm), Red (>30 ppm)
- Requires KH (carbonate hardness) configuration

**Toxic Ammonia (NH₃):**
- Temperature and pH dependent calculation using dissociation equilibrium
- Formula: `pKa = 0.09018 + (2729.92 / T_kelvin)`
- Returns both ratio (fraction as NH₃) and actual ppm
- Critical for fish health monitoring

**Maximum Dissolved Oxygen (DO):**
- Temperature-dependent polynomial approximation
- Formula: `DO = 14.652 - 0.41022×T + 0.007991×T² - 0.000077774×T³`
- Helps assess aeration adequacy

**Stocking Density:**
- Calculated from fish profiles and tank volume
- Formula: `Density = total fish length (cm) / tank volume (L)`
- Color coding: Green (<1), Yellow (1-2), Red (>2)
- Rule of thumb: 1 cm per 1-2 liters for small tropical fish

---

## Installation

### Prerequisites
- PlatformIO installed (via VS Code extension or CLI)
- POET sensor connected via I2C (address `0x1F`)
- ESP32-C3 board (Seeed XIAO ESP32-C3 or compatible)

### 1) Hardware Setup
- Wire POET sensor to ESP32 I2C pins (SDA/SCL)
- Connect sensor power (3.3V)
- Verify I2C connections and pull-ups
- Add output drivers (future) and verify isolation and fusing

### 2) Firmware Flash
```bash
# Clone repository and navigate to project
cd fishtankcontroller

# Build firmware
pio run

# Upload to device (device must be connected via USB)
pio run -t upload

# Monitor serial output to verify
pio device monitor
```

### 3) WiFi Provisioning (First Boot)
On first boot, the device will start in provisioning mode:

1. **Connect to AP:**
   - SSID: `AquariumSetup`
   - Password: `aquarium123`

2. **Configure WiFi:**
   - Open browser to `http://192.168.4.1`
   - Click "Scan for Networks" or manually enter SSID
   - Enter WiFi password
   - Click "Connect to WiFi"

3. **Device restarts and connects:**
   - Watch serial monitor for connection status
   - Note the assigned IP address
   - Device is now accessible via:
     - `http://aquarium.local` (mDNS)
     - `http://<device-ip>`

### 4) Verify Sensor Operation
- Open web dashboard
- Verify sensor readings update every 5 seconds
- Check serial output for detailed sensor data
- Note calibration warnings (pH and EC require calibration)

### 5) UI Access
**Current:**
- Web UI via browser at `http://aquarium.local`

**Future:**
- Flutter app for iOS/Android/Desktop

---

---

## Configuration

### Current Configuration Options
**WiFi (via Web UI):**
- SSID / password (provisioning portal on first boot)
- Stored in NVS (non-volatile storage)
- To reset: clear NVS via serial or reflash firmware

**Network:**
- Hostname / mDNS name: `aquarium` (default, accessible via `http://aquarium.local`)
- AP credentials (when in provisioning mode):
  - SSID: `AquariumSetup`
  - Password: `aquarium123`

**Sensor Polling:**
- Current: 5-second interval (hardcoded in main loop)
- Future: Configurable via web interface

**MQTT (via Web UI - Calibration Page):**
- ✅ Broker host/IP address
- ✅ Broker port (default: 1883)
- ✅ Device ID (topic prefix)
- ✅ Publish interval (milliseconds)
- ✅ Username/password authentication
- ✅ Home Assistant MQTT Discovery toggle
- ✅ Enable/disable MQTT publishing
- ✅ Connection testing and status monitoring
- Stored in NVS (persistent across reboots)

**Calibration & Tank Settings (via Web UI - Calibration Page):**
- ✅ pH calibration (1-point offset or 2-point offset+slope)
- ✅ EC calibration (cell constant with known solution)
- ✅ Calibration status tracking
- ✅ **Tank configuration** (shape, dimensions, volume calculation)
- ✅ **Water chemistry parameters** (KH, TAN, TDS conversion factor)
- ✅ **Fish profile management** (species, count, average length)
- ✅ All settings stored in NVS (persistent across reboots)

### Planned Configuration Options
- Sampling rate: User-configurable interval (currently hardcoded at 5 seconds)
- Output mapping: names, GPIOs, safety defaults
- Alerts: thresholds for pH/ORP/EC/temperature
- TLS/SSL for secure MQTT connections

---

## Console Commands

The firmware includes a serial console command interface for debugging and data analysis. Connect via serial monitor (115200 baud) and type commands.

### Available Commands

| Command | Aliases | Description |
|---------|---------|-------------|
| `help` | `?` | Show help message with all available commands |
| `status` | - | Display system status (WiFi, calibration, uptime) |
| `dump` | `csv` | Export all captured data in CSV format |
| `dump json` | `json` | Export all captured data in JSON format |
| `clear` | - | Clear the terminal screen |

### Data Export Formats

#### CSV Format
Best for analysis in Excel, Google Sheets, or data analysis tools like Python/R.

```csv
# Aquarium Monitor Data Export
# Device: Kate's Aquarium #7 | Export time: Wed Jan  8 12:34:56 2026
# WiFi: MyNetwork
# pH Calibration: Yes
# EC Calibration: Yes
# Data Points: 288
# Interval: 5 seconds
#
Timestamp,Unix_Time,Temperature_C,ORP_mV,pH,EC_mS_cm,Valid
2026-01-08 12:30:00,1736339400,24.50,250.00,7.20,1.410,true
2026-01-08 12:30:05,1736339405,24.51,249.50,7.21,1.411,true
...
```

#### JSON Format
Best for programmatic processing, scripting, or API integration.

```json
{
  "device": {
    "name": "Kate's Aquarium #7",
    "export_timestamp": 1736339400,
    "uptime_seconds": 86400,
    "wifi_ssid": "MyNetwork",
    "wifi_ip": "192.168.1.100",
    "ph_calibrated": true,
    "ec_calibrated": true,
    "data_points": 288,
    "interval_seconds": 5
  },
  "data": [
    {
      "timestamp": 1736339400,
      "temp_c": 24.50,
      "orp_mv": 250.00,
      "ph": 7.20,
      "ec_ms_cm": 1.410,
      "valid": true
    }
  ],
  "summary": {"total_points": 288}
}
```

### Usage Examples

**Check system status:**
```
> status
=== System Status ===
WiFi: Connected to MyNetwork (-45 dBm)
IP: 192.168.1.100
pH Calibration: Calibrated
EC Calibration: Calibrated
Uptime: 86400 seconds
```

**Export data for analysis:**
```
> dump csv
[CSV data output - redirect to file or copy/paste]
```

**Capture data to file (using terminal):**
Most serial terminals allow saving output to a file. Alternatively, use command-line tools:
```bash
# Linux/Mac with screen
screen -L /dev/ttyUSB0 115200
# Type: dump csv
# Output is saved to screenlog.0

# Or with PlatformIO
pio device monitor > data_export.csv
# Type: dump csv
```

### Data History

The controller maintains a circular buffer of 288 data points at 5-second intervals (24 minutes of history). For longer-term logging, use:
- **Web UI Export** - Click "Export CSV" or "Export JSON" buttons on the Charts page
- The `/api/history` REST endpoint or `/api/export/csv` and `/api/export/json` endpoints
- MQTT integration with a time-series database
- Regular console dumps saved to files

### Web UI Export

You can also export data directly from the web interface:

1. Navigate to the **Charts** page at `http://aquarium.local/charts`
2. Click **Export CSV** or **Export JSON** buttons at the top of the page
3. The file will automatically download with a timestamp in the filename (e.g., `aquarium-data-2026-01-08T12-34-56.csv`)

This is the easiest method if you're already using the web interface!

---

## Calibration
> NOTE: Calibration is required for meaningful pH and EC values.

### Accessing the Calibration Interface
Navigate to `http://aquarium.local/calibration` to access a tab-based interface with three sections:

**Sensor Calibration Tab:**
- Real-time sensor readings for calibration reference
- pH calibration tools (1-point and 2-point)
- EC calibration tools (cell constant determination)
- Calibration status indicators

**Tank Settings Tab:**
- Tank shape selection and dimension configuration
- Automatic volume calculation
- Water chemistry parameters (KH, TAN, TDS factor)
- Fish profile management for stocking density calculations

**MQTT Configuration Tab:**
- Broker connection setup
- Publishing interval configuration
- Home Assistant Discovery toggle
- Connection testing and status monitoring

### pH Calibration
**1-Point Calibration (Offset Only):**
1. Prepare a pH buffer solution (pH 4.0, 7.0, or 10.0)
2. Rinse sensor with distilled water and immerse in buffer
3. Wait 1-2 minutes for reading to stabilize
4. Click "Refresh Readings" to get current Ugs voltage
5. Enter buffer pH and measured Ugs voltage
6. Click "Calibrate pH (1-Point)"

**2-Point Calibration (Offset + Slope - Recommended):**
1. Prepare two pH buffer solutions (typically pH 4.0 and 7.0)
2. For first buffer:
   - Rinse sensor and immerse
   - Wait for stabilization
   - Record buffer pH and Ugs voltage
3. For second buffer:
   - Rinse sensor thoroughly
   - Immerse in second buffer
   - Wait for stabilization
   - Record buffer pH and Ugs voltage
4. Enter both measurements and click "Calibrate pH (2-Point)"

Calibration data is stored in NVS and persists across reboots.

### EC / Conductivity Calibration
**Cell Constant Calibration:**
1. Prepare a known conductivity solution:
   - 0.01M KCl: 1.41 mS/cm @ 25°C
   - 0.1M KCl: 12.88 mS/cm @ 25°C
   - 1M KCl: 111.9 mS/cm @ 25°C
2. Rinse sensor with distilled water and immerse in solution
3. Wait 1-2 minutes for reading to stabilize
4. Measure solution temperature accurately
5. Click "Refresh Readings" to get current EC measurements
6. Enter:
   - Known conductivity (mS/cm)
   - Solution temperature (°C)
   - Measured EC current (nA)
   - Measured EC voltage (μV)
7. Click "Calibrate EC"

The system calculates and stores the cell constant with automatic temperature compensation. Calibration data persists in NVS.

### Tank Settings Configuration

The Tank Settings tab allows you to configure your aquarium parameters for accurate derived metrics calculations.

**Tank Configuration:**
1. Navigate to `http://aquarium.local/calibration`
2. Click the **Tank Settings** tab
3. Select tank shape:
   - **Rectangle**: Enter length, width, and height
   - **Cube**: Enter side length
   - **Cylinder**: Enter radius and height
   - **Custom**: Enter manual volume directly
4. Click **Calculate Volume** to compute tank volume automatically
5. Optionally override with **Manual Volume** for irregular shapes

**Water Chemistry Parameters:**
- **KH (Carbonate Hardness)**: Enter in dKH (default: 4.0)
  - Required for CO₂ calculations
  - Typical range: 3-8 dKH for freshwater, 7-12 dKH for marine
- **TAN (Total Ammonia Nitrogen)**: Enter in ppm (default: 0.0)
  - Used for toxic ammonia (NH₃) calculations
  - Measure with standard aquarium test kit
- **TDS Conversion Factor**: Default 0.64 for freshwater
  - Range: 0.5-0.7 depending on water type
  - 0.5 for pure solutions, 0.64 for typical freshwater, 0.7 for saltwater

**Fish Profile Management:**
1. Add fish to calculate stocking density
2. For each species, enter:
   - Species name (e.g., "Neon Tetra", "Angelfish")
   - Count (number of fish)
   - Average length in cm (adult size)
3. Click **Add Fish** to save
4. View total stocking length and density
5. Remove fish or clear all as needed

**Stocking Density Guidelines:**
- **< 1 cm/L**: Lightly stocked (ideal)
- **1-2 cm/L**: Moderately stocked (acceptable with good filtration)
- **> 2 cm/L**: Heavily stocked (requires excellent filtration and frequent water changes)

All settings are automatically saved to NVS and persist across reboots.

---

## Safety + electrical notes
- Treat any mains-powered equipment (heaters, AC pumps) as **high risk**.
- Use proper enclosures, fusing, strain relief, and isolation.
- Consider galvanic isolation between sensor medium and other equipment.
- Define safe default states for outputs on boot/reset.

---

## Roadmap

### ✅ Completed
- [x] Decide MCU + firmware framework (ESP32-C3 + Arduino for prototyping)
- [x] POET driver + raw I2C reads
- [x] WiFi connection stack with credential storage and provisioning AP
- [x] On-device Web UI MVP for sensor monitoring
- [x] REST API for sensor data, history, and configuration
- [x] Historical data tracking with circular buffer (288 points)
- [x] Data export (CSV and JSON formats via web UI and console)
- [x] Real-time charts page with Chart.js visualization
- [x] Calibration storage + workflow (pH 1-point/2-point, EC cell constant)
- [x] Calibration web interface with real-time readings
- [x] **MQTT publisher with configurable broker settings**
- [x] **MQTT telemetry publishing (individual topics + combined JSON)**
- [x] **Home Assistant MQTT Discovery integration**
- [x] **MQTT configuration interface in web UI**
- [x] **MQTT connection status monitoring**
- [x] Console command interface for debugging and data export
- [x] Dark/light theme toggle across all pages
- [x] Responsive mobile-friendly design
- [x] **Derived water quality metrics system:**
  - [x] **Total Dissolved Solids (TDS) calculation from EC**
  - [x] **Dissolved CO₂ calculation from pH and KH**
  - [x] **Toxic ammonia (NH₃) ratio and ppm calculation**
  - [x] **Maximum dissolved oxygen calculation**
  - [x] **Stocking density calculation from fish profiles**
- [x] **Tank configuration management:**
  - [x] **Tank shape selection (rectangle, cube, cylinder, custom)**
  - [x] **Automatic volume calculation from dimensions**
  - [x] **Water chemistry parameters (KH, TAN, TDS factor)**
  - [x] **Fish profile management (up to 10 species)**
- [x] **Tab-based navigation on calibration page**
- [x] **Derived metrics in historical data and charts**
- [x] **Charts page view toggle (Primary/Derived/All metrics)**
- [x] **MQTT publishing of all derived metrics with HA Discovery**
- [x] **Color-coded metric status indicators**

### 🚧 In Progress / Planned
- [ ] WebSocket live feed (currently using HTTP polling - works but could be more efficient)
- [ ] Flutter dashboard MVP (cross-platform mobile/desktop app)
- [ ] Output control with failsafes (relay/driver support)
- [ ] Scheduled automation and rules engine
- [ ] Alert thresholds and notifications
- [ ] User-configurable sampling rate (currently 5 seconds, hardcoded)
- [ ] Long-term logging (flash/SD card/remote database)
- [ ] TLS/SSL support for secure MQTT
- [ ] OTA (Over-The-Air) firmware updates
- [ ] Multi-device support and device discovery
- [ ] Additional sensor integrations (flow, level, leak detection)
- [ ] Migration to ESP-IDF framework for production robustness

---

## Troubleshooting

### WiFi Connection Issues
**Device stuck in AP mode:**
- Verify WiFi credentials are correct
- Check WiFi signal strength (device may be too far from router)
- Ensure WiFi network is 2.4GHz (ESP32-C3 doesn't support 5GHz)
- Check router logs to see if device MAC is being blocked

**Cannot access `http://aquarium.local`:**
- mDNS may not work on all networks/routers
- Use direct IP address instead (shown in serial monitor)
- On Windows, install Bonjour service for mDNS support
- On Android, mDNS support varies by browser (Chrome usually works)

**Forgot WiFi credentials / need to reset:**
- Reflash firmware to clear NVS
- Or add a reset button/command in future version

### Sensor Issues
**"Failed to initialize POET sensor":**
- Check I2C wiring (SDA/SCL swapped?)
- Verify sensor power (3.3V, adequate current)
- Check I2C address (`0x1F`) with I2C scanner
- Verify pull-up resistors on SDA/SCL lines
- Check I2C bus speed (should be ≤400kHz)

**Sensor readings seem wrong:**
- pH and EC require calibration (see Calibration section)
- Temperature readings should be reasonably accurate without calibration
- ORP readings are raw millivolts (reference electrode dependent)

### Web Interface Issues
**Page won't load:**
- Check device is powered on and connected to WiFi
- Verify you're on the same network as the device
- Try direct IP instead of mDNS hostname
- Clear browser cache

**Sensor data shows "--" or doesn't update:**
- Check serial monitor for sensor errors
- Verify POET sensor is properly initialized
- Web interface will still load even if sensor fails

### Build/Upload Issues
**PlatformIO build errors:**
- Ensure PlatformIO is up to date
- Try `pio pkg update` to update libraries
- Clean build: `pio run -t clean`

**Upload fails:**
- Check USB cable (some are charge-only)
- Verify correct COM port selected
- Try putting ESP32-C3 in bootloader mode (hold BOOT button while connecting)
- Check drivers for USB-to-serial chip

### MQTT Issues
**MQTT shows "Disconnected" or "Connection failed":**
- Verify MQTT broker is running and accessible on the network
- Check broker IP/hostname is correct in configuration
- Ensure broker port is correct (usually 1883 for unencrypted)
- Verify username/password if authentication is required
- Check firewall rules aren't blocking MQTT traffic
- Test broker connectivity from another MQTT client (MQTT Explorer, mosquitto_sub)

**MQTT connects but data not appearing in Home Assistant:**
- Verify MQTT integration is configured in Home Assistant
- Check that HA is connected to the same MQTT broker
- Enable "Home Assistant MQTT Discovery" in aquarium controller settings
- Look in HA's MQTT integration for discovered devices
- Check HA logs for MQTT discovery messages
- Verify topics in MQTT Explorer match expected format

**MQTT "Enabled" checkbox doesn't stay checked:**
- Make sure you click "Save MQTT Configuration" after enabling
- Check serial monitor for NVS save errors
- Verify device has sufficient free flash memory

**High latency or missed messages:**
- Reduce publish interval to reduce network load
- Check WiFi signal strength (shown on dashboard)
- Verify MQTT broker isn't overloaded
- Consider QoS settings (future enhancement)

**Testing MQTT without Home Assistant:**
Use MQTT Explorer or command-line tools:
```bash
# Subscribe to all aquarium topics
mosquitto_sub -h <broker_ip> -t "aquarium/#" -v

# Subscribe to specific sensor
mosquitto_sub -h <broker_ip> -t "aquarium/aquarium/telemetry/temperature" -v

# With authentication
mosquitto_sub -h <broker_ip> -u username -P password -t "aquarium/#" -v
```

---

## Contributing
- Issues and PRs welcome.
- Please include:
  - hardware revision
  - firmware version
  - reproduction steps

---

## License
This project is released under the **MIT License**.

You are free to use, modify, distribute, and sublicense the software, including for commercial purposes, provided the original copyright notice and license text are included.
