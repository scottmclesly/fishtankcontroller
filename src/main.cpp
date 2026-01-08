#include <Arduino.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include "WiFiManager.h"
#include "WebServer.h"
#include "CalibrationManager.h"
#include "MQTTManager.h"

// POET Sensor I2C Configuration
#define POET_I2C_ADDR 0x1F
#define I2C_FREQ 400000  // 400 kHz - max supported by POET

// Command byte bits
#define CMD_TEMPERATURE (1 << 0)  // bit 0
#define CMD_ORP         (1 << 1)  // bit 1
#define CMD_PH          (1 << 2)  // bit 2
#define CMD_EC          (1 << 3)  // bit 3
#define CMD_ALL         0x0F      // All measurements

// Measurement delays (ms) - from datasheet
#define DELAY_BASE        100
#define DELAY_TEMP        384
#define DELAY_ORP        1664
#define DELAY_PH          384
#define DELAY_EC          256
#define DELAY_ALL_TOTAL  2788  // 100 + 384 + 1664 + 384 + 256

// Structure to hold POET measurement results
struct POETResult {
  int32_t temp_mC;   // Temperature in milli-degrees Celsius
  int32_t orp_uV;    // ORP in micro-volts
  int32_t ugs_uV;    // pH gate-source potential in micro-volts
  int32_t ec_nA;     // EC sensor current in nano-Amps
  int32_t ec_uV;     // EC excitation in micro-volts
  bool valid;        // Indicates if reading was successful
};

// Global objects
WiFiManager wifiManager;
CalibrationManager calibrationManager;
MQTTManager mqttManager;
AquariumWebServer* webServer = nullptr;

// Function prototypes
bool poetInit();
bool poetMeasure(uint8_t command, POETResult &result);
int32_t readInt32LE();
void printPOETResult(const POETResult &result);
void processSerialCommands();
void printHelp();
void dumpDataCSV();
void dumpDataJSON();

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    delay(10);  // Wait for serial port to connect
  }

  Serial.println("\n\n=== Aquarium Controller Starting ===");
  Serial.println("Sentron POET pH/ORP/EC/Temperature I2C Sensor");
  Serial.println("I2C Address: 0x1F");
  Serial.println();
  Serial.println("Type 'help' for available console commands");
  Serial.println();

  // Initialize Calibration Manager
  if (!calibrationManager.begin()) {
    Serial.println("WARNING: Failed to initialize calibration manager");
  }
  Serial.println();

  // Initialize WiFi Manager
  bool wifiConnected = wifiManager.begin();

  // Initialize mDNS
  if (MDNS.begin("aquarium")) {
    Serial.println("mDNS responder started: http://aquarium.local");
  } else {
    Serial.println("Error setting up mDNS responder!");
  }

  // Initialize MQTT Manager
  if (!mqttManager.begin()) {
    Serial.println("WARNING: Failed to initialize MQTT manager");
  } else {
    Serial.println("MQTT Manager initialized");
  }
  Serial.println();

  // Initialize Web Server
  webServer = new AquariumWebServer(&wifiManager, &calibrationManager, &mqttManager);
  webServer->begin();

  if (wifiConnected) {
    Serial.println("\n=== System Ready ===");
    Serial.println("Access web interface at:");
    Serial.print("  http://");
    Serial.println(wifiManager.getIPAddress());
    Serial.println("  http://aquarium.local");
  } else {
    Serial.println("\n=== Provisioning Mode Active ===");
    Serial.println("Connect to WiFi AP and configure:");
    Serial.println("  SSID: " WIFI_AP_SSID);
    Serial.println("  Password: " WIFI_AP_PASSWORD);
    Serial.println("  URL: http://192.168.4.1");
  }

  Serial.println();

  // Initialize I2C
  if (!poetInit()) {
    Serial.println("ERROR: Failed to initialize POET sensor!");
    Serial.println("Please check:");
    Serial.println("  - I2C connections (SDA/SCL)");
    Serial.println("  - Sensor power (3.3V)");
    Serial.println("  - I2C address (0x1F)");
    Serial.println("\nWeb server will still run, but sensor data will be unavailable.");
  } else {
    Serial.println("POET sensor initialized successfully!");
  }

  Serial.println();
}

