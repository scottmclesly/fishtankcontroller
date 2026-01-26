# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based wireless aquarium controller for freshwater/saltwater tanks. Built around the **Sentron POET pH/ORP/EC/Temperature I2C sensor** with web interface, MQTT integration, OTA updates, and Home Assistant support.

**Status:** Working prototype - WiFi connectivity, sensor monitoring, web UI, MQTT, and OTA operational.

## Build System & Commands

### Platform: PlatformIO + ESP-IDF 5.5

Primary target: `seeed_xiao_esp32c6` (ESP32-C6 board)
Secondary target: `seeed_xiao_esp32c3` (ESP32-C3 board)

**Build commands:**
```bash
# Build firmware (defaults to ESP32-C6)
pio run

# Build for specific target
pio run -e seeed_xiao_esp32c6    # ESP32-C6
pio run -e seeed_xiao_esp32c3    # ESP32-C3

# Build and upload to device
pio run -t upload

# Clean build
pio run -t clean

# Monitor serial output
pio device monitor

# Build + upload + monitor (common workflow)
pio run -t upload && pio device monitor

# Run native tests (on host computer)
pio test -e native

# Run embedded tests (on device)
pio test -e esp32c6_test
```

## Architecture

### Local-First Design Philosophy

This system is designed to work fully offline without cloud dependencies:

- **Device <-> UI:** HTTP REST (config/snapshots) + WebSocket (live telemetry)
- **Device <-> Home Automation:** MQTT with Home Assistant MQTT Discovery support
- **On-device Web UI:** Static HTML/JS served from LittleFS, accessible via mDNS (`http://aquarium.local`)

### Firmware Architecture

The ESP-IDF firmware implements:

1. **Sensor acquisition** - Read POET sensor via I2C (address `0x1F`, 400 kHz)
2. **Data conversion** - Transform raw sensor data to user-facing units
3. **Derived metrics** - Calculate TDS, CO2, NH3, DO, stocking density
4. **Warning system** - Threshold monitoring with hysteresis
5. **APIs** - HTTP REST endpoints for UI communication
6. **MQTT publishing** - Publish telemetry with Home Assistant Discovery
7. **Web UI hosting** - Serve static Web UI from flash filesystem
8. **Provisioning** - Captive portal AP mode for initial Wi-Fi setup
9. **OTA updates** - Over-the-air firmware updates with rollback protection
10. **Display** - SSD1306 OLED driver for local status display

### FreeRTOS Task Structure

```
sensor_task     - POET sensor reading, calibration, derived metrics (priority 5)
http_task       - Web server and REST API (priority 4)
mqtt_task       - MQTT client and publishing (priority 3)
display_task    - OLED display cycling (priority 2)
monitor_task    - Heap monitoring and leak detection (priority 1)
```

### POET I2C Sensor Protocol

**Critical implementation details:**

- **I2C Address:** `0x1F` (7-bit)
- **Bus speed:** 400 kHz
- **Voltage:** 3.3V logic
- **Pull-ups:** External pull-ups recommended

**Measurement protocol:**
1. Write command byte to `0x1F` to start measurement
2. Command bits (LSB -> MSB): bit0=temp, bit1=ORP, bit2=pH, bit3=EC
3. Wait ~2.8 seconds for measurement
4. Read returns variable-length reply of 32-bit little-endian signed integers

## Project Structure

```
/src/main.c           - Application entry point (ESP-IDF convention: in src/)
/components/          - ESP-IDF components (modular firmware)
  /poet_sensor/       - POET I2C sensor driver
  /calibration/       - pH/EC calibration with NVS persistence
  /warning_manager/   - Threshold monitoring and alerts
  /derived_metrics/   - TDS, CO2, NH3, DO calculations
  /tank_settings/     - Tank configuration storage
  /wifi_manager/      - WiFi connection and AP provisioning
  /mqtt_manager/      - MQTT client with HA Discovery
  /http_server/       - Async web server, REST API, WebSocket
  /display_driver/    - SSD1306 OLED driver
  /data_history/      - Circular buffer for historical data
  /ota_manager/       - OTA updates with rollback protection
/test/                - Unit tests
  /test_native/       - Native tests (run on host)
  /test_embedded/     - Embedded tests (run on device)
/docs/                - Documentation
/platformio.ini       - PlatformIO build configuration
/partitions.csv       - Custom partition table with OTA support
/sdkconfig.defaults   - ESP-IDF SDK configuration
```

## Development Considerations

### ESP-IDF Patterns

