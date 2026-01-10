# Development Guide

## Project Overview

ESP32-based aquarium controller built around the Sentron POET pH/ORP/EC/Temperature I2C sensor with web interface and MQTT integration.

**Status:** Working prototype - WiFi, sensor monitoring, web UI, and MQTT operational

## Build System

### Platform: PlatformIO + Arduino Framework

**Current configuration:**
- **Board:** `seeed_xiao_esp32c3` (ESP32-C3)
- **Framework:** Arduino (for rapid prototyping)
- **Build tool:** PlatformIO

**Recommended production platform:** ESP32-S3 (better for rich Web UI and multiple sensors)

### Build Commands

```bash
# Build firmware
pio run

# Build and upload
pio run -t upload

# Clean build
pio run -t clean

# Monitor serial output
pio device monitor

# Build + upload + monitor (common workflow)
pio run -t upload && pio device monitor

# Run tests
pio test

# Update dependencies
pio pkg update

# Erase flash (including NVS)
pio run -t erase
```

### PlatformIO Configuration

**File:** `platformio.ini`

**Key settings:**
- Platform: espressif32
- Framework: arduino
- Board: seeed_xiao_esp32c3
- Monitor speed: 115200 baud
- Libraries: Listed in `lib_deps`

## Architecture

### Local-First Design

The system operates fully offline without cloud dependencies:

**Device ↔ UI:**
- HTTP REST for configuration and snapshots
- WebSocket for live telemetry (planned, currently HTTP polling)
- Static Web UI served from device

**Device ↔ Automation:**
- MQTT with Home Assistant MQTT Discovery
- Works with Home Assistant, Node-RED, etc.

### Firmware Responsibilities

1. **Sensor Acquisition** - Read POET sensor via I2C
2. **Data Conversion** - Transform raw data to user units
3. **Control Outputs** - Manage relays/drivers (planned)
4. **REST API** - Serve endpoints for UI
5. **MQTT Publishing** - Telemetry to automation systems
6. **Web UI Hosting** - Serve static pages
7. **Provisioning** - Captive portal for WiFi setup

### Module Structure

```
/src                   - Main firmware entry point
  main.cpp             - Sensor loop, WiFi, MQTT integration

/lib                   - Custom libraries
  /WiFiManager         - WiFi connection and AP provisioning
  /WebServer           - Async web server, REST API, HTML pages
  /CalibrationManager  - pH/EC calibration with NVS storage
  /MQTTManager         - MQTT client and HA Discovery

/include               - Header files
/test                  - Unit tests
/docs                  - Documentation
/platformio.ini        - Build configuration
```

## POET I2C Sensor Protocol

### Connection Specifications

- **I2C Address:** 0x1F (7-bit, fixed)
- **Bus Speed:** Up to 400 kHz
- **Logic Levels:** 3.3V only
- **Pull-ups:** External 4.7kΩ recommended

### Measurement Protocol

**Command byte structure** (LSB → MSB):
- Bit 0: Temperature
- Bit 1: ORP
- Bit 2: pH
- Bit 3: EC
- Bits 4-7: Reserved (must be 0)

**Example:** Command `0x0F` (binary 00001111) reads all four sensors

**Response:** Variable-length reply of 32-bit little-endian signed integers

**Full protocol:** See [docs/Sonetron ConeFET I2C protocol.pdf](Sonetron%20ConeFET%20I2C%20protocol.pdf)

## Project Status

### ✅ Completed Features

**Core Infrastructure:**
- [x] ESP32-C3 with Arduino framework
- [x] WiFi connection with NVS credential storage
- [x] AP provisioning mode for initial setup
- [x] mDNS responder (`http://aquarium.local`)

**Sensor System:**
- [x] POET I2C driver (full protocol implementation)
- [x] Temperature, ORP, pH, EC measurements
- [x] pH calibration (1-point and 2-point)
- [x] EC calibration (cell constant)
- [x] Calibration storage in NVS

**Data Management:**
- [x] 288-point circular buffer (24 minutes @ 5-second intervals)
- [x] CSV and JSON export
- [x] Historical data tracking
- [x] Derived metrics calculations (TDS, CO₂, NH₃, DO, stocking)

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
- [x] Web-based MQTT configuration
- [x] Real-time connection status

**REST API:**
- [x] `/api/sensors` - Current readings
- [x] `/api/history` - Historical data
- [x] `/api/export/csv` and `/api/export/json`
- [x] `/api/metrics/derived` - Calculated metrics
- [x] `/api/settings/tank` - Tank configuration
- [x] `/api/settings/fish` - Fish profiles
- [x] `/api/mqtt/config` and `/api/mqtt/status`

**Console Interface:**
- [x] Serial command interface
- [x] Status, dump (CSV/JSON), help commands