void loop() {
  // Handle serial commands (non-blocking)
  processSerialCommands();

  // Handle web server periodic tasks (history updates, NTP retries)
  if (webServer != nullptr) {
    webServer->loop();
  }

  // Handle MQTT connection and publishing
  mqttManager.loop();

  POETResult result;

  Serial.println("========================================");
  Serial.println("Starting new measurement cycle...");

  // Perform all measurements
  if (poetMeasure(CMD_ALL, result)) {
    // Update web server with new data
    if (webServer != nullptr) {
      webServer->updateSensorData(result);
    }

    printPOETResult(result);

    // Calculate and display engineering units
    Serial.println("\n--- Converted Values ---");

    // Temperature
    float temp_C = result.temp_mC / 1000.0;
    Serial.print("Temperature: ");
    Serial.print(temp_C, 2);
    Serial.println(" °C");

    // ORP
    float orp_mV = result.orp_uV / 1000.0;
    Serial.print("ORP:         ");
    Serial.print(orp_mV, 2);
    Serial.println(" mV");

    // pH (uses calibration if available)
    float ugs_mV = result.ugs_uV / 1000.0;
    float pH = calibrationManager.calculatePH(ugs_mV);
    Serial.print("pH:          ");
    Serial.print(pH, 2);
    if (!calibrationManager.hasValidPHCalibration()) {
      Serial.println(" (uncalibrated - needs buffer calibration!)");
    } else {
      Serial.println(" (calibrated)");
    }

    // EC (uses calibration if available)
    float ec_mS_cm = calibrationManager.calculateEC(result.ec_nA, result.ec_uV, temp_C);
    Serial.print("EC:          ");
    Serial.print(ec_mS_cm, 3);
    if (!calibrationManager.hasValidECCalibration()) {
      Serial.println(" mS/cm (uncalibrated - needs known solution!)");
    } else {
      Serial.println(" mS/cm (calibrated)");
    }

    // Publish to MQTT if connected
    SensorData sensorData;
    sensorData.temp_c = temp_C;
    sensorData.orp_mv = orp_mV;
    sensorData.ph = pH;
    sensorData.ec_ms_cm = ec_mS_cm;
    sensorData.valid = result.valid;

    if (mqttManager.publishSensorData(sensorData)) {
      Serial.println("\nMQTT: Sensor data published");
    } else if (mqttManager.isConnected()) {
      Serial.println("\nMQTT: Failed to publish (will retry)");
    }

    // Calculate resistance for EC measurement
    if (result.ec_nA != 0) {
      float resistance_ohm = (float)result.ec_uV / (float)result.ec_nA;
      Serial.print("EC Resistance: ");
      Serial.print(resistance_ohm, 1);
      Serial.println(" Ohm");
    }

    // Display WiFi status
    if (wifiManager.isConnected()) {
      Serial.print("\nWiFi: Connected (");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm)");
    } else if (wifiManager.isAPMode()) {
      Serial.print("\nWiFi: AP Mode - Clients: ");
      Serial.println(WiFi.softAPgetStationNum());
    }

  } else {
    Serial.println("ERROR: Failed to read sensor!");
    // Still update web server with invalid data
    if (webServer != nullptr) {
      webServer->updateSensorData(result);
    }
  }

  // Wait 5 seconds before next reading
  Serial.println("\nWaiting 5 seconds...\n");
  delay(5000);
}

/**
 * Initialize I2C and check for POET sensor presence
 */
bool poetInit() {
  // Initialize I2C with standard SDA/SCL pins
  Wire.begin();
  Wire.setClock(I2C_FREQ);

  // Check if device responds
  Wire.beginTransmission(POET_I2C_ADDR);
  uint8_t error = Wire.endTransmission();

  return (error == 0);  // 0 = success
}

