# Troubleshooting Guide

## WiFi Connection Issues

### Device Stuck in AP Mode

**Symptoms:**
- Device creates `AquariumSetup` access point repeatedly
- Cannot connect to home WiFi network
- Serial monitor shows connection failures

**Solutions:**

✓ **Verify credentials are correct**
- Re-enter SSID and password carefully
- Check for typos, case sensitivity
- Ensure special characters entered correctly

✓ **Check WiFi signal strength**
- Move device closer to router
- Check for physical obstructions (metal, walls)
- Consider WiFi extender if signal is weak

✓ **Ensure network is 2.4GHz**
- ESP32-C3 doesn't support 5GHz WiFi
- Check router admin panel for frequency
- Separate 2.4GHz and 5GHz SSIDs if needed

✓ **Check router settings**
- Verify MAC filtering isn't blocking device
- Check router logs for connection attempts
- Temporarily disable AP isolation if enabled

### Cannot Access http://aquarium.local

**Symptoms:**
- mDNS hostname doesn't resolve
- Browser shows "site can't be reached"
- Device connected to WiFi successfully

**Solutions:**

✓ **Use direct IP address instead**
- Check serial monitor for assigned IP
- Navigate to `http://192.168.1.100` (or actual IP)
- Bookmark IP for future access

✓ **Install Bonjour service (Windows)**
- Download from Apple or install iTunes
- Restart computer after installation
- Try mDNS hostname again

✓ **Check mDNS support**
- macOS/iOS: Native support, should work
- Linux: Install Avahi (`sudo apt-get install avahi-daemon`)
- Android: Browser-dependent (Chrome usually works)
- Windows: Requires Bonjour

✓ **Network configuration**
- Some routers block mDNS between VLANs
- Ensure device and client on same network segment
- Check router mDNS/Bonjour forwarding settings

### Forgot WiFi Credentials / Need to Reset

**Solutions:**

**Method 1: Reflash firmware (clears NVS)**
```bash
pio run -t erase
pio run -t upload
```

**Method 2: Future reset command**
- Planned: Serial console command to clear WiFi only
- Watch for updates in future releases

## Sensor Issues

### "Failed to Initialize POET Sensor"

**Symptoms:**
- Serial monitor shows initialization failure
- Dashboard shows "--" for all sensor values
- Sensor communication error

**Solutions:**

✓ **Check I2C wiring**
- Verify SDA and SCL not swapped
- Confirm connections are secure
- Check for broken wires or poor solder joints

✓ **Verify sensor power**
- Confirm 3.3V on sensor VCC pin
- Check adequate current available (avoid sharing regulator with high-current loads)
- Measure voltage with multimeter

✓ **Confirm I2C address**
- Run I2C scanner sketch to detect devices
- POET sensor should appear at address `0x1F` (31 decimal)
- If not detected, check wiring and power

✓ **Check pull-up resistors**
- Verify 4.7kΩ pull-ups on SDA and SCL lines
- Some boards have built-in pull-ups (may not be sufficient)
- Add external pull-ups if missing

✓ **Verify I2C bus speed**
- POET supports up to 400kHz
- Some ESP32 boards default to higher speeds
- Check PlatformIO configuration

### Sensor Readings Seem Wrong

**Symptoms:**
- pH shows unreasonable values
- EC readings don't match expectations
- Temperature seems incorrect

**Solutions:**

✓ **pH and EC require calibration**
- See [Calibration Guide](CALIBRATION.md)
- Perform 2-point pH calibration for best accuracy
- Determine EC cell constant with known solution

✓ **Temperature should be reasonably accurate**
- No calibration required for temperature
- Should read ±1°C of actual temperature
- If far off, sensor may be damaged

✓ **ORP readings are raw millivolts**
- Value depends on reference electrode used
- Typical range: -500mV to +500mV
- Readings relative to reference electrode potential

✓ **Check sensor condition**
- Ensure sensor probe is clean
- Check for buildup or contamination
- Rinse with distilled water

## Web Interface Issues

### Page Won't Load

**Symptoms:**
- Browser shows connection error
- Timeout or "can't reach page"
- Blank page or loading forever

**Solutions:**

✓ **Verify device is powered and connected**
- Check power LED on ESP32 board
- Verify serial monitor shows WiFi connected
- Confirm IP address assigned

✓ **Ensure same network**
- Client device and controller must be on same WiFi network
- Check both are on 2.4GHz network
- Verify not on guest network with AP isolation