### 🚧 Planned Features

**Short-term:**
- [ ] WebSocket live feed (replace HTTP polling)
- [ ] User-configurable sampling rate
- [ ] Configuration export/import (JSON)
- [ ] Console commands for WiFi/MQTT reset

**Medium-term:**
- [ ] Output control (relays/drivers)
- [ ] Scheduled automation
- [ ] Alert thresholds and notifications
- [ ] Long-term data logging (flash/SD)
- [ ] OTA firmware updates

**Long-term:**
- [ ] Flutter cross-platform app
- [ ] TLS/SSL for MQTT
- [ ] Multi-device support
- [ ] Additional sensor integrations
- [ ] Migration to ESP-IDF framework

## Roadmap

### Phase 1: Monitoring (COMPLETE ✅)
- POET sensor integration
- Web UI for monitoring
- MQTT publishing
- Calibration system

### Phase 2: Control (IN PROGRESS)
- Relay/driver outputs
- Manual control via web UI
- Safety interlocks and failsafes
- Watchdog timers

### Phase 3: Automation (PLANNED)
- Scheduled events (lights, feeding)
- Conditional rules engine
- Alert thresholds
- Notification system

### Phase 4: Advanced Features (PLANNED)
- Flutter mobile app
- Long-term data logging
- Multi-device support
- OTA updates
- TLS/SSL security

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
   pio test
   ```

### Development Workflow

1. **Create feature branch:**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make changes and test**

3. **Commit with clear messages:**
   ```bash
   git commit -m "feat: Add output control support"
   ```

4. **Push and create pull request:**
   ```bash
   git push origin feature/your-feature-name
   ```

### Code Style

**Follow existing patterns:**
- Use existing code style and conventions
- Keep functions focused and single-purpose
- Add comments for complex logic
- Update documentation for user-facing changes

**Avoid over-engineering:**
- Don't add unnecessary abstractions
- Keep solutions simple and direct
- Only add features that are needed
- Don't optimize prematurely

### Testing

**Before submitting PR:**
- Build successfully without warnings
- Test on actual hardware if possible
- Verify web UI still works
- Check serial monitor for errors
- Test affected features end-to-end

**Future:** Automated testing framework

### Pull Request Guidelines

**Include in PR description:**
- Hardware used for testing
- Firmware version/commit
- What was changed and why
- Testing performed
- Any breaking changes

**Good PR:**
- Focused on single feature/fix
- Clear commit messages
- Tested on hardware
- Documentation updated
- No unrelated changes

## Framework Considerations

### Current: Arduino Framework

**Pros:**
- Rapid prototyping
- Large library ecosystem
- Easy to understand
- Good for MVP

**Cons:**
- Less control over low-level features
- Potential performance limitations
- OTA and networking less robust

### Future: ESP-IDF

**When to migrate:**
- Approaching production readiness
- Need better networking stack
- Want robust OTA updates
- Performance becomes critical

**Migration plan:**
- Core functionality first (sensors, I2C)
- WiFi and networking
- Web server (AsyncTCP → ESP-IDF HTTP server)
- MQTT (migrate to ESP-MQTT)
- Web UI (LittleFS/SPIFFS compatible)

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

### Development Best Practices

**When adding output control:**
- Define safe default states
- Implement failsafe behavior
- Add interlock logic
- Test power loss scenarios
- Document safety features

## Resources

### Documentation
- [Hardware Guide](HARDWARE.md) - Hardware setup and wiring
- [Installation Guide](INSTALLATION.md) - Firmware installation
- [Calibration Guide](CALIBRATION.md) - Sensor calibration
- [Web UI Guide](WEB_UI.md) - Web interface features
- [MQTT Guide](MQTT.md) - MQTT integration
- [Configuration Guide](CONFIGURATION.md) - Configuration options
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues

### External Resources
- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [AsyncWebServer Library](https://github.com/me-no-dev/ESPAsyncWebServer)
- [PubSubClient (MQTT)](https://github.com/knolleary/pubsubclient)

### Related Projects
- [ESPHome](https://esphome.io/) - ESP-based home automation
- [WLED](https://github.com/Aircoookie/WLED) - ESP LED controller (good architecture reference)
- [Tasmota](https://tasmota.github.io/) - ESP firmware for smart devices

## License

**Apache License 2.0**

See [LICENSE](../LICENSE) for full terms.

**Key Points:**
- ✅ Free for personal, educational, and commercial use
- ✅ Modifications and derivatives allowed
- ✅ Attribution required
- ✅ Patent grant included
- ⚠️ Trademark usage restricted (see [TRADEMARK.md](../TRADEMARK.md))

**Commercial Use:** Permitted under Apache 2.0. See [COMMERCIAL.md](../COMMERCIAL.md) for guidance and partnership opportunities.
