# Development Guide

## Project Overview

ESP32-based aquarium controller built around the Sentron POET pH/ORP/EC/Temperature I2C sensor with web interface, MQTT integration, and OTA updates.

**Status:** Working prototype - WiFi, sensor monitoring, web UI, MQTT, and OTA operational

## Build System

### Platform: PlatformIO + ESP-IDF 5.5

**Supported targets:**
- **Primary:** `seeed_xiao_esp32c6` (ESP32-C6)
- **Secondary:** `seeed_xiao_esp32c3` (ESP32-C3)
- **Future:** ESP32-C5 (when PlatformIO adds support)

### Build Commands

```bash
# Build firmware (defaults to ESP32-C6)
pio run

# Build for specific target
pio run -e seeed_xiao_esp32c6    # ESP32-C6
pio run -e seeed_xiao_esp32c3    # ESP32-C3

# Build and upload
pio run -t upload

# Clean build
pio run -t clean

# Full clean (removes .pio directory)
rm -rf .pio && pio run

# Monitor serial output
pio device monitor

# Build + upload + monitor (common workflow)
pio run -t upload && pio device monitor

# Run native tests (on host computer)
pio test -e native

# Run embedded tests (on device)
pio test -e esp32c6_test

# Erase flash (including NVS)
pio run -t erase
```

### PlatformIO Configuration

**File:** `platformio.ini`

**Key settings:**
- Platform: espressif32
- Framework: espidf (ESP-IDF 5.5)
- Default board: seeed_xiao_esp32c6
- Monitor speed: 115200 baud
- Partition table: Custom with OTA support

**Environments:**
- `seeed_xiao_esp32c6` - Main build target
- `seeed_xiao_esp32c3` - C3 compatibility build
- `native` - Host-based unit tests
- `esp32c6_test` - Device-based integration tests

## Architecture

### Local-First Design

The system operates fully offline without cloud dependencies:

**Device <-> UI:**
- HTTP REST for configuration and snapshots
- WebSocket for live telemetry (planned, currently HTTP polling)
- Static Web UI served from device

**Device <-> Automation:**
- MQTT with Home Assistant MQTT Discovery
- Works with Home Assistant, Node-RED, etc.

### FreeRTOS Task Structure

```
sensor_task     - POET sensor reading, calibration, derived metrics (priority 5)
http_task       - Web server and REST API (priority 4)
mqtt_task       - MQTT client and publishing (priority 3)
display_task    - OLED display cycling (priority 2)
monitor_task    - Heap monitoring and leak detection (priority 1)
```

### Component Structure

```
/src/main.c           - Application entry point, task creation
/components/          - ESP-IDF components (modular firmware)
  /poet_sensor/       - POET I2C sensor driver
  /calibration/       - pH/EC calibration with NVS persistence
  /warning_manager/   - Threshold monitoring and alerts
  /derived_metrics/   - TDS, CO2, NH3, DO calculations
  /tank_settings/     - Tank configuration storage
  /wifi_manager/      - WiFi connection and AP provisioning
  /mqtt_manager/      - MQTT client with HA Discovery
  /http_server/       - HTTP server, REST API, handlers
  /display_driver/    - SSD1306 OLED driver
  /data_history/      - Circular buffer for historical data
  /ota_manager/       - OTA updates with rollback protection
/test/                - Unit tests
  /test_native/       - Native tests (run on host)
  /test_embedded/     - Embedded tests (run on device)
/docs/                - Documentation
/platformio.ini       - PlatformIO build configuration
/partitions.csv       - Custom partition table
/sdkconfig.defaults   - ESP-IDF SDK configuration
```

## ESP-IDF Development Patterns

### Logging

Use ESP-IDF logging macros:
```c
ESP_LOGI(TAG, "Info message: %d", value);
ESP_LOGW(TAG, "Warning message");
ESP_LOGE(TAG, "Error message");
ESP_LOGD(TAG, "Debug message");  // Requires LOG_LOCAL_LEVEL >= DEBUG
```

### Error Handling

Use `esp_err_t` return types:
```c
esp_err_t my_function(void) {
    esp_err_t ret = some_operation();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Operation failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}
```

### NVS (Non-Volatile Storage)

Configuration persistence:
```c
nvs_handle_t handle;
nvs_open("namespace", NVS_READWRITE, &handle);
nvs_set_str(handle, "key", value);
nvs_commit(handle);
nvs_close(handle);
```

### Component CMakeLists.txt

Each component needs a `CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "component.c"
    INCLUDE_DIRS "include"
    REQUIRES log nvs_flash json  # Dependencies
)
```

## POET I2C Sensor Protocol

### Connection Specifications

- **I2C Address:** 0x1F (7-bit, fixed)
- **Bus Speed:** 400 kHz
- **Logic Levels:** 3.3V only
- **Pull-ups:** External 4.7k recommended

### Measurement Protocol

**Command byte structure** (LSB -> MSB):
- Bit 0: Temperature
- Bit 1: ORP
- Bit 2: pH
- Bit 3: EC
- Bits 4-7: Reserved (must be 0)

