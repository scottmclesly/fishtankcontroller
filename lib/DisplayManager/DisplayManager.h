#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display configuration for 0.91" 128x32 SSD1306 OLED
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32
#define DISPLAY_I2C_ADDR 0x3C  // Common address for SSD1306 (some use 0x3D)

// Metric indices
enum DisplayMetric {
    METRIC_TEMPERATURE = 0,
    METRIC_ORP = 1,
    METRIC_PH = 2,
    METRIC_EC = 3,
    METRIC_COUNT = 4
};

// Sensor data structure for display
struct DisplaySensorData {
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    bool valid;
};

class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager();

    // Initialize display
    bool begin();

    // Main loop function - handles metric cycling
    void loop();

    // Update sensor data for display
    void updateSensorData(float temp_c, float orp_mv, float ph, float ec_ms_cm, bool valid);

    // Configuration
    void setCycleInterval(unsigned long interval_ms);
    unsigned long getCycleInterval() const;

    // Force display of specific metric
    void showMetric(DisplayMetric metric);

    // Check if display is initialized
    bool isInitialized() const;

private:
    Adafruit_SSD1306* display;
    DisplaySensorData sensorData;

    DisplayMetric currentMetric;
    unsigned long lastCycleTime;
    unsigned long cycleIntervalMs;
    bool initialized;

    // Display rendering functions
    void renderCurrentMetric();
    void renderTemperature();
    void renderORP();
    void renderPH();
    void renderEC();
    void renderNoData();

    // Symbol drawing helpers (16x16 pixel icons)
    void drawThermometerSymbol(int x, int y);
    void drawLightningSymbol(int x, int y);
    void drawPHSymbol(int x, int y);
    void drawDropletSymbol(int x, int y);

    // Default cycle interval (3 seconds per metric)
    static const unsigned long DEFAULT_CYCLE_INTERVAL = 3000;
};

#endif // DISPLAY_MANAGER_H
