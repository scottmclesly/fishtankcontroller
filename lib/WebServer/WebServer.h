#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Forward declaration
struct POETResult;
class WiFiManager;

class AquariumWebServer {
public:
    AquariumWebServer(WiFiManager* wifiMgr);

    // Initialize web server
    void begin();

    // Update sensor data (call this from main loop)
    void updateSensorData(const POETResult& result);

    // Get server instance
    AsyncWebServer* getServer() { return &server; }

private:
    AsyncWebServer server;
    WiFiManager* wifiManager;

    // Latest sensor readings
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

    // HTML page generators
    String generateHomePage();
    String generateProvisioningPage();
};

#endif // WEB_SERVER_H
