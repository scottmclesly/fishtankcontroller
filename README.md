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
- Auto-refresh every 2 seconds via background polling
- Responsive design works on desktop, tablet, and mobile
- Visual warnings for uncalibrated sensors
- WiFi connection status and signal strength

**WiFi Provisioning:**
- Automatic fallback to AP mode when no credentials stored or connection fails
- Network scanning to discover available WiFi networks
- Simple web-based credential entry
- Credentials stored in ESP32 NVS (non-volatile storage)
- Automatic retry logic with configurable timeout

**API Endpoints:**
- `GET /` - Main dashboard (sensor view or provisioning based on mode)
- `GET /api/sensors` - JSON sensor data for programmatic access
- `GET /setup` - Provisioning configuration page
- `POST /save-wifi` - Save WiFi credentials
- `GET /scan` - Scan for available networks

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
### MQTT (planned)
MQTT is the preferred “gateway” into **Home Assistant** and other automation stacks.

- Supports **Home Assistant MQTT Discovery** so entities appear automatically.
- Allows multiple consumers (HA + Node-RED + your own logger) without coupling.

**Base topic**: `aquarium/<device_id>/...`

Suggested topics:
- `.../telemetry/raw` (raw sensor fields)
- `.../telemetry/normalized` (engineering units)
- `.../state/outputs` (current output states)
- `.../cmd/output/<name>` (set output)
- `.../cmd/config` (push config)

### Data model (draft)
Normalized telemetry payload (example):
```json
{
  "ts": 0,
  "temp_c": 0.0,
  "orp_mv": 0.0,
  "ph": 0.0,
  "ec_ms_cm": 0.0,
  "salinity_ppt": null,
  "device": {
    "id": "",
    "fw": ""
  }
}
```

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

### Planned Configuration Options
- MQTT: broker host/port, auth, TLS, base topic
- Sampling rate: User-configurable interval
- Output mapping: names, GPIOs, safety defaults
- Alerts: thresholds for pH/ORP/EC/temperature
- Calibration values: pH offset/slope, EC cell constant

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

### pH
- Record sensor output in known buffer(s).
- Use 1-point (offset) or 2-point (offset + slope) calibration.

### EC / Conductivity
- Calibrate the cell constant using a known conductivity solution.
- Apply temperature compensation (TBD).

---

## Safety + electrical notes
- Treat any mains-powered equipment (heaters, AC pumps) as **high risk**.
- Use proper enclosures, fusing, strain relief, and isolation.
- Consider galvanic isolation between sensor medium and other equipment.
- Define safe default states for outputs on boot/reset.

---

## Roadmap
- [x] Decide MCU + firmware framework (ESP32-C3 + Arduino for prototyping)
- [x] POET driver + raw reads
- [x] WiFi connection stack with credential storage and provisioning AP
- [x] On-device Web UI MVP for sensor monitoring
- [ ] REST API expansion (configuration, calibration endpoints)
- [ ] WebSocket live feed (currently using polling)
- [ ] Calibration storage + workflow
- [ ] MQTT publisher + command topics
- [ ] Home Assistant MQTT Discovery
- [ ] Flutter dashboard MVP
- [ ] Output control with failsafes
- [ ] Logging (flash/SD/remote)

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
