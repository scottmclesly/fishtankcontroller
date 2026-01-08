#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <time.h>

// Forward declaration
struct POETResult;
class WiFiManager;
class CalibrationManager;

// Data history configuration
#define HISTORY_SIZE 288  // 288 points = 24 hours at 5-minute intervals (or 24 min at 5s intervals)
#define HISTORY_INTERVAL_MS 5000  // 5 seconds between data points

struct DataPoint {
    time_t timestamp;
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    bool valid;
};

class AquariumWebServer {
public:
    AquariumWebServer(WiFiManager* wifiMgr, CalibrationManager* calMgr);

    // Initialize web server
    void begin();

    // Update sensor data (call this from main loop)
    void updateSensorData(const POETResult& result);

    // Handle loop tasks (e.g., periodic history updates)
    void loop();

    // Initialize NTP time synchronization
    void initNTP();

    // Get server instance
    AsyncWebServer* getServer() { return &server; }

private:
    AsyncWebServer server;
    WiFiManager* wifiManager;
    CalibrationManager* calibrationManager;

    // Latest sensor readings (raw)
    int32_t raw_temp_mC;
    int32_t raw_orp_uV;
    int32_t raw_ugs_uV;
    int32_t raw_ec_nA;
    int32_t raw_ec_uV;

    // Converted values
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    unsigned long lastUpdate;
    bool dataValid;

    // Data history (circular buffer)
    DataPoint history[HISTORY_SIZE];
    int historyHead;
    int historyCount;
    unsigned long lastHistoryUpdate;

    // NTP synchronization
    bool ntpInitialized;
    const char* ntpServer1 = "pool.ntp.org";
    const char* ntpServer2 = "time.nist.gov";
    const long gmtOffset_sec = 0;
    const int daylightOffset_sec = 0;

    // Setup routes
    void setupRoutes();

    // Route handlers
    void handleRoot(AsyncWebServerRequest *request);
    void handleSensorData(AsyncWebServerRequest *request);
    void handleProvisioningPage(AsyncWebServerRequest *request);
    void handleSaveWiFi(AsyncWebServerRequest *request);
    void handleScanNetworks(AsyncWebServerRequest *request);
    void handleCalibrationPage(AsyncWebServerRequest *request);
    void handleGetCalibrationStatus(AsyncWebServerRequest *request);
    void handleCalibratePhOnePoint(AsyncWebServerRequest *request);
    void handleCalibratePhTwoPoint(AsyncWebServerRequest *request);
    void handleCalibrateEC(AsyncWebServerRequest *request);
    void handleClearPhCalibration(AsyncWebServerRequest *request);
    void handleClearEcCalibration(AsyncWebServerRequest *request);
    void handleGetRawReadings(AsyncWebServerRequest *request);
    void handleGetHistory(AsyncWebServerRequest *request);
    void handleChartsPage(AsyncWebServerRequest *request);

    // HTML page generators
    String generateHomePage();
    String generateProvisioningPage();
    String generateCalibrationPage();
    String generateChartsPage();

    // History management
    void addDataPointToHistory();
};

#endif // WEB_SERVER_H