- Use `ESP_LOGI/W/E()` for logging (not printf)
- Use `esp_err_t` return types with `ESP_OK`, `ESP_FAIL`, etc.
- Store config in NVS (Non-Volatile Storage)
- Use FreeRTOS primitives (tasks, queues, event groups, mutexes)
- Components are in `/components/` with CMakeLists.txt

### Safety & Failsafes

This controller manages life-support equipment:

- **Always** define safe default states for outputs on boot/reset
- Implement watchdog behavior for critical outputs
- Consider galvanic isolation between sensor medium and equipment
- Use proper enclosures, fusing, and strain relief for mains-powered equipment

### Calibration Requirements

**pH and EC sensors require calibration:**

- **pH:** 1-point (offset) or 2-point (offset + slope) using known buffer solutions
- **EC:** Cell constant calibration using known conductivity solution
- Calibration data persisted to NVS

### MQTT Implementation

**Base topic:** `aquarium/<unit_name>-<chip_id>/...`

**Published topics:**
- `aquarium/<unit>-<id>/telemetry/temperature` - Temperature in C
- `aquarium/<unit>-<id>/telemetry/orp` - ORP in mV
- `aquarium/<unit>-<id>/telemetry/ph` - pH value
- `aquarium/<unit>-<id>/telemetry/ec` - EC in mS/cm
- `aquarium/<unit>-<id>/telemetry/sensors` - Combined JSON payload

**Home Assistant Discovery:** Auto-registers entities at `homeassistant/sensor/<unit>-<id>/*/config`

## Current Implementation Status

### Completed Features

1. **MCU + firmware framework** - ESP32-C6/C3 with ESP-IDF 5.5
2. **POET driver** - Full I2C communication (address 0x1F, 400kHz)
3. **WiFi stack** - Connection management, NVS credentials, AP provisioning
4. **REST API** - Endpoints for sensors, history, calibration, export, settings
5. **Web UI** - Dashboard, Charts, Calibration pages with dark/light themes
6. **Data history** - 288-point circular buffer with statistics
7. **Data export** - CSV and JSON export
8. **Calibration system** - pH (1-point/2-point) and EC with NVS persistence
9. **MQTT publisher** - Full implementation with broker configuration
10. **Home Assistant Discovery** - Automatic entity registration
11. **OTA updates** - HTTP download, direct upload, rollback protection
12. **Display driver** - SSD1306 OLED with metric cycling
13. **Memory monitoring** - Heap tracking with leak detection
14. **Unit tests** - Native tests for calculations, embedded tests for components

### Planned Features

- WebSocket live feed (currently using HTTP polling)
- Output control with failsafes
- Scheduled automation and rules engine
- Long-term logging (flash/SD/remote)
- TLS/SSL for secure MQTT

## Key Files

### Build Configuration
- [platformio.ini](platformio.ini) - PlatformIO config (ESP32-C6/C3, ESP-IDF)
- [partitions.csv](partitions.csv) - Partition table with OTA support
- [sdkconfig.defaults](sdkconfig.defaults) - ESP-IDF SDK defaults

### Firmware Core
- [src/main.c](src/main.c) - Application entry, FreeRTOS tasks, initialization

### Components
- [components/poet_sensor/](components/poet_sensor/) - POET I2C sensor driver
- [components/calibration/](components/calibration/) - pH/EC calibration
- [components/wifi_manager/](components/wifi_manager/) - WiFi connection/provisioning
- [components/mqtt_manager/](components/mqtt_manager/) - MQTT client, HA Discovery
- [components/http_server/](components/http_server/) - Web server, REST API, handlers
- [components/ota_manager/](components/ota_manager/) - OTA updates with rollback
- [components/display_driver/](components/display_driver/) - SSD1306 OLED driver
- [components/data_history/](components/data_history/) - Historical data buffer
- [components/derived_metrics/](components/derived_metrics/) - Calculated metrics
- [components/warning_manager/](components/warning_manager/) - Threshold monitoring
- [components/tank_settings/](components/tank_settings/) - Tank configuration

### Tests
- [test/test_native/](test/test_native/) - Host-based unit tests
- [test/test_embedded/](test/test_embedded/) - Device-based integration tests
- [docs/TESTING.md](docs/TESTING.md) - Testing guide

### Documentation
- [README.md](README.md) - Project overview and quick start
- [docs/INSTALLATION.md](docs/INSTALLATION.md) - Installation guide
- [docs/HARDWARE.md](docs/HARDWARE.md) - Hardware setup
- [docs/WEB_UI.md](docs/WEB_UI.md) - Web interface guide
- [docs/CALIBRATION.md](docs/CALIBRATION.md) - Calibration procedures
- [docs/MQTT.md](docs/MQTT.md) - MQTT and Home Assistant setup
- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) - Development guide
