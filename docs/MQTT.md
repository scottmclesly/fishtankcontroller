# MQTT Integration Guide

## Overview

MQTT is the preferred integration method for connecting the Fish Tank Controller to home automation systems like **Home Assistant**, **Node-RED**, and other MQTT-enabled platforms.

**Status:** ✅ Fully implemented

## Features

- ✅ Configurable broker connection (host, port, authentication)
- ✅ Individual sensor topics for each measurement
- ✅ Combined JSON payload topic
- ✅ Derived metrics publishing (TDS, CO₂, NH₃, DO, stocking)
- ✅ Home Assistant MQTT Discovery (automatic entity creation)
- ✅ Automatic reconnection on connection loss
- ✅ Web-based configuration interface
- ✅ Real-time connection status monitoring
- ✅ Persistent configuration in NVS

## Quick Start

### 1. Configure MQTT

Navigate to `http://aquarium.local/calibration` → **MQTT Configuration** tab

**Required settings:**
- ☑️ Enable MQTT Publishing
- Broker Host/IP (e.g., `192.168.1.100` or `homeassistant.local`)
- Broker Port (default: `1883`)
- Device ID (e.g., `aquarium`, `reef_tank`, `freshwater1`)

**Optional settings:**
- Username and Password (if broker requires authentication)
- Publish Interval (default: 5000ms)
- ☑️ Home Assistant MQTT Discovery (enable for automatic HA integration)

### 2. Test Connection

Click **"Test Connection"** to verify settings before saving.

### 3. Save Configuration

Click **"Save MQTT Configuration"** to persist settings to NVS.

### 4. Monitor Status

Check connection status on Dashboard or Charts page (indicator shows connected/disconnected).

## MQTT Topics

### Base Topic Format

All topics use the pattern: `aquarium/<unit_name>-<chip_id>/...`

Where:
- `<unit_name>` is your configured Unit Name, sanitized for MQTT compatibility (lowercase, spaces→underscores, max 24 chars)
- `<chip_id>` is a unique 6-character hardware ID derived from the ESP32's MAC address

**Example:** If Unit Name is "Kate's Aquarium #7" and chip ID is "A1B2C3":
- Topic prefix becomes: `aquarium/kates_aquarium_7-A1B2C3/...`

### Why This Format?

This naming scheme solves the multi-instance problem:

1. **Human-readable**: Your unit name appears in topics for easy identification
2. **Guaranteed unique**: The chip ID suffix ensures no collisions, even if you accidentally name all your controllers the same thing
3. **Home Assistant friendly**: Each device appears with its friendly name while maintaining unique entity IDs

### Published Topics

#### Primary Sensor Topics

Individual float values for easy consumption:

- `aquarium/<unit>-<id>/telemetry/temperature` - Temperature in °C
- `aquarium/<unit>-<id>/telemetry/orp` - ORP in mV
- `aquarium/<unit>-<id>/telemetry/ph` - pH value (0-14)
- `aquarium/<unit>-<id>/telemetry/ec` - Electrical conductivity in mS/cm

**Example message:**
```
Topic: aquarium/kates_aquarium_7-A1B2C3/telemetry/temperature
Payload: 24.5
```

#### Derived Metrics Topics

Calculated water quality metrics:

- `aquarium/<unit>-<id>/telemetry/tds` - Total Dissolved Solids in ppm
- `aquarium/<unit>-<id>/telemetry/co2` - Dissolved CO₂ in ppm
- `aquarium/<unit>-<id>/telemetry/nh3_ratio` - Toxic ammonia ratio (percentage)
- `aquarium/<unit>-<id>/telemetry/nh3_ppm` - Toxic ammonia (NH₃) in ppm
- `aquarium/<unit>-<id>/telemetry/max_do` - Maximum dissolved oxygen in mg/L
- `aquarium/<unit>-<id>/telemetry/stocking` - Stocking density in cm/L

**Example message:**
```
Topic: aquarium/kates_aquarium_7-A1B2C3/telemetry/co2
Payload: 18.5
```

#### Combined JSON Payload

All sensor data in single JSON message:

**Topic:** `aquarium/<unit>-<id>/telemetry/sensors`

**Payload example:**
```json
{
  "temperature_c": 24.5,
  "orp_mv": 250.3,
  "ph": 7.2,
  "ec_ms_cm": 1.41,
  "tds_ppm": 903.4,
  "co2_ppm": 18.5,
  "nh3_ratio": 0.015,
  "nh3_ppm": 0.0045,
  "max_do_mg_l": 8.24,
  "stocking_density": 1.25,
  "valid": true,
  "timestamp": 1736339400
}
```

## Home Assistant Integration

### Automatic Discovery

When **Home Assistant MQTT Discovery** is enabled, the controller automatically publishes configuration messages that create entities in Home Assistant.

**Discovery topic format:**
```
homeassistant/sensor/<unit>-<id>/<metric>/config
```

**Example:** `homeassistant/sensor/kates_aquarium_7-A1B2C3/temperature/config`

