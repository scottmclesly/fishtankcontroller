#include "DisplayManager.h"

DisplayManager::DisplayManager()
    : display(nullptr),
      currentMetric(METRIC_TEMPERATURE),
      lastCycleTime(0),
      cycleIntervalMs(DEFAULT_CYCLE_INTERVAL),
      initialized(false) {

    // Initialize sensor data with defaults
    sensorData.temp_c = 0.0;
    sensorData.orp_mv = 0.0;
    sensorData.ph = 0.0;
    sensorData.ec_ms_cm = 0.0;
    sensorData.valid = false;
}

DisplayManager::~DisplayManager() {
    if (display) {
        delete display;
    }
}

bool DisplayManager::begin() {
    Serial.println("[Display] Initializing OLED display...");

    // Create display object
    display = new Adafruit_SSD1306(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);

    if (!display) {
        Serial.println("[Display] ERROR: Failed to allocate display object");
        return false;
    }

    // Initialize display - SSD1306_SWITCHCAPVCC generates display voltage internally
    if (!display->begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR)) {
        Serial.println("[Display] ERROR: SSD1306 initialization failed");
        Serial.println("[Display] Check I2C connections and address (0x3C or 0x3D)");
        delete display;
        display = nullptr;
        return false;
    }

    // Clear display buffer
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);

    // Show startup message
    display->setTextSize(1);
    display->setCursor(20, 4);
    display->println(F("Aquarium"));
    display->setCursor(20, 16);
    display->println(F("Controller"));
    display->display();

    initialized = true;
    lastCycleTime = millis();

    Serial.println("[Display] OLED initialized successfully (128x32)");
    Serial.printf("[Display] Cycle interval: %lu ms\n", cycleIntervalMs);

    return true;
}

void DisplayManager::loop() {
    if (!initialized || !display) {
        return;
    }

    // Check if it's time to cycle to next metric
    unsigned long now = millis();
    if (now - lastCycleTime >= cycleIntervalMs) {
        lastCycleTime = now;

        // Advance to next metric
        currentMetric = (DisplayMetric)((currentMetric + 1) % METRIC_COUNT);

        // Render the new metric
        renderCurrentMetric();
    }
}

void DisplayManager::updateSensorData(float temp_c, float orp_mv, float ph, float ec_ms_cm, bool valid) {
    sensorData.temp_c = temp_c;
    sensorData.orp_mv = orp_mv;
    sensorData.ph = ph;
    sensorData.ec_ms_cm = ec_ms_cm;
    sensorData.valid = valid;

    // Immediately refresh display with new data
    if (initialized) {
        renderCurrentMetric();
    }
}

void DisplayManager::setCycleInterval(unsigned long interval_ms) {
    cycleIntervalMs = interval_ms;
    Serial.printf("[Display] Cycle interval set to %lu ms\n", cycleIntervalMs);
}

unsigned long DisplayManager::getCycleInterval() const {
    return cycleIntervalMs;
}

void DisplayManager::showMetric(DisplayMetric metric) {
    if (metric < METRIC_COUNT) {
        currentMetric = metric;
        lastCycleTime = millis();
        if (initialized) {
            renderCurrentMetric();
        }
    }
}

bool DisplayManager::isInitialized() const {
    return initialized;
}

void DisplayManager::renderCurrentMetric() {
    if (!display) {
        return;
    }

    display->clearDisplay();

    if (!sensorData.valid) {
        renderNoData();
    } else {
        switch (currentMetric) {
            case METRIC_TEMPERATURE:
                renderTemperature();
                break;
            case METRIC_ORP:
                renderORP();
                break;
            case METRIC_PH:
                renderPH();
                break;
            case METRIC_EC:
                renderEC();
                break;
            default:
                renderNoData();
                break;
        }
    }

    display->display();
}

void DisplayManager::renderTemperature() {
    // Draw thermometer icon
    drawThermometerSymbol(4, 8);

    // Draw value - large text
    display->setTextSize(2);
    display->setCursor(28, 2);

    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%.1f", sensorData.temp_c);
    display->print(valueStr);

    // Draw unit
    display->setTextSize(1);
    display->setCursor(28, 22);
    display->print(F("deg C"));
}

