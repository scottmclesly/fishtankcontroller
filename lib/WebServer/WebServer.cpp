#include "WebServer.h"
#include "WiFiManager.h"
#include <WiFi.h>

// Include POETResult struct definition from main
struct POETResult {
    int32_t temp_mC;
    int32_t orp_uV;
    int32_t ugs_uV;
    int32_t ec_nA;
    int32_t ec_uV;
    bool valid;
};

AquariumWebServer::AquariumWebServer(WiFiManager* wifiMgr)
    : server(80), wifiManager(wifiMgr), temp_c(0), orp_mv(0),
      ph(0), ec_ms_cm(0), lastUpdate(0), dataValid(false) {
}

void AquariumWebServer::begin() {
    setupRoutes();
    server.begin();
    Serial.println("Web server started on port 80");
}

void AquariumWebServer::setupRoutes() {
    // Root page - sensor dashboard or provisioning
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleRoot(request);
    });

    // API endpoint for sensor data (JSON)
    server.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleSensorData(request);
    });

    // Provisioning page
    server.on("/setup", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleProvisioningPage(request);
    });

    // Save WiFi credentials
    server.on("/save-wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleSaveWiFi(request);
    });

    // Scan for networks
    server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleScanNetworks(request);
    });

    // 404 handler
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not Found");
    });
}

void AquariumWebServer::handleRoot(AsyncWebServerRequest *request) {
    if (wifiManager->isAPMode()) {
        request->send(200, "text/html", generateProvisioningPage());
    } else {
        request->send(200, "text/html", generateHomePage());
    }
}

void AquariumWebServer::handleSensorData(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["timestamp"] = millis();
    doc["valid"] = dataValid;

    if (dataValid) {
        doc["temperature_c"] = temp_c;
        doc["orp_mv"] = orp_mv;
        doc["ph"] = ph;
        doc["ec_ms_cm"] = ec_ms_cm;
    }

    doc["wifi"]["ssid"] = wifiManager->getSSID();
    doc["wifi"]["ip"] = wifiManager->getIPAddress();
    doc["wifi"]["rssi"] = WiFi.RSSI();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleProvisioningPage(AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateProvisioningPage());
}

void AquariumWebServer::handleSaveWiFi(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
        String ssid = request->getParam("ssid", true)->value();
        String password = request->getParam("password", true)->value();

        wifiManager->saveCredentials(ssid, password);

        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>WiFi Saved</title></head><body style='font-family: Arial; text-align: center; padding: 50px;'>";
        html += "<h1>WiFi Credentials Saved!</h1>";
        html += "<p>The device will restart and attempt to connect to: <strong>" + ssid + "</strong></p>";
        html += "<p>If connection is successful, access the device at: <strong>http://aquarium.local</strong></p>";
        html += "<p>Restarting in 3 seconds...</p>";
        html += "</body></html>";

        request->send(200, "text/html", html);

        // Restart after 3 seconds to apply new credentials
        delay(3000);
        ESP.restart();
    } else {
        request->send(400, "text/plain", "Missing SSID or password");
    }
}

void AquariumWebServer::handleScanNetworks(AsyncWebServerRequest *request) {
    int n = WiFi.scanNetworks();

    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n; i++) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::updateSensorData(const POETResult& result) {
    if (!result.valid) {
        dataValid = false;
        return;
    }

    temp_c = result.temp_mC / 1000.0;
    orp_mv = result.orp_uV / 1000.0;

    // pH calculation (uncalibrated)
    float ugs_mV = result.ugs_uV / 1000.0;
    ph = 7.0 + (ugs_mV - 0.0) / 52.0;

    // EC calculation (uncalibrated)
    if (result.ec_nA != 0) {
        float resistance_ohm = (float)result.ec_uV / (float)result.ec_nA;
        ec_ms_cm = (1.0 / resistance_ohm) * 1000.0;
    } else {
        ec_ms_cm = 0.0;
    }

    lastUpdate = millis();
    dataValid = true;
}

