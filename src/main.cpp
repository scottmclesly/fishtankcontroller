#include <Arduino.h>
#include <Wire.h>

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

// Function prototypes
bool poetInit();
bool poetMeasure(uint8_t command, POETResult &result);
int32_t readInt32LE();
void printPOETResult(const POETResult &result);
float calculatePH(int32_t ugs_uV, float buffer_pH = 7.0, float buffer_ugs_mV = 0.0);
float calculateEC(int32_t ec_nA, int32_t ec_uV, float cell_constant = 1.0);

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    delay(10);  // Wait for serial port to connect
  }

  Serial.println("\n\n=== POET Sensor Test ===");
  Serial.println("Sentron POET pH/ORP/EC/Temperature I2C Sensor");
  Serial.println("I2C Address: 0x1F");
  Serial.println();

  // Initialize I2C
  if (!poetInit()) {
    Serial.println("ERROR: Failed to initialize POET sensor!");
    Serial.println("Please check:");
    Serial.println("  - I2C connections (SDA/SCL)");
    Serial.println("  - Sensor power (3.3V)");
    Serial.println("  - I2C address (0x1F)");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("POET sensor initialized successfully!");
  Serial.println();
}

void loop() {
  POETResult result;

  Serial.println("========================================");
  Serial.println("Starting new measurement cycle...");

  // Perform all measurements
  if (poetMeasure(CMD_ALL, result)) {
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

    // pH (uncalibrated - assumes 0mV offset at pH 7.0)
    float pH = calculatePH(result.ugs_uV);
    Serial.print("pH:          ");
    Serial.print(pH, 2);
    Serial.println(" (uncalibrated - needs buffer calibration!)");

    // EC (uncalibrated - assumes cell constant = 1.0 /cm)
    float ec_mS_cm = calculateEC(result.ec_nA, result.ec_uV, 1.0);
    Serial.print("EC:          ");
    Serial.print(ec_mS_cm, 3);
    Serial.println(" mS/cm (uncalibrated - needs known solution!)");

    // Calculate resistance for EC measurement
    if (result.ec_nA != 0) {
      float resistance_ohm = (float)result.ec_uV / (float)result.ec_nA;
      Serial.print("EC Resistance: ");
      Serial.print(resistance_ohm, 1);
      Serial.println(" Ohm");
    }

  } else {
    Serial.println("ERROR: Failed to read sensor!");
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
 * Calculate pH from gate-source voltage
 * @param ugs_uV Gate-source voltage in microvolts
 * @param buffer_pH Known pH of buffer solution used for calibration (default 7.0)
 * @param buffer_ugs_mV Gate-source voltage in millivolts when measured in buffer
 * @return Calculated pH value
 *
 * Note: Requires calibration in known buffer solution for accurate results
 * Formula: pH = buffer_pH + (sample_ugs_mV - buffer_ugs_mV) / 52
 * Sensitivity: ~52 mV/pH
 */
float calculatePH(int32_t ugs_uV, float buffer_pH, float buffer_ugs_mV) {
  float ugs_mV = ugs_uV / 1000.0;
  float pH = buffer_pH + (ugs_mV - buffer_ugs_mV) / 52.0;
  return pH;
}

/**
 * Calculate EC (Electrical Conductivity) from sensor measurements
 * @param ec_nA Sensor current in nano-amps
 * @param ec_uV Excitation voltage in micro-volts
 * @param cell_constant Cell constant in /cm (requires calibration)
 * @return Conductivity in mS/cm
 *
 * Note: Requires calibration in known conductivity solution
 * Example: 0.01M KCl solution = 1.41 mS/cm @ 25°C
 */
float calculateEC(int32_t ec_nA, int32_t ec_uV, float cell_constant) {
  if (ec_nA == 0) return 0.0;

  // Calculate resistance: R = V / I
  float resistance_ohm = (float)ec_uV / (float)ec_nA;

  // Calculate conductivity: EC = cell_constant / resistance
  float ec_mS_cm = (cell_constant / resistance_ohm) * 1000.0;

  return ec_mS_cm;
}