/**
 * Perform measurement with POET sensor
 * @param command Bit mask of measurements to perform (CMD_TEMPERATURE, CMD_ORP, CMD_PH, CMD_EC)
 * @param result Reference to POETResult structure to store results
 * @return true if successful, false otherwise
 */
bool poetMeasure(uint8_t command, POETResult &result) {
  result.valid = false;

  // Send command byte to POET
  Wire.beginTransmission(POET_I2C_ADDR);
  Wire.write(command);
  uint8_t error = Wire.endTransmission();

  if (error != 0) {
    Serial.print("I2C transmission error: ");
    Serial.println(error);
    return false;
  }

  // Calculate required delay based on requested measurements
  uint16_t delay_ms = DELAY_BASE;
  if (command & CMD_TEMPERATURE) delay_ms += DELAY_TEMP;
  if (command & CMD_ORP)        delay_ms += DELAY_ORP;
  if (command & CMD_PH)         delay_ms += DELAY_PH;
  if (command & CMD_EC)         delay_ms += DELAY_EC;

  Serial.print("Waiting ");
  Serial.print(delay_ms);
  Serial.println(" ms for measurement...");

  delay(delay_ms);

  // Calculate expected number of bytes based on command
  uint8_t expected_bytes = 0;
  if (command & CMD_TEMPERATURE) expected_bytes += 4;
  if (command & CMD_ORP)        expected_bytes += 4;
  if (command & CMD_PH)         expected_bytes += 4;
  if (command & CMD_EC)         expected_bytes += 8;

  // Request data from POET
  uint8_t bytes_received = Wire.requestFrom((uint16_t)POET_I2C_ADDR, (uint8_t)expected_bytes);

  if (bytes_received != expected_bytes) {
    Serial.print("ERROR: Expected ");
    Serial.print(expected_bytes);
    Serial.print(" bytes, received ");
    Serial.println(bytes_received);
    return false;
  }

  // Read results in order: Temperature, ORP, pH, EC
  // All values are 32-bit signed integers in little-endian format

  if (command & CMD_TEMPERATURE) {
    result.temp_mC = readInt32LE();
  }

  if (command & CMD_ORP) {
    result.orp_uV = readInt32LE();
  }

  if (command & CMD_PH) {
    result.ugs_uV = readInt32LE();
  }

  if (command & CMD_EC) {
    result.ec_nA = readInt32LE();
    result.ec_uV = readInt32LE();
  }

  result.valid = true;
  return true;
}

/**
 * Read 32-bit signed integer in little-endian format from I2C
 */
int32_t readInt32LE() {
  uint8_t b0 = Wire.read();
  uint8_t b1 = Wire.read();
  uint8_t b2 = Wire.read();
  uint8_t b3 = Wire.read();

  // Combine bytes in little-endian order
  int32_t value = (int32_t)b0 |
                  ((int32_t)b1 << 8) |
                  ((int32_t)b2 << 16) |
                  ((int32_t)b3 << 24);

  return value;
}

/**
 * Print raw POET result values
 */
void printPOETResult(const POETResult &result) {
  Serial.println("\n--- Raw Sensor Values ---");
  Serial.print("temp_mC:  ");
  Serial.println(result.temp_mC);
  Serial.print("orp_uV:   ");
  Serial.println(result.orp_uV);
  Serial.print("ugs_uV:   ");
  Serial.println(result.ugs_uV);
  Serial.print("ec_nA:    ");
  Serial.println(result.ec_nA);
  Serial.print("ec_uV:    ");
  Serial.println(result.ec_uV);
}

/**
 * Process serial commands (non-blocking)
 */