void DisplayManager::renderORP() {
    // Draw lightning icon
    drawLightningSymbol(4, 8);

    // Draw value - large text
    display->setTextSize(2);
    display->setCursor(28, 2);

    char valueStr[16];
    // Show as integer for ORP since it's typically in hundreds of mV
    snprintf(valueStr, sizeof(valueStr), "%.0f", sensorData.orp_mv);
    display->print(valueStr);

    // Draw unit
    display->setTextSize(1);
    display->setCursor(28, 22);
    display->print(F("mV ORP"));
}

void DisplayManager::renderPH() {
    // Draw pH icon
    drawPHSymbol(4, 8);

    // Draw value - large text
    display->setTextSize(2);
    display->setCursor(28, 2);

    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%.2f", sensorData.ph);
    display->print(valueStr);

    // Draw unit
    display->setTextSize(1);
    display->setCursor(28, 22);
    display->print(F("pH"));
}

void DisplayManager::renderEC() {
    // Draw droplet icon
    drawDropletSymbol(4, 8);

    // Draw value - large text
    display->setTextSize(2);
    display->setCursor(28, 2);

    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%.2f", sensorData.ec_ms_cm);
    display->print(valueStr);

    // Draw unit
    display->setTextSize(1);
    display->setCursor(28, 22);
    display->print(F("mS/cm EC"));
}

void DisplayManager::renderNoData() {
    display->setTextSize(1);
    display->setCursor(24, 8);
    display->println(F("Waiting for"));
    display->setCursor(24, 18);
    display->println(F("sensor data..."));
}

// 16x16 pixel thermometer icon
void DisplayManager::drawThermometerSymbol(int x, int y) {
    // Bulb at bottom
    display->fillCircle(x + 7, y + 12, 4, SSD1306_WHITE);

    // Stem
    display->fillRect(x + 5, y, 5, 10, SSD1306_WHITE);

    // Inner bulb (for hollow effect)
    display->fillCircle(x + 7, y + 12, 2, SSD1306_BLACK);

    // Mercury level indicator
    display->fillRect(x + 6, y + 4, 3, 8, SSD1306_WHITE);
    display->fillCircle(x + 7, y + 12, 2, SSD1306_WHITE);

    // Top rounded cap
    display->fillCircle(x + 7, y + 1, 2, SSD1306_WHITE);
    display->fillRect(x + 5, y + 1, 5, 2, SSD1306_WHITE);
}

// 16x16 pixel lightning bolt icon (for ORP/voltage)
void DisplayManager::drawLightningSymbol(int x, int y) {
    // Lightning bolt shape using triangles and lines
    // Top part pointing right
    display->fillTriangle(
        x + 10, y,      // top right
        x + 4, y + 7,   // middle left
        x + 8, y + 7,   // middle right
        SSD1306_WHITE
    );

    // Bottom part pointing down-left
    display->fillTriangle(
        x + 8, y + 6,   // top
        x + 6, y + 6,   // top left
        x + 4, y + 15,  // bottom point
        SSD1306_WHITE
    );

    // Middle connection
    display->fillTriangle(
        x + 6, y + 6,
        x + 10, y + 6,
        x + 8, y + 10,
        SSD1306_WHITE
    );
}

// 16x16 pixel pH symbol
void DisplayManager::drawPHSymbol(int x, int y) {
    // Draw "pH" text as icon
    display->setTextSize(2);
    display->setCursor(x, y);
    display->print(F("pH"));
}

// 16x16 pixel water droplet icon (for EC/conductivity)
void DisplayManager::drawDropletSymbol(int x, int y) {
    // Droplet shape - pointed top, rounded bottom
    // Top point
    display->fillTriangle(
        x + 7, y,       // top point
        x + 3, y + 8,   // bottom left
        x + 11, y + 8,  // bottom right
        SSD1306_WHITE
    );

    // Bottom rounded part
    display->fillCircle(x + 7, y + 10, 5, SSD1306_WHITE);

    // Inner hollow (optional - makes it look like a drop outline)
    // Uncomment for hollow style:
    // display->fillCircle(x + 7, y + 10, 3, SSD1306_BLACK);
    // display->fillTriangle(x + 7, y + 3, x + 5, y + 8, x + 9, y + 8, SSD1306_BLACK);
}