✓ **Try direct IP instead of mDNS**
- Get IP from serial monitor
- Navigate to `http://192.168.1.100` (or actual IP)
- Rules out mDNS issue

✓ **Clear browser cache**
- Hard refresh: Ctrl+F5 (Windows) or Cmd+Shift+R (Mac)
- Clear cache and cookies
- Try incognito/private browsing mode

### Sensor Data Shows "--" or Doesn't Update

**Symptoms:**
- Dashboard shows "--" instead of values
- Readings don't update
- Web interface loads but no data

**Solutions:**

✓ **Check serial monitor for errors**
- Look for sensor initialization failures
- Check for I2C communication errors
- Verify sensor loop is running

✓ **Verify POET sensor initialized**
- Serial should show successful initialization
- If failed, see sensor troubleshooting above

✓ **Check JavaScript console (browser)**
- Open developer tools (F12)
- Check Console tab for errors
- Look for failed API requests

✓ **Refresh page**
- Simple page refresh may resolve
- Clear cache if persistent

## Build and Upload Issues

### PlatformIO Build Errors

**Symptoms:**
- Compilation fails with errors
- Library not found errors
- Undefined reference errors

**Solutions:**

✓ **Update PlatformIO**
```bash
# Update PlatformIO core
pio upgrade

# Update platform and libraries
pio pkg update
```

✓ **Clean and rebuild**
```bash
pio run -t clean
pio run
```

✓ **Check platformio.ini**
- Verify all required libraries listed
- Check library versions compatible
- Ensure board configuration correct

✓ **Delete .pio folder and rebuild**
```bash
rm -rf .pio
pio run
```

### Upload Fails

**Symptoms:**
- Upload starts but fails partway
- "Failed to connect to ESP32" error
- Timeout during upload

**Solutions:**

✓ **Check USB cable**
- Some cables are charge-only (no data lines)
- Try different USB cable
- Use short cable (< 1m) if possible

✓ **Verify correct COM port**
- Check PlatformIO detects correct port
- Look in Device Manager (Windows) or `ls /dev/tty*` (Linux/Mac)
- Try unplugging and replugging USB

✓ **Enter bootloader mode manually**
- Hold BOOT button while connecting USB
- Or hold BOOT, press RESET, release BOOT
- Try upload again

✓ **Update USB-to-serial drivers**
- CP210x for most ESP32 boards
- CH340 for some Chinese clones
- Download from manufacturer website

## MQTT Issues

### MQTT Shows "Disconnected" or "Connection Failed"

**Symptoms:**
- MQTT status shows disconnected
- Serial monitor shows connection failures
- No data appearing in Home Assistant

**Solutions:**

✓ **Verify broker is running**
- Check broker service status
- Ping broker IP from another device
- Ensure broker isn't crashed or stopped

✓ **Check broker IP/hostname**
- Verify IP address is correct
- Try IP instead of hostname
- Ensure no typos in configuration

✓ **Confirm broker port**
- Standard MQTT: 1883
- MQTT over TLS: 8883 (not supported yet)
- Check broker configuration for actual port

✓ **Verify authentication**
- If broker requires auth, enter username/password
- Check credentials are correct
- Try disabling auth temporarily to test

✓ **Check firewall rules**
- Ensure port 1883 not blocked
- Check router firewall
- Check broker host firewall

✓ **Test from another MQTT client**
```bash
# Test connection with mosquitto_sub
mosquitto_sub -h <broker_ip> -t "test" -v

# With authentication
mosquitto_sub -h <broker_ip> -u username -P password -t "test" -v
```

### MQTT Connects But No Data in Home Assistant

**Symptoms:**
- MQTT connection successful
- Serial shows publishing messages
- No entities appear in Home Assistant

**Solutions:**

✓ **Verify HA MQTT integration configured**
- Settings → Devices & Services → MQTT
- Must be configured and connected
- Check broker connection in HA

✓ **Enable Home Assistant MQTT Discovery**
- In aquarium controller MQTT config
- Toggle "Home Assistant MQTT Discovery" ON
- Save configuration

✓ **Check HA logs for discovery messages**
- Settings → System → Logs
- Look for MQTT discovery messages
- Should show entities being created

✓ **Verify topics with MQTT Explorer**
- Install MQTT Explorer
- Connect to broker
- Browse to `aquarium/` topics
- Verify messages being published

