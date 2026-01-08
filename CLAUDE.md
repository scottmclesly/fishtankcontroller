# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based wireless aquarium controller for freshwater/saltwater tanks. Built around the **Sentron POET pH/ORP/EC/Temperature I2C sensor** with planned support for additional sensors and relay/driver outputs for tank equipment.

**Status:** Work in progress - currently in early development stage with basic project structure.

## Build System & Commands

### Platform: PlatformIO + Arduino Framework

Current target: `seeed_xiao_esp32c3` (ESP32-C3 board)

**Build commands:**
```bash
# Build firmware
pio run

# Build and upload to device
pio run -t upload

# Clean build
pio run -t clean

# Monitor serial output
pio device monitor

# Build + upload + monitor (common workflow)
pio run -t upload && pio device monitor

# Run tests
pio test
```

**Note:** The README recommends **ESP32-S3** as the default MCU choice for production, but current platformio.ini is configured for Seeed XIAO ESP32-C3. Consider updating the board configuration if migrating to S3.

## Architecture

### Local-First Design Philosophy

This system is designed to work fully offline without cloud dependencies:

- **Device ↔ UI:** HTTP REST (config/snapshots) + WebSocket (live telemetry)
- **Device ↔ Home Automation:** MQTT with Home Assistant MQTT Discovery support
- **On-device Web UI:** Static HTML/JS served from LittleFS/SPIFFS, accessible via mDNS (`http://aquarium.local`)
- **Flutter UI:** Cross-platform app using same REST/WebSocket API as Web UI

### Firmware Architecture (Planned)

The firmware will be responsible for:

1. **Sensor acquisition** - Read POET sensor via I2C (address `0x1F`, up to 400 kHz)
2. **Data conversion** - Transform raw sensor data to user-facing units (°C, mV, pH, mS/cm)
3. **Control outputs** - Manage relays/drivers with watchdog/failsafe behavior
4. **APIs** - Serve REST + WebSocket endpoints for UI communication
5. **MQTT publishing** - Publish telemetry and handle command topics
6. **Web UI hosting** - Serve static Web UI from flash filesystem
7. **Provisioning** - Captive portal AP mode for initial Wi-Fi setup

### POET I2C Sensor Protocol

**Critical implementation details:**

- **I2C Address:** `0x1F` (7-bit)
- **Bus speed:** Up to 400 kHz
- **Voltage:** 3.3V logic (use level shifting if needed)
- **Pull-ups:** External pull-ups recommended

**Measurement protocol:**
1. Write a command byte to `0x1F` to start measurement
2. Command bits (LSB → MSB):
   - bit0: temperature
   - bit1: ORP
   - bit2: pH
   - bit3: EC
   - bit4-7: reserved (must be 0)
3. Read returns variable-length reply of 32-bit little-endian signed integers

Full protocol details: [docs/Sonetron ConeFET I2C protocol.pdf](docs/Sonetron%20ConeFET%20I2C%20protocol.pdf)

## Project Structure

```
/src              - Main firmware source (currently stub Arduino code)
/lib              - Custom libraries (empty, ready for POET driver, etc.)
/include          - Header files
/test             - Unit tests
/docs             - Documentation (includes POET I2C protocol PDF)
/platformio.ini   - PlatformIO build configuration
```

**Planned structure expansion** (from README):
```
/firmware         - ESP32 firmware (current root is already firmware)
/app              - Flutter cross-platform UI (not yet created)
/web              - Static Web UI files (to be served by MCU)
/hardware         - Schematics, BOM, enclosure designs
/scripts          - Build/deployment automation
```

## Development Considerations

### Framework Choice

- **Current:** Arduino framework (for rapid prototyping)
- **Recommended for production:** ESP-IDF (better for networking, OTA, long-term robustness)
- Consider migration path to ESP-IDF when core functionality stabilizes

### Safety & Failsafes

This controller manages life-support equipment and potentially mains-powered devices:

- **Always** define safe default states for outputs on boot/reset
- Implement watchdog behavior for critical outputs
- Consider galvanic isolation between sensor medium and other equipment
- Use proper enclosures, fusing, and strain relief for mains-powered equipment

### Calibration Requirements

**pH and EC sensors require calibration for meaningful values:**

- **pH:** 1-point (offset) or 2-point (offset + slope) using known buffer solutions
- **EC:** Cell constant calibration using known conductivity solution with temperature compensation
- Calibration data must be persisted to flash/EEPROM

### MQTT Implementation ✅