void processSerialCommands() {
  static String commandBuffer = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (commandBuffer.length() > 0) {
        commandBuffer.trim();
        commandBuffer.toLowerCase();

        Serial.println("\n>>> Command: " + commandBuffer);

        if (commandBuffer == "help" || commandBuffer == "?") {
          printHelp();
        } else if (commandBuffer == "dump csv" || commandBuffer == "dump" || commandBuffer == "csv") {
          dumpDataCSV();
        } else if (commandBuffer == "dump json" || commandBuffer == "json") {
          dumpDataJSON();
        } else if (commandBuffer == "status") {
          Serial.println("\n=== System Status ===");
          Serial.print("WiFi: ");
          if (wifiManager.isConnected()) {
            Serial.print("Connected to ");
            Serial.print(wifiManager.getSSID());
            Serial.print(" (");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm)");
            Serial.print("IP: ");
            Serial.println(wifiManager.getIPAddress());
          } else if (wifiManager.isAPMode()) {
            Serial.print("AP Mode - Clients: ");
            Serial.println(WiFi.softAPgetStationNum());
          } else {
            Serial.println("Disconnected");
          }
          Serial.print("pH Calibration: ");
          Serial.println(calibrationManager.hasValidPHCalibration() ? "Calibrated" : "Not calibrated");
          Serial.print("EC Calibration: ");
          Serial.println(calibrationManager.hasValidECCalibration() ? "Calibrated" : "Not calibrated");
          Serial.print("Uptime: ");
          Serial.print(millis() / 1000);
          Serial.println(" seconds");
        } else if (commandBuffer == "clear") {
          // Clear the serial terminal
          Serial.println("\033[2J\033[H");
        } else {
          Serial.println("Unknown command. Type 'help' for available commands.");
        }

        commandBuffer = "";
      }
    } else {
      commandBuffer += c;
    }
  }
}

/**
 * Print help message with available commands
 */
void printHelp() {
  Serial.println("\n=== Available Console Commands ===");
  Serial.println("help, ?         - Show this help message");
  Serial.println("status          - Show system status");
  Serial.println("dump, csv       - Dump all captured data in CSV format");
  Serial.println("dump json       - Dump all captured data in JSON format");
  Serial.println("clear           - Clear terminal screen");
  Serial.println("\nData dump formats:");
  Serial.println("  CSV  - Best for Excel, spreadsheets, data analysis tools");
  Serial.println("  JSON - Best for programmatic processing, APIs");
  Serial.println("=====================================\n");
}

/**
 * Dump all captured data in CSV format
 */
void dumpDataCSV() {
  if (webServer == nullptr) {
    Serial.println("ERROR: Web server not initialized");
    return;
  }

  int historyCount = webServer->getHistoryCount();
  int historyHead = webServer->getHistoryHead();
  const DataPoint* history = webServer->getHistory();

  Serial.println("\n=== Data Dump (CSV Format) ===");
  Serial.println("# Aquarium Monitor Data Export");
  Serial.print("# Device: Kate's Aquarium #7 | Export time: ");

  time_t now = time(nullptr);
  if (now > 100000) {
    Serial.println(ctime(&now));
  } else {
    Serial.print(millis() / 1000);
    Serial.println(" seconds since boot (NTP not synced)");
  }

  Serial.print("# WiFi: ");
  Serial.println(wifiManager.getSSID());
  Serial.print("# pH Calibration: ");
  Serial.println(calibrationManager.hasValidPHCalibration() ? "Yes" : "No");
  Serial.print("# EC Calibration: ");
  Serial.println(calibrationManager.hasValidECCalibration() ? "Yes" : "No");
  Serial.print("# Data Points: ");
  Serial.println(historyCount);
  Serial.print("# Interval: 5 seconds");
  Serial.println("\n#");

  // CSV Header
  Serial.println("Timestamp,Unix_Time,Temperature_C,ORP_mV,pH,EC_mS_cm,Valid");

  // Output data in chronological order
  int startIdx = historyCount < HISTORY_SIZE ? 0 : historyHead;
  int validCount = 0;

  for (int i = 0; i < historyCount; i++) {
    int idx = (startIdx + i) % HISTORY_SIZE;

    if (history[idx].valid) {
      validCount++;

      // Format timestamp
      time_t ts = history[idx].timestamp;
      if (ts > 100000) {
        // Convert timestamp to readable format
        struct tm* timeinfo = localtime(&ts);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
        Serial.print(timeStr);
      } else {
        Serial.print("N/A");
      }
      Serial.print(",");

      // Unix timestamp
      Serial.print(history[idx].timestamp);
      Serial.print(",");

      // Temperature
      Serial.print(history[idx].temp_c, 2);
      Serial.print(",");

      // ORP
      Serial.print(history[idx].orp_mv, 2);
      Serial.print(",");

      // pH
      Serial.print(history[idx].ph, 2);
      Serial.print(",");

      // EC
      Serial.print(history[idx].ec_ms_cm, 3);
      Serial.print(",");

      // Valid flag
      Serial.println("true");
    }
  }

  Serial.println("#");
  Serial.print("# Total data points exported: ");
  Serial.println(validCount);
  Serial.println("=== End of CSV Data ===\n");
}