✓ **Restart HA MQTT integration**
- Settings → Devices & Services → MQTT
- Click "..." → Reload
- Wait 30 seconds for re-discovery

### MQTT "Enabled" Checkbox Doesn't Stay Checked

**Symptoms:**
- Enable MQTT, save, but resets to disabled
- Configuration doesn't persist
- MQTT never starts

**Solutions:**

✓ **Click "Save MQTT Configuration" after enabling**
- Enabling alone doesn't save
- Must click save button
- Wait for confirmation message

✓ **Check serial monitor for NVS errors**
- Look for "NVS write failed" messages
- May indicate flash corruption
- Try reflashing firmware

✓ **Verify flash memory not full**
- Check partition table in platformio.ini
- Ensure NVS partition allocated
- Reflash if corrupted

## Data Export Issues

### Export Doesn't Download File

**Symptoms:**
- Click export button, nothing happens
- File doesn't download
- Browser shows error

**Solutions:**

✓ **Check browser pop-up blocker**
- May be blocking download
- Allow downloads from site
- Try different browser

✓ **Check download folder permissions**
- Verify write access to download folder
- Check disk space available

✓ **Try different export format**
- If CSV fails, try JSON
- If JSON fails, try CSV
- Use console export as alternative

### Empty or Incomplete Export

**Symptoms:**
- Export file is empty
- Export has fewer points than expected
- Data seems truncated

**Solutions:**

✓ **Wait for history buffer to populate**
- Buffer fills over time (5-second intervals)
- Need at least a few minutes of runtime
- Maximum 288 points (24 minutes)

✓ **Check sensor is working**
- Verify readings on dashboard
- Ensure sensor initialized correctly
- Check for sensor errors in serial monitor

## Calibration Issues

See [Calibration Guide](CALIBRATION.md) for detailed calibration troubleshooting.

### pH Calibration Issues

**Readings drift during calibration:**
- Solution temperature changing
- Buffer contaminated
- Sensor needs more time to stabilize

**Calibration fails validation:**
- Buffer solutions expired
- Wrong buffer pH values
- Sensor damaged

### EC Calibration Issues

**Unstable readings:**
- Temperature not stable
- Air bubbles on electrode
- Incorrect solution concentration

**Cell constant seems wrong:**
- Verify calibration solution specs
- Check temperature measurement
- Ensure sensor fully immersed

## General Troubleshooting Steps

### Start with the basics:

1. **Check power:**
   - Ensure stable 5V power supply
   - Verify no brownouts during WiFi transmission
   - Check power LED on board

2. **Check connections:**
   - All wiring secure
   - No short circuits
   - Proper polarity

3. **Check serial monitor:**
   - Connect at 115200 baud
   - Look for error messages
   - Verify boot sequence completes

4. **Restart device:**
   - Power cycle (unplug and replug)
   - Wait for full boot
   - Check if issue persists

5. **Reflash firmware:**
   - Clean flash may resolve issues
   ```bash
   pio run -t erase
   pio run -t upload
   ```

6. **Check for updates:**
   - Pull latest code from repository
   - Review changelog for bug fixes
   - Update dependencies

## Getting Help

### Before Asking for Help

**Gather information:**
- Hardware revision (ESP32-C3, ESP32-S3, etc.)
- Firmware version (git commit hash)
- Serial monitor output (full boot sequence + errors)
- Network configuration
- Steps to reproduce issue

### Where to Get Help

**GitHub Issues:**
- Check existing issues first
- Include all information from above
- Provide reproduction steps
- Attach relevant logs

**Serial Monitor Output:**
Always include complete output showing:
```
Boot sequence
WiFi connection
Sensor initialization
Error messages
```

## Common Error Messages

| Error | Meaning | Solution |
|-------|---------|----------|
| `Failed to initialize POET sensor` | I2C communication failure | Check wiring, power, I2C address |
| `WiFi connection failed` | Can't connect to WiFi | Verify credentials, signal, 2.4GHz |
| `MQTT connection failed` | Can't connect to broker | Check broker IP, port, auth |
| `NVS write failed` | Flash storage error | Reflash firmware, check partition table |
| `Sensor reading invalid` | Bad sensor data | Check sensor, calibration, wiring |

## Next Steps

- [Installation Guide](INSTALLATION.md) - Reinstall from scratch
- [Configuration](CONFIGURATION.md) - Reset configuration
- [Calibration](CALIBRATION.md) - Recalibrate sensors
- [MQTT Guide](MQTT.md) - Fix MQTT issues
