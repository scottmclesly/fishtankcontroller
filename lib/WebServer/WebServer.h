#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Forward declaration
struct POETResult;
class WiFiManager;
class CalibrationManager;

class AquariumWebServer {
public:
    AquariumWebServer(WiFiManager* wifiMgr, CalibrationManager* calMgr);

    // Initialize web server
    void begin();

    // Update sensor data (call this from main loop)
    void updateSensorData(const POETResult& result);

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

    // HTML page generators
    String generateHomePage();
    String generateProvisioningPage();
    String generateCalibrationPage();
};

#endif // WEB_SERVER_H