### Primary Sensor Entities

Auto-discovered entities for primary sensors:

- `sensor.<unit>_<id>_temperature` - Temperature (°C)
- `sensor.<unit>_<id>_orp` - ORP (mV)
- `sensor.<unit>_<id>_ph` - pH
- `sensor.<unit>_<id>_ec` - Electrical Conductivity (mS/cm)

**Example:** `sensor.kates_aquarium_7_a1b2c3_temperature`

### Derived Metric Entities

Auto-discovered entities for calculated metrics:

- `sensor.<unit>_<id>_tds` - Total Dissolved Solids (ppm)
- `sensor.<unit>_<id>_co2` - Dissolved CO₂ (ppm)
- `sensor.<unit>_<id>_nh3_ratio` - Toxic Ammonia Ratio (%)
- `sensor.<unit>_<id>_nh3_ppm` - Toxic Ammonia (ppm)
- `sensor.<unit>_<id>_max_do` - Max Dissolved Oxygen (mg/L)
- `sensor.<unit>_<id>_stocking` - Stocking Density (cm/L)

### Entity Configuration

Each entity includes:
- **Friendly name** (e.g., "Aquarium Temperature")
- **Unit of measurement** (e.g., "°C", "mV", "ppm")
- **Device class** (for proper HA categorization)
- **State topic** (where values are published)
- **Device info** (groups all entities under one device)

### Home Assistant Setup

**Prerequisites:**
- Home Assistant with MQTT integration configured
- MQTT broker (Mosquitto recommended)

**Steps:**
1. Configure MQTT broker in Home Assistant (if not already done)
2. Enable "Home Assistant MQTT Discovery" in aquarium controller
3. Save MQTT configuration
4. Wait ~30 seconds for discovery messages
5. Check **Settings → Devices & Services → MQTT**
6. Look for device named: "Kate's Aquarium #7" (or your configured device name)
7. All sensors appear automatically under this device

**No manual YAML configuration needed!**

## MQTT Broker Setup

### Using Mosquitto (Recommended)

**Install on Home Assistant:**
1. Go to **Settings → Add-ons**
2. Search for "Mosquitto broker"
3. Click Install
4. Configure (username/password optional but recommended)
5. Start add-on

**Install on standalone system:**
```bash
# Ubuntu/Debian
sudo apt-get install mosquitto mosquitto-clients

# Start service
sudo systemctl start mosquitto
sudo systemctl enable mosquitto
```

### Broker Configuration

**Basic configuration** (`/etc/mosquitto/mosquitto.conf`):
```
listener 1883
allow_anonymous true
```

**With authentication** (recommended):
```
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
```

Create users:
```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd username
```

## Testing MQTT

### Command Line Tools

**Subscribe to all aquarium topics:**
```bash
mosquitto_sub -h <broker_ip> -t "aquarium/#" -v
```

**Subscribe to specific sensor (replace with your unit-id):**
```bash
mosquitto_sub -h <broker_ip> -t "aquarium/kates_aquarium_7-A1B2C3/telemetry/temperature" -v
```

**Subscribe to all units, specific metric:**
```bash
mosquitto_sub -h <broker_ip> -t "aquarium/+/telemetry/temperature" -v
```

**With authentication:**
```bash
mosquitto_sub -h <broker_ip> -u username -P password -t "aquarium/#" -v
```

**Expected output:**
```
aquarium/kates_aquarium_7-A1B2C3/telemetry/temperature 24.5
aquarium/kates_aquarium_7-A1B2C3/telemetry/ph 7.2
aquarium/kates_aquarium_7-A1B2C3/telemetry/orp 250.3
aquarium/kates_aquarium_7-A1B2C3/telemetry/ec 1.41
...
```

### MQTT Explorer (GUI Tool)