/**
 * Dump all captured data in JSON format
 */
void dumpDataJSON() {
  if (webServer == nullptr) {
    Serial.println("ERROR: Web server not initialized");
    return;
  }

  int historyCount = webServer->getHistoryCount();
  int historyHead = webServer->getHistoryHead();
  const DataPoint* history = webServer->getHistory();

  Serial.println("\n=== Data Dump (JSON Format) ===");

  time_t now = time(nullptr);

  Serial.println("{");
  Serial.println("  \"device\": {");
  Serial.println("    \"name\": \"Kate's Aquarium #7\",");
  Serial.print("    \"export_timestamp\": ");
  if (now > 100000) {
    Serial.print(now);
    Serial.println(",");
  } else {
    Serial.println("null,");
  }
  Serial.print("    \"uptime_seconds\": ");
  Serial.print(millis() / 1000);
  Serial.println(",");
  Serial.print("    \"wifi_ssid\": \"");
  Serial.print(wifiManager.getSSID());
  Serial.println("\",");
  Serial.print("    \"wifi_ip\": \"");
  Serial.print(wifiManager.getIPAddress());
  Serial.println("\",");
  Serial.print("    \"ph_calibrated\": ");
  Serial.print(calibrationManager.hasValidPHCalibration() ? "true" : "false");
  Serial.println(",");
  Serial.print("    \"ec_calibrated\": ");
  Serial.print(calibrationManager.hasValidECCalibration() ? "true" : "false");
  Serial.println(",");
  Serial.print("    \"data_points\": ");
  Serial.print(historyCount);
  Serial.println(",");
  Serial.println("    \"interval_seconds\": 5");
  Serial.println("  },");
  Serial.println("  \"data\": [");

  // Output data in chronological order
  int startIdx = historyCount < HISTORY_SIZE ? 0 : historyHead;
  int validCount = 0;

  for (int i = 0; i < historyCount; i++) {
    int idx = (startIdx + i) % HISTORY_SIZE;

    if (history[idx].valid) {
      if (validCount > 0) {
        Serial.println(",");
      }
      validCount++;

      Serial.println("    {");
      Serial.print("      \"timestamp\": ");
      Serial.print(history[idx].timestamp);
      Serial.println(",");
      Serial.print("      \"temp_c\": ");
      Serial.print(history[idx].temp_c, 2);
      Serial.println(",");
      Serial.print("      \"orp_mv\": ");
      Serial.print(history[idx].orp_mv, 2);
      Serial.println(",");
      Serial.print("      \"ph\": ");
      Serial.print(history[idx].ph, 2);
      Serial.println(",");
      Serial.print("      \"ec_ms_cm\": ");
      Serial.print(history[idx].ec_ms_cm, 3);
      Serial.println(",");
      Serial.println("      \"valid\": true");
      Serial.print("    }");
    }
  }

  if (validCount > 0) {
    Serial.println();
  }

  Serial.println("  ],");
  Serial.print("  \"summary\": {");
  Serial.print("\"total_points\": ");
  Serial.print(validCount);
  Serial.println("}");
  Serial.println("}");

  Serial.println("=== End of JSON Data ===\n");
}