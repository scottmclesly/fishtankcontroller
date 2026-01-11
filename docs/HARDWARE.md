# Hardware Setup

## Overview

The Fish Tank Controller is built around the **Sentron POET pH/ORP/EC/Temperature I2C sensor** with an ESP32 microcontroller.

## Core Components

### Primary Sensor: Sentron POET
- **Measurements:** pH, ORP (Oxidation-Reduction Potential), EC (Electrical Conductivity), Temperature
- **Interface:** I2C
- **Address:** 0x1F (7-bit)

### Microcontroller: ESP32 Family

**Recommended: ESP32-S3**
- Best all-around choice for controllers with rich Web UI and multiple sensors
- Consider PSRAM variant for better performance
- Typical module: **ESP32-S3-WROOM-1**

**Alternative: ESP32-C3** (current implementation)
- Good low-cost option for minimal UI
- Lower specs but adequate for basic monitoring
- Current development board: **Seeed XIAO ESP32-C3**

**Alternative: ESP32-C6**
- Newer radios (WiFi 6, 802.15.4)
- Good if you need latest wireless standards
- Less mature ecosystem than S3

**If unsure, choose ESP32-S3** for best balance of features and stability.

## I2C Sensor Connection (POET)

### Specifications
- **I2C Address:** 0x1F (7-bit, fixed)
- **Bus Speed:** Up to 400 kHz
- **Logic Levels:** 3.3V (do NOT connect to 5V systems without level shifting)
- **Pull-ups:** External pull-up resistors recommended (typically 4.7kΩ on SDA and SCL)

### Wiring
```
POET Sensor          ESP32
-----------          -----
VCC (3.3V)    -->    3.3V
GND           -->    GND
SDA           -->    GPIO SDA (with 4.7kΩ pull-up to 3.3V)
SCL           -->    GPIO SCL (with 4.7kΩ pull-up to 3.3V)
```

**ESP32-C3 Default I2C Pins (Seeed XIAO):**
- SDA: GPIO6
- SCL: GPIO7

### Measurement Protocol

1. Write a command byte to address `0x1F` to start measurement
2. Command byte structure (LSB → MSB):
   - bit 0: Temperature
   - bit 1: ORP
   - bit 2: pH
   - bit 3: EC
   - bits 4-7: Reserved (must be 0)
3. Read returns variable-length reply of 32-bit little-endian signed integers

**Example:** To read all sensors, send command byte `0x0F` (binary: 00001111)

## Planned Additions

### Additional Sensors (Future)
- Temperature probes (redundancy/multi-zone)
- Water level sensors
- Leak detection
- Flow rate / pressure sensors

### Output Control (Future)
- **Relays:** For AC devices (heaters, pumps, lights)
- **MOSFETs:** For DC devices (LED lighting, DC pumps)
- **Solid-State Relays (SSR):** For quiet, long-life switching
- **Driver ICs:** For dosing pumps, stepper motors

## Power Supply

### Current Requirements
- **ESP32-C3:** ~160mA typical, 350mA peak (WiFi active)
- **ESP32-S3:** ~240mA typical, 500mA peak (WiFi active)
- **POET Sensor:** Low power (< 10mA typical)

### Recommended Power Supply
- **5V USB power supply** (1A minimum, 2A recommended)
- Use quality power adapter to avoid brownouts during WiFi transmission
- Consider dedicated 3.3V regulator if adding multiple sensors

### Power Considerations
- ESP32 has built-in 3.3V regulator (typically fed from 5V USB)
- Avoid powering multiple relays from ESP32 regulator
- Use separate power for high-current loads (relays, pumps)

## Enclosure and Mounting

### Environmental Considerations
- **Humidity:** High humidity near aquarium - consider sealed enclosure
- **Splashing:** Keep electronics away from direct water contact
- **Corrosion:** Saltwater environments require corrosion-resistant materials

### Recommended Approach
- IP65 or higher rated enclosure for electronics
- Cable glands for sensor wiring
- Mount controller above water level
- Keep sensor probe submerged, electronics dry

## Safety Considerations

### Electrical Safety
- **Galvanic Isolation:** Consider isolating sensor ground from mains-powered equipment
- **GFCI Protection:** Use GFCI/RCD outlets for all aquarium equipment
- **Fusing:** Add appropriate fuses to protect circuits
- **Strain Relief:** Secure all cables to prevent accidental disconnection

### Life-Support Systems
This controller may manage life-support equipment for living organisms:

- Define safe default states for all outputs on boot/reset
- Implement watchdog timers for critical equipment
- Consider redundancy for critical functions (heating, aeration)
- Test failsafe behavior regularly

### High-Voltage Equipment
When controlling mains-powered devices:

- Use properly rated relays/SSRs
- Follow local electrical codes
- Consider professional installation for mains wiring
- Use proper enclosures and labeling
- Never work on live circuits

## Next Steps

- [Installation Guide](INSTALLATION.md) - Flash firmware and connect to WiFi
- [Calibration](CALIBRATION.md) - Calibrate pH and EC sensors
- [Configuration](CONFIGURATION.md) - Configure tank settings and MQTT