[MQTT Explorer](http://mqtt-explorer.com/) provides visual MQTT browsing:

1. Download and install MQTT Explorer
2. Connect to your broker
3. Browse to `aquarium/` topic
4. View real-time updates
5. Inspect message payloads

## Configuration Options

### Via Web Interface

Navigate to `http://aquarium.local/calibration` → **MQTT Configuration** tab

| Setting | Description | Default |
|---------|-------------|---------|
| Enable MQTT | Toggle MQTT publishing on/off | Disabled |
| Broker Host | IP or hostname of MQTT broker | `192.168.1.100` |
| Broker Port | MQTT broker port | `1883` |
| Unit Name | Friendly name for your tank (used in topics, combined with chip ID) | `aquarium` |
| Publish Interval | Milliseconds between publishes | `5000` |
| Username | MQTT authentication username | (empty) |
| Password | MQTT authentication password | (empty) |
| HA Discovery | Enable Home Assistant auto-discovery | Enabled |

**Note:** The Unit Name is sanitized for MQTT compatibility (lowercase, spaces→underscores) and combined with a hardware-derived Chip ID to create unique topic paths. For example, "Kate's Aquarium #7" becomes `kates_aquarium_7-A1B2C3` in topics.

### Persistent Storage

All settings stored in ESP32 NVS and persist across:
- Reboots
- Power cycles
- Firmware updates (if NVS not erased)

## Connection Management

### Automatic Reconnection

If MQTT connection is lost, the system automatically:
1. Detects disconnection
2. Waits 5 seconds
3. Attempts reconnection
4. Repeats until successful

**Status visible on:**
- Dashboard page (connection indicator)
- Charts page (connection indicator)
- Serial monitor (connection logs)

### Connection States

**Connected (Green):**
- Publishing data successfully
- Indicator shows "MQTT: Connected"

**Disconnected (Red):**
- Connection lost or failed
- Indicator shows "MQTT: Disconnected"
- Automatic reconnection in progress

## Publish Intervals

**Configurable publish interval** controls how often data is sent to MQTT:

- **Fast (1000ms / 1 second):** High-frequency monitoring, increased network load
- **Normal (5000ms / 5 seconds):** Default, good balance
- **Slow (30000ms / 30 seconds):** Reduced network load, less frequent updates

**Recommendation:** Use 5 seconds for most applications. Reduce for time-critical monitoring, increase for battery or network-constrained systems.

## Advanced Usage

### Node-RED Integration

Use MQTT nodes to create custom automation:

**Example flow:**
1. MQTT In node subscribed to `aquarium/+/telemetry/ph` (all tanks) or `aquarium/reef_tank-A1B2C3/telemetry/ph` (specific tank)
2. Function node checks if pH < 6.5 or pH > 8.5
3. Send notification or trigger action

### Data Logging

Store historical data in time-series database:

**InfluxDB example:**
1. Use Telegraf MQTT consumer
2. Subscribe to `aquarium/+/telemetry/sensors`
3. Parse JSON and store in InfluxDB
4. Visualize with Grafana

### Multi-Device Support

**Run multiple controllers on the same broker:**

Each controller automatically generates a unique topic structure by combining:
- Your user-configured **Unit Name** (friendly, descriptive)
- The hardware **Chip ID** (6-character hex, unique per ESP32)

**Example setup with 3 tanks:**

| Unit Name | Chip ID | MQTT Topic Prefix |
|-----------|---------|-------------------|
| Reef Tank | A1B2C3 | `aquarium/reef_tank-A1B2C3/...` |
| Quarantine | D4E5F6 | `aquarium/quarantine-D4E5F6/...` |
| Fry Tank | 789ABC | `aquarium/fry_tank-789ABC/...` |

**Collision protection:** Even if you accidentally name all three tanks "Aquarium", they will still have unique topics:
- `aquarium/aquarium-A1B2C3/...`
- `aquarium/aquarium-D4E5F6/...`
- `aquarium/aquarium-789ABC/...`

**In Home Assistant:** Each appears as a separate device with its friendly Unit Name displayed, but with guaranteed unique entity IDs.

**Subscribe to all tanks:**
```bash
mosquitto_sub -h <broker_ip> -t "aquarium/+/telemetry/#" -v
```

## Security Considerations

### Current Limitations
- ❌ No TLS/SSL support (unencrypted MQTT)
- ⚠️ Credentials stored in plain text in NVS
- ⚠️ No certificate-based authentication

### Recommendations
- Use MQTT broker on trusted local network only
- Don't expose broker to internet without VPN
- Use strong username/password if authentication enabled
- Consider network segmentation (IoT VLAN)

### Planned Security Features
- TLS/SSL support for encrypted MQTT
- Certificate-based authentication
- Encrypted credential storage

## Troubleshooting

### Connection Issues

**"MQTT: Disconnected" status:**
- ✓ Verify broker is running and accessible
- ✓ Check broker IP/hostname is correct
- ✓ Ensure port 1883 is open (firewall rules)
- ✓ Test broker with mosquitto_sub from another device
- ✓ Check username/password if authentication enabled

**Connection succeeds but no data in HA:**
- ✓ Verify HA MQTT integration configured
- ✓ Enable "Home Assistant MQTT Discovery"
- ✓ Check HA logs for MQTT discovery messages
- ✓ Use MQTT Explorer to verify topics being published
- ✓ Restart HA MQTT integration if needed

**Connection works but then drops:**
- ✓ Check WiFi signal strength (dashboard indicator)
- ✓ Verify broker isn't limiting connections
- ✓ Check broker logs for errors
- ✓ Increase keepalive timeout in broker config

### Performance Issues

**High network load:**
- Increase publish interval (reduce frequency)
- Disable unused derived metrics (future feature)
- Check for network congestion

**Missed messages:**
- Reduce publish interval (may worsen load)
- Check broker capacity
- Consider QoS settings (future feature)

## Next Steps

- [Web UI Guide](WEB_UI.md) - Monitor MQTT status in web interface
- [Configuration](CONFIGURATION.md) - Advanced configuration options
- [Troubleshooting](TROUBLESHOOTING.md) - More troubleshooting guidance