**Example:** Command `0x0F` (binary 00001111) reads all four sensors

**Response:** Variable-length reply of 32-bit little-endian signed integers

## Testing

### Native Tests

Run on host computer without hardware:
```bash
pio test -e native
```

Tests pure C calculation functions (derived_metrics).

### Embedded Tests

Run on ESP32 device:
```bash
pio test -e esp32c6_test
```

Tests components requiring ESP-IDF (NVS, FreeRTOS).

### Memory Monitoring

Built-in heap tracking in `monitor_task`:
- Logs heap usage every 60 seconds
- Tracks minimum free heap
- Warns on potential memory leaks

## Project Status

### Completed Features

**Core Infrastructure:**
- [x] ESP32-C6/C3 with ESP-IDF 5.5 framework
- [x] WiFi connection with NVS credential storage
- [x] AP provisioning mode for initial setup
- [x] mDNS responder (`http://aquarium.local`)
- [x] SNTP time synchronization
- [x] OTA firmware updates with rollback protection

**Sensor System:**
- [x] POET I2C driver (full protocol implementation)
- [x] Temperature, ORP, pH, EC measurements
- [x] pH calibration (1-point and 2-point)
- [x] EC calibration (cell constant)
- [x] Calibration storage in NVS

**Data Management:**
- [x] 288-point circular buffer
- [x] CSV and JSON export
- [x] Historical statistics (min/max/avg)
- [x] Derived metrics (TDS, CO2, NH3, DO, stocking)

**Web Interface:**
- [x] Live sensor dashboard
- [x] Historical charts with Chart.js
- [x] Calibration interface (tab-based)
- [x] Tank settings configuration
- [x] Fish profile management
- [x] Dark/light theme toggle
- [x] Responsive mobile design

**MQTT Integration:**
- [x] MQTT client with auto-reconnection
- [x] Configurable broker settings
- [x] Individual sensor topics
- [x] Combined JSON payload topic
- [x] Derived metrics publishing
- [x] Home Assistant MQTT Discovery

**Testing:**
- [x] Native unit tests (derived_metrics)
- [x] Embedded integration tests
- [x] Memory usage monitoring

### Planned Features

**Short-term:**
- [ ] WebSocket live feed (replace HTTP polling)
- [ ] User-configurable sampling rate
- [ ] Configuration export/import (JSON)

**Medium-term:**
- [ ] Output control (relays/drivers)
- [ ] Scheduled automation
- [ ] Alert thresholds and notifications
- [ ] Long-term data logging (flash/SD)

**Long-term:**
- [ ] Flutter cross-platform app
- [ ] TLS/SSL for MQTT
- [ ] Multi-device support

## Contributing

### Getting Started

1. **Fork repository**
2. **Clone your fork:**
   ```bash
   git clone https://github.com/yourusername/fishtankcontroller.git
   cd fishtankcontroller
   ```
3. **Install PlatformIO**
4. **Build and test:**
   ```bash
   pio run
   pio test -e native
   ```

### Development Workflow

1. **Create feature branch:**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make changes and test**

3. **Run tests:**
   ```bash
   pio test -e native
   ```

4. **Commit with clear messages:**
   ```bash
   git commit -m "feat: Add output control support"
   ```

5. **Push and create pull request:**
   ```bash
   git push origin feature/your-feature-name
   ```

### Code Style

**Follow ESP-IDF patterns:**
- Use `ESP_LOG*()` for logging
- Use `esp_err_t` return types
- Store config in NVS
- Use FreeRTOS primitives

**Keep it simple:**
- Don't add unnecessary abstractions
- Keep solutions direct
- Only add features that are needed

### Pull Request Guidelines

**Include in PR description:**
- Hardware used for testing
- Firmware version/commit
- What was changed and why
- Testing performed
- Any breaking changes

## Safety Considerations

### Life-Support Systems

This controller may manage equipment supporting living organisms:

**Critical design principles:**
- Safe defaults on boot/reset
- Watchdog timers for critical outputs
- Redundancy for essential functions
- Regular testing of failsafe behavior

### Electrical Safety

**High-voltage equipment:**
- Proper enclosures and fusing
- Galvanic isolation where needed
- Follow local electrical codes
- Professional installation recommended

## Resources

### Documentation
- [Hardware Guide](HARDWARE.md) - Hardware setup and wiring
- [Installation Guide](INSTALLATION.md) - Firmware installation
- [Calibration Guide](CALIBRATION.md) - Sensor calibration
- [Web UI Guide](WEB_UI.md) - Web interface features
- [MQTT Guide](MQTT.md) - MQTT integration
- [Testing Guide](TESTING.md) - Running tests
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues

### External Resources
- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/)

## License

**Apache License 2.0**

See [LICENSE](../LICENSE) for full terms.

**Key Points:**
- Free for personal, educational, and commercial use
- Modifications and derivatives allowed
- Attribution required
- Patent grant included
- Trademark usage restricted (see [TRADEMARK.md](../TRADEMARK.md))
