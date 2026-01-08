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

### MQTT Topics Structure

Base topic: `aquarium/<device_id>/...`

Suggested topic hierarchy:
- `telemetry/raw` - Raw sensor readings
- `telemetry/normalized` - Engineering units (°C, mV, pH, mS/cm)
- `state/outputs` - Current output states
- `cmd/output/<name>` - Set output command
- `cmd/config` - Configuration updates

Example normalized telemetry payload:
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

## Current Roadmap (from README)

Priority order for implementation:

1. ✅ Decide MCU + firmware framework (ESP32-S3 + ESP-IDF recommended, but currently using C3 + Arduino)
2. ⏳ POET driver + raw I2C reads
3. ⏳ REST API + WebSocket live feed
4. ⏳ On-device Web UI MVP
5. ⏳ Calibration storage + workflow
6. ⏳ MQTT publisher + command topics
7. ⏳ Home Assistant MQTT Discovery
8. ⏳ Flutter dashboard MVP
9. ⏳ Output control with failsafes
10. ⏳ Logging (flash/SD/remote)

## Key Files

- [README.md](README.md) - Comprehensive project documentation
- [platformio.ini](platformio.ini) - Build configuration
- [src/main.cpp](src/main.cpp) - Main firmware entry point (currently stub code)
- [docs/Sonetron ConeFET I2C protocol.pdf](docs/Sonetron%20ConeFET%20I2C%20protocol.pdf) - POET sensor I2C protocol specification