String AquariumWebServer::generateHomePage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Aquarium Monitor</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; background: #f0f8ff; }";
    html += "h1 { color: #006494; text-align: center; }";
    html += ".sensor-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }";
    html += ".sensor-card { background: white; border-radius: 10px; padding: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
    html += ".sensor-value { font-size: 2em; font-weight: bold; color: #006494; margin: 10px 0; }";
    html += ".sensor-label { color: #666; font-size: 0.9em; }";
    html += ".sensor-unit { color: #999; font-size: 0.8em; }";
    html += ".status { text-align: center; padding: 10px; background: #e8f5e9; border-radius: 5px; margin: 20px 0; }";
    html += ".warning { background: #fff3cd; color: #856404; padding: 10px; border-radius: 5px; margin: 10px 0; }";
    html += ".info { background: #d1ecf1; color: #0c5460; padding: 10px; border-radius: 5px; margin: 10px 0; font-size: 0.9em; }";
    html += "</style>";
    html += "<script>";
    html += "function updateData() {";
    html += "  fetch('/api/sensors')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      if (data.valid) {";
    html += "        document.getElementById('temp').textContent = data.temperature_c.toFixed(2);";
    html += "        document.getElementById('orp').textContent = data.orp_mv.toFixed(2);";
    html += "        document.getElementById('ph').textContent = data.ph.toFixed(2);";
    html += "        document.getElementById('ec').textContent = data.ec_ms_cm.toFixed(3);";
    html += "      }";
    html += "      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();";
    html += "    });";
    html += "}";
    html += "setInterval(updateData, 2000);";
    html += "updateData();";
    html += "</script>";
    html += "</head><body>";
    html += "<h1>🐠 Aquarium Monitor</h1>";

    html += "<div class='status'>✓ Connected to WiFi: <strong>" + wifiManager->getSSID() + "</strong> | ";
    html += "IP: <strong>" + wifiManager->getIPAddress() + "</strong></div>";

    html += "<div class='warning'>⚠️ Sensors require calibration for accurate readings</div>";

    html += "<div class='sensor-grid'>";

    // Temperature
    html += "<div class='sensor-card'>";
    html += "<div class='sensor-label'>Temperature</div>";
    html += "<div class='sensor-value'><span id='temp'>";
    html += dataValid ? String(temp_c, 2) : "--";
    html += "</span> <span class='sensor-unit'>°C</span></div>";
    html += "</div>";

    // ORP
    html += "<div class='sensor-card'>";
    html += "<div class='sensor-label'>ORP</div>";
    html += "<div class='sensor-value'><span id='orp'>";
    html += dataValid ? String(orp_mv, 2) : "--";
    html += "</span> <span class='sensor-unit'>mV</span></div>";
    html += "</div>";

    // pH
    html += "<div class='sensor-card'>";
    html += "<div class='sensor-label'>pH</div>";
    html += "<div class='sensor-value'><span id='ph'>";
    html += dataValid ? String(ph, 2) : "--";
    html += "</span></div>";
    html += "<div class='warning' style='font-size: 0.7em; margin-top: 5px;'>Uncalibrated</div>";
    html += "</div>";

    // EC
    html += "<div class='sensor-card'>";
    html += "<div class='sensor-label'>Conductivity</div>";
    html += "<div class='sensor-value'><span id='ec'>";
    html += dataValid ? String(ec_ms_cm, 3) : "--";
    html += "</span> <span class='sensor-unit'>mS/cm</span></div>";
    html += "<div class='warning' style='font-size: 0.7em; margin-top: 5px;'>Uncalibrated</div>";
    html += "</div>";

    html += "</div>";

    html += "<div class='info'>Last update: <span id='lastUpdate'>--</span></div>";
    html += "<div class='info'>Auto-refresh every 2 seconds</div>";

    html += "</body></html>";

    return html;
}

String AquariumWebServer::generateProvisioningPage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Aquarium Setup</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; max-width: 500px; margin: 0 auto; padding: 20px; background: #f0f8ff; }";
    html += "h1 { color: #006494; text-align: center; }";
    html += ".card { background: white; border-radius: 10px; padding: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); margin: 20px 0; }";
    html += "input, select, button { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }";
    html += "button { background: #006494; color: white; border: none; cursor: pointer; font-size: 1em; }";
    html += "button:hover { background: #004d73; }";
    html += ".network-item { padding: 10px; margin: 5px 0; background: #f8f9fa; border-radius: 5px; cursor: pointer; }";
    html += ".network-item:hover { background: #e9ecef; }";
    html += ".info { color: #666; font-size: 0.9em; text-align: center; margin: 10px 0; }";
    html += "</style>";
    html += "<script>";
    html += "function scanNetworks() {";
    html += "  document.getElementById('networks').innerHTML = '<p>Scanning...</p>';";
    html += "  fetch('/scan')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      let html = '';";
    html += "      data.networks.forEach(network => {";
    html += "        html += `<div class='network-item' onclick='selectNetwork(\"${network.ssid}\")'>${network.ssid} (${network.rssi} dBm) ${network.encryption}</div>`;";
    html += "      });";
    html += "      document.getElementById('networks').innerHTML = html || '<p>No networks found</p>';";
    html += "    });";
    html += "}";
    html += "function selectNetwork(ssid) {";
    html += "  document.getElementById('ssid').value = ssid;";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    html += "<h1>🐠 Aquarium Setup</h1>";

    html += "<div class='card'>";
    html += "<h2>WiFi Configuration</h2>";
    html += "<p class='info'>Connect your aquarium controller to your WiFi network</p>";

    html += "<form action='/save-wifi' method='POST'>";
    html += "<input type='text' id='ssid' name='ssid' placeholder='WiFi Network Name (SSID)' required>";
    html += "<input type='password' name='password' placeholder='WiFi Password' required>";
    html += "<button type='submit'>Connect to WiFi</button>";
    html += "</form>";

    html += "<button onclick='scanNetworks()' style='background: #28a745; margin-top: 10px;'>Scan for Networks</button>";
    html += "<div id='networks' style='margin-top: 15px;'></div>";
    html += "</div>";

    html += "<div class='info'>After connecting to WiFi, access at http://aquarium.local</div>";
    html += "<div class='info'>Current AP: " + String(WIFI_AP_SSID) + " | IP: 192.168.4.1</div>";

    html += "</body></html>";

    return html;
}
