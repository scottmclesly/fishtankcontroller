# Fish Tank Controller

Wireless aquarium controller (freshwater / saltwater) built around the **Sentron POET pH / ORP / EC / Temperature over I2C** sensor, plus additional sensors and relay/driver outputs for tank equipment.

> **Status:** WIP

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
  - **ESP-IDF (recommended)** for long-term robustness, networking, and OTA
  - Arduino framework is acceptable for faster prototyping

---

## Web UI
The controller hosts a Web UI so you can manage it from any device on the same network (phone/tablet/laptop) without installing anything.

### Scope (initial)
- Live dashboard (readings + health)
- Output controls (manual override)
- Config pages (Wi‑Fi, MQTT, sampling rate, alarms)
- Calibration workflows (pH / EC)

### Implementation
- **Simple static HTML + JS** (no framework/build step)
- Served directly from the device (LittleFS/SPIFFS)
- REST for configuration, WebSocket for live updates
- mDNS enabled (`http://aquarium.local`)

### Provisioning
- On first boot or Wi‑Fi failure, the device starts an AP and captive portal
- Portal collects Wi‑Fi credentials and optional MQTT settings
- Device reboots/applies settings and joins the LAN


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
### 1) Hardware
- Wire sensor and verify I2C bus stability.
- Add output drivers and verify isolation and fusing.

### 2) First boot provisioning (AP captive portal)
- Power on the device.
- Join the setup AP (e.g., `Aquarium-Setup-XXXX`).
- Complete the captive portal:
  - connect to Wi‑Fi
  - set device name
  - (optional) configure MQTT
- After it joins your LAN, open `http://aquarium.local` (or the device IP).

### 3) Firmware
- Flash the MCU using PlatformIO.
- Confirm the sensor is detected and readings update.

### 4) UI access
- Open the Web UI in a browser **or**
- Install the Flutter app and connect over the same LAN.

---

---

## Configuration
- Wi‑Fi: SSID / password (via captive portal on first boot)
- Hostname / mDNS name (e.g., `aquarium.local`)
- MQTT: broker host/port, auth, TLS, base topic
- Sampling rate: TBD
- Output mapping: names, GPIOs, safety defaults
- Alerts: thresholds for pH / ORP / EC / temperature
- MQTT: broker host/port, auth, TLS, base topic
- Sampling rate: TBD
- Output mapping: names, GPIOs, safety defaults
- Alerts: thresholds for pH/ORP/EC/temp

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
- [ ] Decide MCU + firmware framework (ESP32-S3 + ESP-IDF recommended)
- [ ] POET driver + raw reads
- [ ] REST API + WebSocket live feed
- [ ] On-device Web UI MVP
- [ ] Calibration storage + workflow
- [ ] MQTT publisher + command topics
- [ ] Home Assistant MQTT Discovery
- [ ] Flutter dashboard MVP
- [ ] Output control with failsafes
- [ ] Logging (flash/SD/remote)

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
