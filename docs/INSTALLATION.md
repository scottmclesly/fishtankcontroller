# Installation Guide

## Prerequisites

### Required Hardware
- ESP32 board (ESP32-C3, ESP32-S3, or compatible)
- Sentron POET sensor connected via I2C
- USB cable for programming
- WiFi network (2.4GHz - ESP32 doesn't support 5GHz)

### Required Software
- [PlatformIO](https://platformio.org/) - via VS Code extension or CLI
- Git (to clone repository)

## Step 1: Hardware Setup

1. **Wire POET sensor to ESP32:**
   - Connect VCC to 3.3V
   - Connect GND to GND
   - Connect SDA and SCL to I2C pins (see [Hardware Guide](HARDWARE.md))
   - Add 4.7kΩ pull-up resistors on SDA and SCL lines

2. **Verify connections:**
   - Double-check voltage (3.3V, NOT 5V)
   - Ensure proper polarity
   - Check for short circuits

3. **Connect ESP32 to computer via USB**

## Step 2: Clone and Build Firmware

```bash
# Clone the repository
git clone https://github.com/yourusername/fishtankcontroller.git
cd fishtankcontroller

# Build firmware
pio run

# Build and upload to device
pio run -t upload

# Monitor serial output
pio device monitor
```

**Build Targets:**
- `pio run` - Build firmware only
- `pio run -t upload` - Build and upload to device
- `pio run -t clean` - Clean build files
- `pio device monitor` - Open serial monitor

**Combined workflow:**
```bash
pio run -t upload && pio device monitor
```

## Step 3: First Boot and WiFi Provisioning

On first boot (or when WiFi credentials are not stored), the device enters **provisioning mode**.

### Provisioning Mode Behavior

**Serial output will show:**
```
WiFi credentials not found
Starting provisioning AP...
AP Started: AquariumSetup
IP: 192.168.4.1
```

**The device creates a WiFi access point:**
- **SSID:** `AquariumSetup`
- **Password:** `aquarium123`

### Connect to WiFi

1. **Connect your phone/laptop to the AP:**
   - WiFi network: `AquariumSetup`
   - Password: `aquarium123`

2. **Open web browser and navigate to:**
   - `http://192.168.4.1`

3. **Configure WiFi credentials:**
   - Click **"Scan for Networks"** to see available networks, or
   - Manually enter SSID and password
   - Click **"Connect to WiFi"**

4. **Device restarts and connects:**
   - Watch serial monitor for connection status
   - Device will show assigned IP address
   - Provisioning AP shuts down

### WiFi Connection

**Successful connection shows in serial:**
```
Connecting to WiFi: YourNetwork
WiFi connected!
IP: 192.168.1.100
mDNS responder started: http://aquarium.local
Web server started
```

**If connection fails:**
- Device falls back to provisioning AP after 3 retry attempts
- Check WiFi credentials and try again
- Ensure WiFi network is 2.4GHz (not 5GHz)

## Step 4: Access Web Interface

Once connected to WiFi, access the web interface via:

### Option 1: mDNS Hostname (Recommended)
```
http://aquarium.local
```

**Works on:**
- macOS, iOS (native mDNS support)
- Linux (with Avahi installed)
- Windows (requires Bonjour service, installed with iTunes or separately)
- Android (browser-dependent, Chrome usually works)

### Option 2: Direct IP Address
```
http://192.168.1.100
```
(Replace with your device's actual IP, shown in serial monitor)

**Use this if mDNS doesn't work on your device.**

## Step 5: Verify Sensor Operation

1. **Open web dashboard** (`http://aquarium.local`)

2. **Check sensor readings:**
   - Temperature should show reasonable value (room temperature)
   - ORP shows raw millivolts
   - pH and EC will show values but require calibration for accuracy

3. **Check serial monitor for detailed output:**
```
=== Sensor Reading ===
Temperature: 24.5 °C
ORP: 250.3 mV
pH: 7.2 (uncalibrated)
EC: 1.41 mS/cm (uncalibrated)
```

4. **Note calibration warnings:**
   - pH readings marked as "uncalibrated" until calibration performed
   - EC readings marked as "uncalibrated" until cell constant set
   - See [Calibration Guide](CALIBRATION.md) for calibration procedures

## Step 6: Configure Tank Settings (Optional)

Navigate to `http://aquarium.local/calibration` and configure:

1. **Tank Settings tab:**
   - Tank dimensions and volume
   - Water chemistry parameters (KH, TAN)
   - Fish stocking profile

2. **MQTT Configuration tab** (if using Home Assistant):
   - Enable MQTT publishing
   - Enter broker IP/hostname
   - Configure authentication if required
   - See [MQTT Guide](MQTT.md) for details

## Resetting WiFi Credentials

If you need to change WiFi networks or forgot credentials:

**Method 1: Reflash firmware**
```bash
pio run -t upload
```
This clears NVS (non-volatile storage) and forces provisioning mode.

**Method 2: Clear NVS via serial (future feature)**
- Planned console command to reset WiFi without reflashing

## Troubleshooting

### Upload Fails
- **Check USB cable:** Some cables are charge-only (no data)
- **Verify COM port:** Ensure correct port selected in PlatformIO
- **Enter bootloader mode:** Hold BOOT button while connecting USB
- **Update drivers:** Install USB-to-serial drivers for your board

### Build Errors
```bash
# Update PlatformIO core and libraries
pio pkg update

# Clean and rebuild
pio run -t clean
pio run
```

### WiFi Won't Connect
- **Verify credentials:** Double-check SSID and password
- **Check frequency:** ESP32 only supports 2.4GHz WiFi (not 5GHz)
- **Signal strength:** Move device closer to router
- **Router settings:** Check MAC filtering, firewall rules

### mDNS Not Working
- **Windows:** Install Bonjour service
- **Android:** mDNS support varies by browser (try Chrome)
- **Fallback:** Use direct IP address from serial monitor

### Sensor Initialization Fails
```
Failed to initialize POET sensor
```

**Checklist:**
- Verify I2C wiring (SDA/SCL not swapped)
- Check sensor power (3.3V with adequate current)
- Confirm I2C address 0x1F (use I2C scanner to verify)
- Add pull-up resistors (4.7kΩ on SDA and SCL)
- Check bus speed ≤ 400kHz

See [Troubleshooting Guide](TROUBLESHOOTING.md) for more solutions.

## Next Steps

- [Calibration](CALIBRATION.md) - Calibrate pH and EC sensors for accurate readings
- [Web UI Guide](WEB_UI.md) - Explore web interface features
- [MQTT Setup](MQTT.md) - Integrate with Home Assistant
- [Configuration](CONFIGURATION.md) - Advanced configuration options