**Status:** FULLY IMPLEMENTED

**Base topic:** `aquarium/<device_id>/...`

**Published topics:**
- `aquarium/<device_id>/telemetry/temperature` - Temperature in °C (float)
- `aquarium/<device_id>/telemetry/orp` - ORP in mV (float)
- `aquarium/<device_id>/telemetry/ph` - pH value (float)
- `aquarium/<device_id>/telemetry/ec` - EC in mS/cm (float)
- `aquarium/<device_id>/telemetry/sensors` - Combined JSON payload (all sensors)

**Home Assistant Discovery topics:**
- `homeassistant/sensor/<device_id>/temperature/config`
- `homeassistant/sensor/<device_id>/orp/config`
- `homeassistant/sensor/<device_id>/ph/config`
- `homeassistant/sensor/<device_id>/ec/config`

**Implementation details:**
- Location: `lib/MQTTManager/` (MQTTManager.h/cpp)
- Library: PubSubClient v2.8
- Configuration storage: ESP32 NVS (persistent)
- Features:
  - Configurable broker host/port
  - Username/password authentication
  - Configurable device ID and publish intervals
  - Home Assistant MQTT Discovery
  - Automatic reconnection (5-second retry interval)
  - Web-based configuration interface
  - Real-time connection status monitoring

**Combined telemetry payload example:**
```json
{
  "temperature_c": 25.5,
  "orp_mv": 350.2,
  "ph": 7.8,
  "ec_ms_cm": 1.234,
  "valid": true,
  "timestamp": 123456789
}
```

**Configuration via Web UI:**
Navigate to `http://aquarium.local/calibration` → MQTT Configuration section

## Current Implementation Status

### ✅ Completed Features

1. ✅ **MCU + firmware framework** - ESP32-C3 with Arduino framework
2. ✅ **POET driver** - Full I2C communication with raw reads (address 0x1F, 400kHz)
3. ✅ **WiFi stack** - Connection management, NVS credential storage, AP provisioning mode
4. ✅ **REST API** - Comprehensive endpoints for sensors, history, calibration, export, MQTT config
5. ✅ **Web UI** - Dashboard, Charts, Calibration pages with dark/light themes
6. ✅ **Data history** - 288-point circular buffer with 5-second intervals
7. ✅ **Data export** - CSV and JSON export via web UI and console commands
8. ✅ **Calibration system** - pH (1-point/2-point) and EC (cell constant) with NVS persistence
9. ✅ **MQTT publisher** - Full implementation with broker configuration
10. ✅ **MQTT telemetry** - Individual sensor topics + combined JSON payload
11. ✅ **Home Assistant MQTT Discovery** - Automatic entity registration
12. ✅ **MQTT configuration UI** - Web-based setup with connection testing
13. ✅ **Status monitoring** - Real-time MQTT connection indicators on all pages
14. ✅ **Console interface** - Serial commands for debugging and data export

### 🚧 Planned Features

- WebSocket live feed (currently using HTTP polling)
- Flutter dashboard MVP
- Output control with failsafes
- Scheduled automation and rules engine
- Long-term logging (flash/SD/remote)
- TLS/SSL for secure MQTT
- OTA firmware updates

## Key Files

### Documentation
- [README.md](README.md) - Comprehensive project documentation with full feature details
- [CLAUDE.md](CLAUDE.md) - This file - development context for AI assistants
- [docs/Sonetron ConeFET I2C protocol.pdf](docs/Sonetron%20ConeFET%20I2C%20protocol.pdf) - POET sensor I2C protocol specification

### Build Configuration
- [platformio.ini](platformio.ini) - PlatformIO build config (ESP32-C3, libraries)

### Firmware Core
- [src/main.cpp](src/main.cpp) - Main firmware entry point, sensor loop, MQTT integration

### Libraries
- [lib/WiFiManager/](lib/WiFiManager/) - WiFi connection and provisioning AP
- [lib/WebServer/](lib/WebServer/) - Async web server, REST API, HTML pages
- [lib/CalibrationManager/](lib/CalibrationManager/) - pH/EC calibration with NVS storage
- [lib/MQTTManager/](lib/MQTTManager/) - MQTT client, broker config, HA Discovery

### Web UI Components
- [lib/WebServer/WebServer.cpp](lib/WebServer/WebServer.cpp) - Dashboard, calibration page generation
- [lib/WebServer/charts_page.h](lib/WebServer/charts_page.h) - Charts page HTML with Chart.js
