#include "WebServer.h"
#include "WiFiManager.h"
#include "CalibrationManager.h"
#include "MQTTManager.h"
#include "charts_page.h"
#include <WiFi.h>
#include <Preferences.h>

// Include POETResult struct definition from main
struct POETResult {
    int32_t temp_mC;
    int32_t orp_uV;
    int32_t ugs_uV;
    int32_t ec_nA;
    int32_t ec_uV;
    bool valid;
};

AquariumWebServer::AquariumWebServer(WiFiManager* wifiMgr, CalibrationManager* calMgr, MQTTManager* mqttMgr)
    : server(80), wifiManager(wifiMgr), calibrationManager(calMgr), mqttManager(mqttMgr),
      raw_temp_mC(0), raw_orp_uV(0), raw_ugs_uV(0), raw_ec_nA(0), raw_ec_uV(0),
      temp_c(0), orp_mv(0), ph(0), ec_ms_cm(0), lastUpdate(0), dataValid(false),
      historyHead(0), historyCount(0), lastHistoryUpdate(0), ntpInitialized(false) {
    // Initialize history buffer
    for (int i = 0; i < HISTORY_SIZE; i++) {
        history[i].valid = false;
    }
}

void AquariumWebServer::begin() {
    setupRoutes();
    server.begin();
    Serial.println("Web server started on port 80");
    initNTP();
}

void AquariumWebServer::initNTP() {
    Serial.println("Initializing NTP...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

    // Wait up to 5 seconds for time sync
    int retries = 10;
    while (retries > 0 && time(nullptr) < 100000) {
        delay(500);
        Serial.print(".");
        retries--;
    }
    Serial.println();

    time_t now = time(nullptr);
    if (now > 100000) {
        ntpInitialized = true;
        Serial.println("NTP synchronized: " + String(ctime(&now)));
    } else {
        Serial.println("NTP sync failed - will retry later");
    }
}

void AquariumWebServer::loop() {
    // Check if it's time to add a data point to history
    if (dataValid && (millis() - lastHistoryUpdate >= HISTORY_INTERVAL_MS)) {
        addDataPointToHistory();
        lastHistoryUpdate = millis();
    }

    // Retry NTP if not initialized and connected to WiFi
    if (!ntpInitialized && !wifiManager->isAPMode()) {
        static unsigned long lastNtpRetry = 0;
        if (millis() - lastNtpRetry > 60000) { // Retry every minute
            initNTP();
            lastNtpRetry = millis();
        }
    }
}

void AquariumWebServer::addDataPointToHistory() {
    DataPoint dp;
    dp.timestamp = time(nullptr);
    dp.temp_c = temp_c;
    dp.orp_mv = orp_mv;
    dp.ph = ph;
    dp.ec_ms_cm = ec_ms_cm;
    dp.valid = dataValid;

    history[historyHead] = dp;
    historyHead = (historyHead + 1) % HISTORY_SIZE;
    if (historyCount < HISTORY_SIZE) {
        historyCount++;
    }
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

    // Calibration page
    server.on("/calibration", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleCalibrationPage(request);
    });

    // Charts page
    server.on("/charts", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleChartsPage(request);
    });

    // History data API
    server.on("/api/history", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetHistory(request);
    });

    // Data export endpoints
    server.on("/api/export/csv", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleExportCSV(request);
    });

    server.on("/api/export/json", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleExportJSON(request);
    });

    // Calibration API endpoints
    server.on("/api/calibration/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetCalibrationStatus(request);
    });

    server.on("/api/calibration/raw", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetRawReadings(request);
    });

    server.on("/api/calibration/ph/1point", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleCalibratePhOnePoint(request);
    });

    server.on("/api/calibration/ph/2point", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleCalibratePhTwoPoint(request);
    });

    server.on("/api/calibration/ec", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleCalibrateEC(request);
    });

    server.on("/api/calibration/ph/clear", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleClearPhCalibration(request);
    });

    server.on("/api/calibration/ec/clear", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleClearEcCalibration(request);
    });

    // MQTT API endpoints
    server.on("/api/mqtt/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetMQTTConfig(request);
    });

    server.on("/api/mqtt/config", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleSaveMQTTConfig(request);
    });

    server.on("/api/mqtt/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetMQTTStatus(request);
    });

    // Unit name API endpoints
    server.on("/api/unit/name", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetUnitName(request);
    });

    server.on("/api/unit/name", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleSaveUnitName(request);
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

    // Store raw values
    raw_temp_mC = result.temp_mC;
    raw_orp_uV = result.orp_uV;
    raw_ugs_uV = result.ugs_uV;
    raw_ec_nA = result.ec_nA;
    raw_ec_uV = result.ec_uV;

    // Convert to engineering units
    temp_c = result.temp_mC / 1000.0;
    orp_mv = result.orp_uV / 1000.0;

    // pH calculation (uses calibration if available)
    float ugs_mV = result.ugs_uV / 1000.0;
    ph = calibrationManager->calculatePH(ugs_mV);

    // EC calculation (uses calibration if available)
    ec_ms_cm = calibrationManager->calculateEC(result.ec_nA, result.ec_uV, temp_c);

    lastUpdate = millis();
    dataValid = true;
}

String AquariumWebServer::generateHomePage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<link rel='icon' href='data:image/svg+xml,<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\"><text y=\".9em\" font-size=\"90\">🐠</text></svg>'>";
    html += "<title>Aquarium Monitor</title>";
    html += "<style>";
    html += ":root {";
    html += "  --bg-primary: #0a0e1a;";
    html += "  --bg-card: #1a1f2e;";
    html += "  --bg-status: #1e293b;";
    html += "  --text-primary: #e0e7ff;";
    html += "  --text-secondary: #94a3b8;";
    html += "  --text-tertiary: #64748b;";
    html += "  --color-primary: #00d4ff;";
    html += "  --color-secondary: #7c3aed;";
    html += "  --color-primary-hover: #00b8e6;";
    html += "  --border-color: #1e293b;";
    html += "  --shadow: rgba(0, 212, 255, 0.1);";
    html += "  --glow: rgba(0, 212, 255, 0.3);";
    html += "  --temp-color: #ef4444;";
    html += "  --orp-color: #f59e0b;";
    html += "  --ph-color: #10b981;";
    html += "  --ec-color: #3b82f6;";
    html += "  --warning-bg: #7f6003;";
    html += "  --warning-text: #fff3cd;";
    html += "  --success-bg: #2e7d32;";
    html += "  --success-text: #c8e6c9;";
    html += "}";
    html += "[data-theme='light'] {";
    html += "  --bg-primary: #f8fafc;";
    html += "  --bg-card: #ffffff;";
    html += "  --bg-status: #f1f5f9;";
    html += "  --text-primary: #0f172a;";
    html += "  --text-secondary: #475569;";
    html += "  --text-tertiary: #94a3b8;";
    html += "  --color-primary: #0ea5e9;";
    html += "  --color-secondary: #8b5cf6;";
    html += "  --color-primary-hover: #0284c7;";
    html += "  --border-color: #e2e8f0;";
    html += "  --shadow: rgba(14, 165, 233, 0.1);";
    html += "  --glow: rgba(14, 165, 233, 0.2);";
    html += "  --warning-bg: #fff3cd;";
    html += "  --warning-text: #856404;";
    html += "  --success-bg: #d4edda;";
    html += "  --success-text: #155724;";
    html += "}";
    html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
    html += "body { font-family: 'Segoe UI', system-ui, -apple-system, sans-serif; background: var(--bg-primary); color: var(--text-primary); padding: 20px; min-height: 100vh; transition: all 0.3s ease; }";
    html += ".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; padding: 20px; background: var(--bg-card); border-radius: 15px; border: 1px solid var(--border-color); box-shadow: 0 4px 20px var(--shadow); }";
    html += "h1 { font-size: 2em; background: linear-gradient(135deg, var(--color-primary), var(--color-secondary)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; background-clip: text; font-weight: 700; letter-spacing: -0.5px; margin: 0; }";
    html += ".nav { display: flex; gap: 15px; align-items: center; }";
    html += ".nav a, .theme-toggle { padding: 10px 20px; background: var(--bg-primary); color: var(--text-primary); text-decoration: none; border-radius: 8px; border: 1px solid var(--border-color); transition: all 0.3s ease; font-size: 0.9em; font-weight: 500; cursor: pointer; }";
    html += ".nav a:hover, .theme-toggle:hover { background: var(--color-primary); color: var(--bg-primary); box-shadow: 0 0 20px var(--glow); transform: translateY(-2px); }";
    html += ".status-bar { display: flex; justify-content: center; gap: 20px; padding: 15px; background: var(--bg-card); border-radius: 10px; margin-bottom: 20px; border: 1px solid var(--border-color); flex-wrap: wrap; }";
    html += ".status-item { display: flex; align-items: center; gap: 8px; font-size: 0.85em; color: var(--text-secondary); }";
    html += ".status-dot { width: 10px; height: 10px; border-radius: 50%; background: #10b981; animation: pulse 2s ease-in-out infinite; }";
    html += "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }";
    html += ".sensor-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin: 20px 0; }";
    html += ".sensor-card { background: var(--bg-card); padding: 20px; border-radius: 12px; border: 1px solid var(--border-color); box-shadow: 0 2px 10px var(--shadow); position: relative; overflow: hidden; }";
    html += ".sensor-card::before { content: ''; position: absolute; top: 0; left: 0; width: 4px; height: 100%; background: linear-gradient(180deg, var(--card-color), transparent); }";
    html += ".sensor-label { font-size: 0.85em; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 1px; font-weight: 600; margin-bottom: 10px; }";
    html += ".sensor-value { font-size: 2.5em; font-weight: 700; color: var(--card-color); margin: 10px 0 5px 0; font-variant-numeric: tabular-nums; }";
    html += ".sensor-unit { font-size: 0.9em; color: var(--text-tertiary); font-weight: 500; }";
    html += ".sensor-status { font-size: 0.75em; margin-top: 8px; padding: 4px 8px; border-radius: 4px; display: inline-block; }";
    html += ".calibrated { background: var(--success-bg); color: var(--success-text); }";
    html += ".uncalibrated { background: var(--warning-bg); color: var(--warning-text); }";
    html += ".warning-banner { background: var(--warning-bg); color: var(--warning-text); padding: 15px; border-radius: 10px; margin: 20px 0; border: 1px solid var(--border-color); }";
    html += ".warning-banner a { color: var(--warning-text); text-decoration: underline; font-weight: bold; }";
    html += ".info-footer { text-align: center; padding: 15px; background: var(--bg-card); border-radius: 10px; margin-top: 20px; border: 1px solid var(--border-color); font-size: 0.85em; color: var(--text-secondary); }";
    html += "</style>";
    html += "<script>";
    html += "function initTheme() {";
    html += "  const savedTheme = localStorage.getItem('theme') || 'dark';";
    html += "  document.documentElement.setAttribute('data-theme', savedTheme);";
    html += "  updateThemeIcon(savedTheme);";
    html += "}";
    html += "function toggleTheme() {";
    html += "  const current = document.documentElement.getAttribute('data-theme') || 'dark';";
    html += "  const newTheme = current === 'light' ? 'dark' : 'light';";
    html += "  document.documentElement.setAttribute('data-theme', newTheme);";
    html += "  localStorage.setItem('theme', newTheme);";
    html += "  updateThemeIcon(newTheme);";
    html += "}";
    html += "function updateThemeIcon(theme) {";
    html += "  const btn = document.getElementById('themeToggle');";
    html += "  btn.textContent = theme === 'light' ? '🌙' : '☀️';";
    html += "}";
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
    html += "    })";
    html += "    .catch(err => console.error('Update failed:', err))";
    html += "    .finally(() => {";
    html += "      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();";
    html += "    });";
    html += "}";
    html += "function updateMqttStatus() {";
    html += "  fetch('/api/mqtt/status')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      const statusEl = document.getElementById('mqttStatus');";
    html += "      if (data.connected) {";
    html += "        statusEl.textContent = '✓ Connected';";
    html += "        statusEl.style.color = 'var(--success-text)';";
    html += "      } else if (data.enabled) {";
    html += "        statusEl.textContent = '⚠ ' + data.status;";
    html += "        statusEl.style.color = 'var(--warning-text)';";
    html += "      } else {";
    html += "        statusEl.textContent = 'Disabled';";
    html += "        statusEl.style.color = 'var(--text-tertiary)';";
    html += "      }";
    html += "    });";
    html += "}";
    html += "</script>";
    html += "</head><body>";

    html += "<div class='header'>";
    html += "<h1>🐠 " + getUnitName() + " Monitor</h1>";
    html += "<div class='nav'>";
    html += "<a href='/'>Dashboard</a>";
    html += "<a href='/calibration'>Calibration</a>";
    html += "<a href='/charts'>Charts</a>";
    html += "<button class='theme-toggle' onclick='toggleTheme()' id='themeToggle'>☀️</button>";
    html += "</div></div>";

    html += "<div class='status-bar'>";
    html += "<div class='status-item'><div class='status-dot'></div><span>Connected to: <strong>" + wifiManager->getSSID() + "</strong></span></div>";
    html += "<div class='status-item'><span>📡 IP: <strong>" + wifiManager->getIPAddress() + "</strong></span></div>";
    html += "<div class='status-item' id='mqttStatusItem'><span id='mqttIndicator'>📊 MQTT: <span id='mqttStatus'>Checking...</span></span></div>";
    html += "<div class='status-item'><span>⏱️ Update: <span id='lastUpdate'>--</span></span></div>";
    html += "</div>";

    String calWarning = "";
    if (!calibrationManager->hasValidPHCalibration() || !calibrationManager->hasValidECCalibration()) {
        calWarning = "<div class='warning-banner'>⚠️ Sensors require calibration for accurate readings. ";
        calWarning += "<a href='/calibration'>Click here to calibrate</a></div>";
    }
    html += calWarning;

    html += "<div class='sensor-grid'>";

    // Temperature
    html += "<div class='sensor-card' style='--card-color: var(--temp-color)'>";
    html += "<div class='sensor-label'>Temperature</div>";
    html += "<div class='sensor-value'><span id='temp'>";
    html += dataValid ? String(temp_c, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>°Celsius</div>";
    html += "</div>";

    // ORP
    html += "<div class='sensor-card' style='--card-color: var(--orp-color)'>";
    html += "<div class='sensor-label'>ORP</div>";
    html += "<div class='sensor-value'><span id='orp'>";
    html += dataValid ? String(orp_mv, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>Millivolts</div>";
    html += "</div>";

    // pH
    html += "<div class='sensor-card' style='--card-color: var(--ph-color)'>";
    html += "<div class='sensor-label'>pH Level</div>";
    html += "<div class='sensor-value'><span id='ph'>";
    html += dataValid ? String(ph, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>pH Units</div>";
    if (!calibrationManager->hasValidPHCalibration()) {
        html += "<div class='sensor-status uncalibrated'>⚠ Uncalibrated</div>";
    } else {
        html += "<div class='sensor-status calibrated'>✓ Calibrated</div>";
    }
    html += "</div>";

    // EC
    html += "<div class='sensor-card' style='--card-color: var(--ec-color)'>";
    html += "<div class='sensor-label'>Conductivity</div>";
    html += "<div class='sensor-value'><span id='ec'>";
    html += dataValid ? String(ec_ms_cm, 3) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>mS/cm</div>";
    if (!calibrationManager->hasValidECCalibration()) {
        html += "<div class='sensor-status uncalibrated'>⚠ Uncalibrated</div>";
    } else {
        html += "<div class='sensor-status calibrated'>✓ Calibrated</div>";
    }
    html += "</div>";

    html += "</div>";

    html += "<div class='info-footer'>Auto-refresh every 2 seconds | Real-time monitoring active<br>Scott McLelslie to my beloved wife Kate 2026. Happy new year</div>";

    html += "<script>";
    html += "initTheme();";
    html += "setInterval(updateData, 2000);";
    html += "setInterval(updateMqttStatus, 5000);";
    html += "updateData();";
    html += "updateMqttStatus();";
    html += "</script>";
    html += "</body></html>";

    return html;
}

String AquariumWebServer::generateProvisioningPage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<link rel='icon' href='data:image/svg+xml,<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\"><text y=\".9em\" font-size=\"90\">🐠</text></svg>'>";
    html += "<title>Aquarium Setup</title>";
    html += "<style>";
    html += ":root {";
    html += "  --bg-primary: #f0f8ff;";
    html += "  --bg-card: #ffffff;";
    html += "  --text-primary: #333333;";
    html += "  --text-secondary: #666666;";
    html += "  --color-primary: #006494;";
    html += "  --color-primary-hover: #004d73;";
    html += "  --color-success: #28a745;";
    html += "  --color-success-hover: #218838;";
    html += "  --border-color: #dddddd;";
    html += "  --shadow: rgba(0,0,0,0.1);";
    html += "  --network-item-bg: #f8f9fa;";
    html += "  --network-item-hover: #e9ecef;";
    html += "}";
    html += "[data-theme='dark'] {";
    html += "  --bg-primary: #0a1929;";
    html += "  --bg-card: #132f4c;";
    html += "  --text-primary: #e3f2fd;";
    html += "  --text-secondary: #b0bec5;";
    html += "  --color-primary: #29b6f6;";
    html += "  --color-primary-hover: #0288d1;";
    html += "  --color-success: #4caf50;";
    html += "  --color-success-hover: #45a049;";
    html += "  --border-color: #1e4976;";
    html += "  --shadow: rgba(0,0,0,0.3);";
    html += "  --network-item-bg: #1e4976;";
    html += "  --network-item-hover: #2a5a8f;";
    html += "}";
    html += "body { font-family: Arial, sans-serif; max-width: 500px; margin: 0 auto; padding: 20px; background: var(--bg-primary); color: var(--text-primary); transition: background 0.3s, color 0.3s; }";
    html += "h1 { color: var(--color-primary); text-align: center; }";
    html += "h2 { color: var(--color-primary); }";
    html += ".theme-toggle { position: fixed; top: 20px; right: 20px; background: var(--bg-card); border: 2px solid var(--border-color); border-radius: 25px; padding: 8px 16px; cursor: pointer; font-size: 1.2em; box-shadow: 0 2px 5px var(--shadow); z-index: 1000; transition: all 0.3s; }";
    html += ".theme-toggle:hover { transform: scale(1.05); }";
    html += ".card { background: var(--bg-card); border-radius: 10px; padding: 20px; box-shadow: 0 2px 5px var(--shadow); margin: 20px 0; border: 1px solid var(--border-color); }";
    html += "input, select, button { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid var(--border-color); border-radius: 5px; box-sizing: border-box; background: var(--bg-card); color: var(--text-primary); }";
    html += "button { background: var(--color-primary); color: white; border: none; cursor: pointer; font-size: 1em; }";
    html += "button:hover { background: var(--color-primary-hover); }";
    html += ".btn-success { background: var(--color-success) !important; }";
    html += ".btn-success:hover { background: var(--color-success-hover) !important; }";
    html += ".network-item { padding: 10px; margin: 5px 0; background: var(--network-item-bg); border-radius: 5px; cursor: pointer; border: 1px solid var(--border-color); }";
    html += ".network-item:hover { background: var(--network-item-hover); }";
    html += ".info { color: var(--text-secondary); font-size: 0.9em; text-align: center; margin: 10px 0; }";
    html += "</style>";
    html += "<script>";
    html += "function initTheme() {";
    html += "  const savedTheme = localStorage.getItem('theme') || 'light';";
    html += "  document.documentElement.setAttribute('data-theme', savedTheme);";
    html += "  updateThemeIcon(savedTheme);";
    html += "}";
    html += "function toggleTheme() {";
    html += "  const current = document.documentElement.getAttribute('data-theme') || 'light';";
    html += "  const newTheme = current === 'light' ? 'dark' : 'light';";
    html += "  document.documentElement.setAttribute('data-theme', newTheme);";
    html += "  localStorage.setItem('theme', newTheme);";
    html += "  updateThemeIcon(newTheme);";
    html += "}";
    html += "function updateThemeIcon(theme) {";
    html += "  const btn = document.getElementById('themeToggle');";
    html += "  btn.textContent = theme === 'light' ? '🌙' : '☀️';";
    html += "  btn.title = theme === 'light' ? 'Switch to dark mode' : 'Switch to light mode';";
    html += "}";
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
    html += "initTheme();";
    html += "</script>";
    html += "</head><body>";
    html += "<button id='themeToggle' class='theme-toggle' onclick='toggleTheme()'>🌙</button>";
    html += "<h1>🐠 Aquarium Setup</h1>";

    html += "<div class='card'>";
    html += "<h2>WiFi Configuration</h2>";
    html += "<p class='info'>Connect your aquarium controller to your WiFi network</p>";

    html += "<form action='/save-wifi' method='POST'>";
    html += "<input type='text' id='ssid' name='ssid' placeholder='WiFi Network Name (SSID)' required>";
    html += "<input type='password' name='password' placeholder='WiFi Password' required>";
    html += "<button type='submit'>Connect to WiFi</button>";
    html += "</form>";

    html += "<button onclick='scanNetworks()' class='btn-success' style='margin-top: 10px;'>Scan for Networks</button>";
    html += "<div id='networks' style='margin-top: 15px;'></div>";
    html += "</div>";

    html += "<div class='info'>After connecting to WiFi, access at http://aquarium.local</div>";
    html += "<div class='info'>Current AP: " + String(WIFI_AP_SSID) + " | IP: 192.168.4.1</div>";
    html += "<div class='info' style='margin-top: 20px;'>Scott McLelslie to my beloved wife Kate 2026. Happy new year</div>";

    html += "</body></html>";

    return html;
}

// ============================================================================
// Calibration Handlers
// ============================================================================

void AquariumWebServer::handleCalibrationPage(AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateCalibrationPage());
}

void AquariumWebServer::handleChartsPage(AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateChartsPage());
}

void AquariumWebServer::handleGetHistory(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["ntp_synced"] = ntpInitialized;
    doc["count"] = historyCount;
    doc["interval_ms"] = HISTORY_INTERVAL_MS;

    JsonArray dataArray = doc["data"].to<JsonArray>();

    // Read data from circular buffer in chronological order
    int startIdx = historyCount < HISTORY_SIZE ? 0 : historyHead;
    for (int i = 0; i < historyCount; i++) {
        int idx = (startIdx + i) % HISTORY_SIZE;
        if (history[idx].valid) {
            JsonObject point = dataArray.add<JsonObject>();
            point["t"] = (long long)history[idx].timestamp;
            point["temp"] = serialized(String(history[idx].temp_c, 2));
            point["orp"] = serialized(String(history[idx].orp_mv, 2));
            point["ph"] = serialized(String(history[idx].ph, 2));
            point["ec"] = serialized(String(history[idx].ec_ms_cm, 3));
        }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleGetCalibrationStatus(AsyncWebServerRequest *request) {
    JsonDocument doc;

    // pH calibration status
    auto phCal = calibrationManager->getPHCalibration();
    doc["ph"]["calibrated"] = phCal.isCalibrated;
    doc["ph"]["two_point"] = phCal.hasTwoPoints;
    doc["ph"]["point1_pH"] = phCal.point1_pH;
    doc["ph"]["point1_ugs_mV"] = phCal.point1_ugs_mV;
    doc["ph"]["point2_pH"] = phCal.point2_pH;
    doc["ph"]["point2_ugs_mV"] = phCal.point2_ugs_mV;
    doc["ph"]["sensitivity"] = phCal.sensitivity_mV_pH;
    doc["ph"]["timestamp"] = phCal.timestamp;

    // EC calibration status
    auto ecCal = calibrationManager->getECCalibration();
    doc["ec"]["calibrated"] = ecCal.isCalibrated;
    doc["ec"]["cell_constant"] = ecCal.cellConstant_per_cm;
    doc["ec"]["solution"] = ecCal.cal_solution_mS_cm;
    doc["ec"]["temp"] = ecCal.cal_temp_C;
    doc["ec"]["timestamp"] = ecCal.timestamp;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleGetRawReadings(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["valid"] = dataValid;
    doc["temp_mC"] = raw_temp_mC;
    doc["orp_uV"] = raw_orp_uV;
    doc["ugs_uV"] = raw_ugs_uV;
    doc["ec_nA"] = raw_ec_nA;
    doc["ec_uV"] = raw_ec_uV;

    // Converted values for display
    doc["temp_C"] = temp_c;
    doc["orp_mV"] = orp_mv;
    doc["ugs_mV"] = raw_ugs_uV / 1000.0;
    if (raw_ec_nA != 0) {
        doc["ec_resistance_ohm"] = (float)raw_ec_uV / (float)raw_ec_nA;
    } else {
        doc["ec_resistance_ohm"] = 0.0;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleCalibratePhOnePoint(AsyncWebServerRequest *request) {
    if (!request->hasParam("buffer_pH", true) || !request->hasParam("measured_ugs_mV", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
        return;
    }

    float buffer_pH = request->getParam("buffer_pH", true)->value().toFloat();
    float measured_ugs_mV = request->getParam("measured_ugs_mV", true)->value().toFloat();

    bool success = calibrationManager->calibratePH_1Point(buffer_pH, measured_ugs_mV);

    JsonDocument doc;
    doc["success"] = success;
    if (success) {
        doc["message"] = "pH 1-point calibration successful";
    } else {
        doc["error"] = "Calibration failed";
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleCalibratePhTwoPoint(AsyncWebServerRequest *request) {
    if (!request->hasParam("buffer1_pH", true) || !request->hasParam("measured1_ugs_mV", true) ||
        !request->hasParam("buffer2_pH", true) || !request->hasParam("measured2_ugs_mV", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
        return;
    }

    float buffer1_pH = request->getParam("buffer1_pH", true)->value().toFloat();
    float measured1_ugs_mV = request->getParam("measured1_ugs_mV", true)->value().toFloat();
    float buffer2_pH = request->getParam("buffer2_pH", true)->value().toFloat();
    float measured2_ugs_mV = request->getParam("measured2_ugs_mV", true)->value().toFloat();

    bool success = calibrationManager->calibratePH_2Point(buffer1_pH, measured1_ugs_mV,
                                                          buffer2_pH, measured2_ugs_mV);

    JsonDocument doc;
    doc["success"] = success;
    if (success) {
        doc["message"] = "pH 2-point calibration successful";
    } else {
        doc["error"] = "Calibration failed (pH values too close)";
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleCalibrateEC(AsyncWebServerRequest *request) {
    if (!request->hasParam("known_conductivity", true) || !request->hasParam("temperature", true) ||
        !request->hasParam("measured_ec_nA", true) || !request->hasParam("measured_ec_uV", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
        return;
    }

    float known_conductivity = request->getParam("known_conductivity", true)->value().toFloat();
    float temperature = request->getParam("temperature", true)->value().toFloat();
    int32_t measured_ec_nA = request->getParam("measured_ec_nA", true)->value().toInt();
    int32_t measured_ec_uV = request->getParam("measured_ec_uV", true)->value().toInt();

    bool success = calibrationManager->calibrateEC(known_conductivity, temperature,
                                                   measured_ec_nA, measured_ec_uV);

    JsonDocument doc;
    doc["success"] = success;
    if (success) {
        doc["message"] = "EC calibration successful";
        doc["cell_constant"] = calibrationManager->getECCalibration().cellConstant_per_cm;
    } else {
        doc["error"] = "Calibration failed (invalid measurement)";
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleClearPhCalibration(AsyncWebServerRequest *request) {
    calibrationManager->clearPHCalibration();

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "pH calibration cleared";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleClearEcCalibration(AsyncWebServerRequest *request) {
    calibrationManager->clearECCalibration();

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "EC calibration cleared";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleGetMQTTConfig(AsyncWebServerRequest *request) {
    MQTTConfiguration config = mqttManager->getMQTTConfig();

    JsonDocument doc;
    doc["enabled"] = config.enabled;
    doc["broker_host"] = config.broker_host;
    doc["broker_port"] = config.broker_port;
    doc["username"] = config.username;
    doc["password"] = config.password;
    doc["device_id"] = config.device_id;
    doc["publish_interval_ms"] = config.publish_interval_ms;
    doc["discovery_enabled"] = config.discovery_enabled;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleSaveMQTTConfig(AsyncWebServerRequest *request) {
    MQTTConfiguration config;

    // Parse form parameters
    if (request->hasParam("enabled", true)) {
        String enabled = request->getParam("enabled", true)->value();
        config.enabled = (enabled == "true" || enabled == "1");
    } else {
        config.enabled = false;
    }

    if (request->hasParam("broker_host", true)) {
        String host = request->getParam("broker_host", true)->value();
        strncpy(config.broker_host, host.c_str(), sizeof(config.broker_host) - 1);
        config.broker_host[sizeof(config.broker_host) - 1] = '\0';
    }

    if (request->hasParam("broker_port", true)) {
        config.broker_port = request->getParam("broker_port", true)->value().toInt();
    } else {
        config.broker_port = 1883;
    }

    if (request->hasParam("username", true)) {
        String username = request->getParam("username", true)->value();
        strncpy(config.username, username.c_str(), sizeof(config.username) - 1);
        config.username[sizeof(config.username) - 1] = '\0';
    }

    if (request->hasParam("password", true)) {
        String password = request->getParam("password", true)->value();
        strncpy(config.password, password.c_str(), sizeof(config.password) - 1);
        config.password[sizeof(config.password) - 1] = '\0';
    }

    if (request->hasParam("device_id", true)) {
        String deviceId = request->getParam("device_id", true)->value();
        strncpy(config.device_id, deviceId.c_str(), sizeof(config.device_id) - 1);
        config.device_id[sizeof(config.device_id) - 1] = '\0';
    } else {
        strncpy(config.device_id, "aquarium", sizeof(config.device_id) - 1);
    }

    if (request->hasParam("publish_interval_ms", true)) {
        config.publish_interval_ms = request->getParam("publish_interval_ms", true)->value().toInt();
    } else {
        config.publish_interval_ms = 5000;
    }

    if (request->hasParam("discovery_enabled", true)) {
        String discovery = request->getParam("discovery_enabled", true)->value();
        config.discovery_enabled = (discovery == "true" || discovery == "1");
    } else {
        config.discovery_enabled = false;
    }

    bool success = mqttManager->saveMQTTConfig(config);

    JsonDocument doc;
    doc["success"] = success;
    doc["message"] = success ? "MQTT configuration saved" : "Failed to save MQTT configuration";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleGetMQTTStatus(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["connected"] = mqttManager->isConnected();
    doc["status"] = mqttManager->getConnectionStatus();
    doc["error"] = mqttManager->getLastError();

    MQTTConfiguration config = mqttManager->getMQTTConfig();
    doc["enabled"] = config.enabled;
    doc["broker"] = String(config.broker_host) + ":" + String(config.broker_port);
    doc["device_id"] = config.device_id;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

String AquariumWebServer::getUnitName() {
    Preferences prefs;
    if (!prefs.begin("system", true)) {
        return "Kate's Aquarium #7";  // Default name
    }
    String name = prefs.getString("unit_name", "Kate's Aquarium #7");
    prefs.end();
    return name;
}

void AquariumWebServer::handleGetUnitName(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["name"] = getUnitName();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleSaveUnitName(AsyncWebServerRequest *request) {
    String unitName = "Kate's Aquarium #7";  // Default

    if (request->hasParam("name", true)) {
        unitName = request->getParam("name", true)->value();
        // Limit length to 50 characters
        if (unitName.length() > 50) {
            unitName = unitName.substring(0, 50);
        }
    }

    Preferences prefs;
    bool success = false;

    if (prefs.begin("system", false)) {
        prefs.putString("unit_name", unitName);
        prefs.end();
        success = true;
    }

    JsonDocument doc;
    doc["success"] = success;
    doc["message"] = success ? "Unit name saved" : "Failed to save unit name";
    doc["name"] = unitName;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

String AquariumWebServer::generateCalibrationPage() {
    String html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <link rel='icon' href='data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><text y=".9em" font-size="90">🐠</text></svg>'>
    <title>Sensor Calibration</title>
    <style>
        :root {
            --bg-primary: #f8fafc;
            --bg-card: #ffffff;
            --text-primary: #0f172a;
            --text-secondary: #475569;
            --color-primary: #0ea5e9;
            --color-primary-hover: #0284c7;
            --color-secondary: #8b5cf6;
            --color-danger: #dc3545;
            --color-danger-hover: #c82333;
            --border-color: #e2e8f0;
            --shadow: rgba(14, 165, 233, 0.1);
            --glow: rgba(14, 165, 233, 0.2);
            --status-calibrated-bg: #d4edda;
            --status-calibrated-text: #155724;
            --status-uncalibrated-bg: #fff3cd;
            --status-uncalibrated-text: #856404;
            --info-bg: #d1ecf1;
            --info-text: #0c5460;
            --warning-bg: #fff3cd;
            --warning-text: #856404;
            --success-bg: #d4edda;
            --success-text: #155724;
            --error-bg: #f8d7da;
            --error-text: #721c24;
            --readings-bg: #f1f5f9;
            --steps-bg: #f8fafc;
            --steps-border: #0ea5e9;
        }
        [data-theme='dark'] {
            --bg-primary: #0a0e1a;
            --bg-card: #1a1f2e;
            --text-primary: #e0e7ff;
            --text-secondary: #94a3b8;
            --color-primary: #00d4ff;
            --color-primary-hover: #00b8e6;
            --color-secondary: #7c3aed;
            --color-danger: #ef5350;
            --color-danger-hover: #e53935;
            --border-color: #1e293b;
            --shadow: rgba(0, 212, 255, 0.1);
            --glow: rgba(0, 212, 255, 0.3);
            --status-calibrated-bg: #2e7d32;
            --status-calibrated-text: #c8e6c9;
            --status-uncalibrated-bg: #7f6003;
            --status-uncalibrated-text: #fff3cd;
            --info-bg: #0c5460;
            --info-text: #d1ecf1;
            --warning-bg: #7f6003;
            --warning-text: #fff3cd;
            --success-bg: #2e7d32;
            --success-text: #c8e6c9;
            --error-bg: #c62828;
            --error-text: #ffcdd2;
            --readings-bg: #1e293b;
            --steps-bg: #1e293b;
            --steps-border: #00d4ff;
        }
        * { box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            max-width: 900px;
            margin: 0 auto;
            padding: 20px;
            background: var(--bg-primary);
            color: var(--text-primary);
            transition: background 0.3s, color 0.3s;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
            padding: 20px;
            background: var(--bg-card);
            border-radius: 15px;
            border: 1px solid var(--border-color);
            box-shadow: 0 4px 20px var(--shadow);
        }
        h1 {
            font-size: 2em;
            background: linear-gradient(135deg, var(--color-primary), var(--color-secondary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            font-weight: 700;
            letter-spacing: -0.5px;
            margin: 0;
        }
        h2 { color: var(--color-primary); margin-top: 30px; }
        h3 { color: var(--color-primary); }
        .nav {
            display: flex;
            gap: 15px;
            align-items: center;
        }
        .nav a, .theme-toggle {
            padding: 10px 20px;
            background: var(--bg-primary);
            color: var(--text-primary);
            text-decoration: none;
            border-radius: 8px;
            border: 1px solid var(--border-color);
            transition: all 0.3s ease;
            font-size: 0.9em;
            font-weight: 500;
            cursor: pointer;
        }
        .nav a:hover, .theme-toggle:hover {
            background: var(--color-primary);
            color: var(--bg-primary);
            box-shadow: 0 0 20px var(--glow);
            transform: translateY(-2px);
        }
        .card {
            background: var(--bg-card);
            border-radius: 10px;
            padding: 20px;
            box-shadow: 0 2px 5px var(--shadow);
            margin: 20px 0;
            border: 1px solid var(--border-color);
        }
        .status {
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-weight: bold;
        }
        .status.calibrated { background: var(--status-calibrated-bg); color: var(--status-calibrated-text); }
        .status.uncalibrated { background: var(--status-uncalibrated-bg); color: var(--status-uncalibrated-text); }
        .form-group { margin: 15px 0; }
        label {
            display: block;
            margin-bottom: 5px;
            color: var(--text-primary);
            font-weight: bold;
        }
        input, select {
            width: 100%;
            padding: 10px;
            border: 1px solid var(--border-color);
            border-radius: 5px;
            font-size: 1em;
            background: var(--bg-card);
            color: var(--text-primary);
        }
        button {
            background: var(--color-primary);
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 1em;
            margin: 5px;
        }
        button:hover { background: var(--color-primary-hover); }
        button.secondary { background: var(--color-secondary); }
        button.secondary:hover { background: var(--color-secondary-hover); }
        button.danger { background: var(--color-danger); }
        button.danger:hover { background: var(--color-danger-hover); }
        .info {
            background: var(--info-bg);
            color: var(--info-text);
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-size: 0.9em;
        }
        .warning {
            background: var(--warning-bg);
            color: var(--warning-text);
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .success {
            background: var(--success-bg);
            color: var(--success-text);
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .error {
            background: var(--error-bg);
            color: var(--error-text);
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .readings {
            background: var(--readings-bg);
            padding: 15px;
            border-radius: 5px;
            margin: 10px 0;
            border: 1px solid var(--border-color);
        }
        .readings div {
            margin: 5px 0;
            font-family: monospace;
            color: var(--text-primary);
        }
        .hidden { display: none; }
        .steps {
            background: var(--steps-bg);
            padding: 15px;
            border-left: 4px solid var(--steps-border);
            margin: 10px 0;
            border-radius: 5px;
        }
        .steps ol { margin: 10px 0; padding-left: 20px; }
        .steps li { margin: 5px 0; }
    </style>
</head>
<body>
    <div class='header'>
        <h1>🔬 Sensor Calibration</h1>
        <div class='nav'>
            <a href='/'>Dashboard</a>
            <a href='/calibration'>Calibration</a>
            <a href='/charts'>Charts</a>
            <button class='theme-toggle' onclick='toggleTheme()' id='themeToggle'>🌙</button>
        </div>
    </div>

    <div id='messages'></div>

    <!-- Unit Name Configuration Card -->
    <div class='card'>
        <h2>Unit Name Configuration</h2>
        <div class='info'>
            <strong>Customize your unit name:</strong><br>
            This name will appear in the dashboard, charts, and data exports.
        </div>

        <div class='form-group'>
            <label>Unit Name:</label>
            <input type='text' id='unit_name' placeholder='e.g., Kate&apos;s Aquarium #7' maxlength='50' value='Kate&apos;s Aquarium #7'>
            <small>Maximum 50 characters</small>
        </div>

        <button onclick='saveUnitName()'>Save Unit Name</button>
    </div>

    <!-- Current Readings Card -->
    <div class='card'>
        <h2>Current Sensor Readings</h2>
        <button onclick='refreshReadings()'>🔄 Refresh Readings</button>
        <div id='currentReadings' class='readings'>
            <div>Loading...</div>
        </div>
    </div>

    <!-- pH Calibration Card -->
    <div class='card'>
        <h2>pH Calibration</h2>
        <div id='phStatus' class='status'>Loading...</div>

        <div class='steps'>
            <strong>Calibration Procedure:</strong>
            <ol>
                <li>Rinse the pH sensor with distilled water and pat dry</li>
                <li>Immerse sensor in pH buffer solution (pH 4.0, 7.0, or 10.0)</li>
                <li>Wait 1-2 minutes for reading to stabilize</li>
                <li>Click "Refresh Readings" to get current Ugs value</li>
                <li>Enter buffer pH and measured Ugs voltage below</li>
                <li>For best accuracy, use 2-point calibration with pH 4.0 and 7.0 buffers</li>
            </ol>
        </div>

        <h3>1-Point Calibration (Offset Only)</h3>
        <div class='form-group'>
            <label>Buffer pH:</label>
            <select id='ph1_buffer'>
                <option value='4.0'>pH 4.0</option>
                <option value='7.0' selected>pH 7.0</option>
                <option value='10.0'>pH 10.0</option>
            </select>
        </div>
        <div class='form-group'>
            <label>Measured Ugs (mV):</label>
            <input type='number' step='0.001' id='ph1_ugs' placeholder='e.g., 2999.908'>
        </div>
        <button onclick='calibratePh1Point()'>Calibrate pH (1-Point)</button>

        <h3>2-Point Calibration (Offset + Slope)</h3>
        <div class='form-group'>
            <label>Buffer 1 pH:</label>
            <select id='ph2_buffer1'>
                <option value='4.0' selected>pH 4.0</option>
                <option value='7.0'>pH 7.0</option>
                <option value='10.0'>pH 10.0</option>
            </select>
        </div>
        <div class='form-group'>
            <label>Measured Ugs 1 (mV):</label>
            <input type='number' step='0.001' id='ph2_ugs1' placeholder='e.g., 3155.908'>
        </div>
        <div class='form-group'>
            <label>Buffer 2 pH:</label>
            <select id='ph2_buffer2'>
                <option value='4.0'>pH 4.0</option>
                <option value='7.0' selected>pH 7.0</option>
                <option value='10.0'>pH 10.0</option>
            </select>
        </div>
        <div class='form-group'>
            <label>Measured Ugs 2 (mV):</label>
            <input type='number' step='0.001' id='ph2_ugs2' placeholder='e.g., 2999.908'>
        </div>
        <button onclick='calibratePh2Point()'>Calibrate pH (2-Point)</button>
        <button class='danger' onclick='clearPhCal()'>Clear pH Calibration</button>
    </div>

    <!-- EC Calibration Card -->
    <div class='card'>
        <h2>EC Calibration</h2>
        <div id='ecStatus' class='status'>Loading...</div>

        <div class='steps'>
            <strong>Calibration Procedure:</strong>
            <ol>
                <li>Rinse the EC sensor with distilled water and pat dry</li>
                <li>Immerse sensor in known conductivity solution (e.g., 0.01M KCl = 1.41 mS/cm @ 25°C)</li>
                <li>Wait 1-2 minutes for reading to stabilize</li>
                <li>Measure solution temperature accurately</li>
                <li>Click "Refresh Readings" to get current EC measurement</li>
                <li>Enter known conductivity, temperature, and measured values below</li>
            </ol>
        </div>

        <div class='info'>
            <strong>Common calibration solutions:</strong><br>
            • 0.01M KCl: 1.41 mS/cm @ 25°C<br>
            • 0.1M KCl: 12.88 mS/cm @ 25°C<br>
            • 1M KCl: 111.9 mS/cm @ 25°C
        </div>

        <div class='form-group'>
            <label>Known Conductivity (mS/cm):</label>
            <input type='number' step='0.001' id='ec_known' placeholder='e.g., 1.41' value='1.41'>
        </div>
        <div class='form-group'>
            <label>Solution Temperature (°C):</label>
            <input type='number' step='0.1' id='ec_temp' placeholder='e.g., 25.0' value='25.0'>
        </div>
        <div class='form-group'>
            <label>Measured EC Current (nA):</label>
            <input type='number' id='ec_nA' placeholder='e.g., 66000'>
        </div>
        <div class='form-group'>
            <label>Measured EC Voltage (uV):</label>
            <input type='number' id='ec_uV' placeholder='e.g., 66000'>
        </div>
        <button onclick='calibrateEc()'>Calibrate EC</button>
        <button class='danger' onclick='clearEcCal()'>Clear EC Calibration</button>
    </div>

    <!-- MQTT Configuration Card -->
    <div class='card'>
        <h2>MQTT Configuration</h2>
        <div id='mqttStatus' class='status'>Loading...</div>

        <div class='info'>
            <strong>MQTT Setup:</strong><br>
            Configure MQTT broker connection to publish sensor data to Home Assistant or other MQTT subscribers.
        </div>

        <div class='form-group'>
            <label>
                <input type='checkbox' id='mqtt_enabled' onchange='updateMqttStatus()'>
                Enable MQTT Publishing
            </label>
        </div>

        <div class='form-group'>
            <label>Broker Host/IP:</label>
            <input type='text' id='mqtt_broker_host' placeholder='e.g., 192.168.1.100 or mqtt.local'>
        </div>

        <div class='form-group'>
            <label>Broker Port:</label>
            <input type='number' id='mqtt_broker_port' placeholder='1883' value='1883'>
        </div>

        <div class='form-group'>
            <label>Device ID:</label>
            <input type='text' id='mqtt_device_id' placeholder='e.g., aquarium' value='aquarium'>
        </div>

        <div class='form-group'>
            <label>Publish Interval (ms):</label>
            <input type='number' id='mqtt_publish_interval' placeholder='5000' value='5000'>
            <small>Time between MQTT publishes (default: 5000ms)</small>
        </div>

        <div class='form-group'>
            <label>Username (optional):</label>
            <input type='text' id='mqtt_username' placeholder='MQTT username'>
        </div>

        <div class='form-group'>
            <label>Password (optional):</label>
            <input type='password' id='mqtt_password' placeholder='MQTT password'>
        </div>

        <div class='form-group'>
            <label>
                <input type='checkbox' id='mqtt_discovery'>
                Enable Home Assistant MQTT Discovery
            </label>
        </div>

        <div class='info'>
            <strong>MQTT Topics:</strong><br>
            • <code>aquarium/{device_id}/telemetry/temperature</code> - Temperature in °C<br>
            • <code>aquarium/{device_id}/telemetry/orp</code> - ORP in mV<br>
            • <code>aquarium/{device_id}/telemetry/ph</code> - pH value<br>
            • <code>aquarium/{device_id}/telemetry/ec</code> - EC in mS/cm<br>
            • <code>aquarium/{device_id}/telemetry/sensors</code> - Combined JSON payload
        </div>

        <button onclick='saveMqttConfig()'>Save MQTT Configuration</button>
        <button onclick='testMqttConnection()'>Test Connection</button>
    </div>

    <script>
        function initTheme() {
            const savedTheme = localStorage.getItem('theme') || 'dark';
            document.documentElement.setAttribute('data-theme', savedTheme);
            updateThemeIcon(savedTheme);
        }

        function toggleTheme() {
            const current = document.documentElement.getAttribute('data-theme') || 'dark';
            const newTheme = current === 'light' ? 'dark' : 'light';
            document.documentElement.setAttribute('data-theme', newTheme);
            localStorage.setItem('theme', newTheme);
            updateThemeIcon(newTheme);
        }

        function updateThemeIcon(theme) {
            const btn = document.getElementById('themeToggle');
            btn.textContent = theme === 'light' ? '🌙' : '☀️';
        }

        function showMessage(message, type) {
            const div = document.createElement('div');
            div.className = type;
            div.textContent = message;
            document.getElementById('messages').innerHTML = '';
            document.getElementById('messages').appendChild(div);
            setTimeout(() => div.remove(), 5000);
        }

        function refreshReadings() {
            fetch('/api/calibration/raw')
                .then(r => r.json())
                .then(data => {
                    const html = `
                        <div><strong>Temperature:</strong> ${data.temp_C.toFixed(2)} °C (${data.temp_mC} mC)</div>
                        <div><strong>ORP:</strong> ${data.orp_mV.toFixed(2)} mV (${data.orp_uV} uV)</div>
                        <div><strong>pH Ugs:</strong> ${data.ugs_mV.toFixed(3)} mV (${data.ugs_uV} uV)</div>
                        <div><strong>EC Current:</strong> ${data.ec_nA} nA</div>
                        <div><strong>EC Voltage:</strong> ${data.ec_uV} uV</div>
                        <div><strong>EC Resistance:</strong> ${data.ec_resistance_ohm.toFixed(1)} Ω</div>
                    `;
                    document.getElementById('currentReadings').innerHTML = html;

                    // Auto-populate EC fields
                    document.getElementById('ec_nA').value = data.ec_nA;
                    document.getElementById('ec_uV').value = data.ec_uV;
                    document.getElementById('ec_temp').value = data.temp_C.toFixed(1);
                });
        }

        function refreshStatus() {
            fetch('/api/calibration/status')
                .then(r => r.json())
                .then(data => {
                    // pH status
                    const phDiv = document.getElementById('phStatus');
                    if (data.ph.calibrated) {
                        phDiv.className = 'status calibrated';
                        phDiv.innerHTML = `✓ CALIBRATED (${data.ph.two_point ? '2-point' : '1-point'})<br>` +
                            `Sensitivity: ${data.ph.sensitivity.toFixed(2)} mV/pH`;
                    } else {
                        phDiv.className = 'status uncalibrated';
                        phDiv.textContent = '⚠ NOT CALIBRATED';
                    }

                    // EC status
                    const ecDiv = document.getElementById('ecStatus');
                    if (data.ec.calibrated) {
                        ecDiv.className = 'status calibrated';
                        ecDiv.innerHTML = `✓ CALIBRATED<br>Cell constant: ${data.ec.cell_constant.toFixed(4)} /cm`;
                    } else {
                        ecDiv.className = 'status uncalibrated';
                        ecDiv.textContent = '⚠ NOT CALIBRATED';
                    }
                });
        }

        function calibratePh1Point() {
            const buffer_pH = document.getElementById('ph1_buffer').value;
            const measured_ugs_mV = document.getElementById('ph1_ugs').value;

            if (!measured_ugs_mV) {
                showMessage('Please enter measured Ugs voltage', 'error');
                return;
            }

            const params = new URLSearchParams();
            params.append('buffer_pH', buffer_pH);
            params.append('measured_ugs_mV', measured_ugs_mV);

            fetch('/api/calibration/ph/1point', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message, 'success');
                        refreshStatus();
                    } else {
                        showMessage(data.error, 'error');
                    }
                });
        }

        function calibratePh2Point() {
            const buffer1_pH = document.getElementById('ph2_buffer1').value;
            const measured1_ugs_mV = document.getElementById('ph2_ugs1').value;
            const buffer2_pH = document.getElementById('ph2_buffer2').value;
            const measured2_ugs_mV = document.getElementById('ph2_ugs2').value;

            if (!measured1_ugs_mV || !measured2_ugs_mV) {
                showMessage('Please enter both Ugs voltage measurements', 'error');
                return;
            }

            const params = new URLSearchParams();
            params.append('buffer1_pH', buffer1_pH);
            params.append('measured1_ugs_mV', measured1_ugs_mV);
            params.append('buffer2_pH', buffer2_pH);
            params.append('measured2_ugs_mV', measured2_ugs_mV);

            fetch('/api/calibration/ph/2point', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message, 'success');
                        refreshStatus();
                    } else {
                        showMessage(data.error, 'error');
                    }
                });
        }

        function calibrateEc() {
            const known_conductivity = document.getElementById('ec_known').value;
            const temperature = document.getElementById('ec_temp').value;
            const measured_ec_nA = document.getElementById('ec_nA').value;
            const measured_ec_uV = document.getElementById('ec_uV').value;

            if (!known_conductivity || !temperature || !measured_ec_nA || !measured_ec_uV) {
                showMessage('Please fill in all EC calibration fields', 'error');
                return;
            }

            const params = new URLSearchParams();
            params.append('known_conductivity', known_conductivity);
            params.append('temperature', temperature);
            params.append('measured_ec_nA', measured_ec_nA);
            params.append('measured_ec_uV', measured_ec_uV);

            fetch('/api/calibration/ec', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message + ' - Cell constant: ' + data.cell_constant.toFixed(4) + ' /cm', 'success');
                        refreshStatus();
                    } else {
                        showMessage(data.error, 'error');
                    }
                });
        }

        function clearPhCal() {
            if (!confirm('Clear pH calibration? The sensor will revert to uncalibrated state.')) return;

            fetch('/api/calibration/ph/clear', { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    showMessage(data.message, 'success');
                    refreshStatus();
                });
        }

        function clearEcCal() {
            if (!confirm('Clear EC calibration? The sensor will revert to uncalibrated state.')) return;

            fetch('/api/calibration/ec/clear', { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    showMessage(data.message, 'success');
                    refreshStatus();
                });
        }

        function loadMqttConfig() {
            fetch('/api/mqtt/config')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('mqtt_enabled').checked = data.enabled;
                    document.getElementById('mqtt_broker_host').value = data.broker_host || '';
                    document.getElementById('mqtt_broker_port').value = data.broker_port || 1883;
                    document.getElementById('mqtt_device_id').value = data.device_id || 'aquarium';
                    document.getElementById('mqtt_publish_interval').value = data.publish_interval_ms || 5000;
                    document.getElementById('mqtt_username').value = data.username || '';
                    document.getElementById('mqtt_password').value = data.password || '';
                    document.getElementById('mqtt_discovery').checked = data.discovery_enabled || false;
                });
        }

        function refreshMqttStatus() {
            fetch('/api/mqtt/status')
                .then(r => r.json())
                .then(data => {
                    const mqttDiv = document.getElementById('mqttStatus');
                    if (data.connected) {
                        mqttDiv.className = 'status calibrated';
                        mqttDiv.innerHTML = `✓ CONNECTED<br>Broker: ${data.broker}<br>Device: ${data.device_id}`;
                    } else if (data.enabled) {
                        mqttDiv.className = 'status uncalibrated';
                        mqttDiv.innerHTML = `⚠ ${data.status}<br>${data.error || ''}`;
                    } else {
                        mqttDiv.className = 'status';
                        mqttDiv.textContent = 'MQTT Disabled';
                    }
                });
        }

        function saveMqttConfig() {
            const params = new URLSearchParams();
            params.append('enabled', document.getElementById('mqtt_enabled').checked);
            params.append('broker_host', document.getElementById('mqtt_broker_host').value);
            params.append('broker_port', document.getElementById('mqtt_broker_port').value);
            params.append('device_id', document.getElementById('mqtt_device_id').value);
            params.append('publish_interval_ms', document.getElementById('mqtt_publish_interval').value);
            params.append('username', document.getElementById('mqtt_username').value);
            params.append('password', document.getElementById('mqtt_password').value);
            params.append('discovery_enabled', document.getElementById('mqtt_discovery').checked);

            fetch('/api/mqtt/config', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message, 'success');
                        setTimeout(refreshMqttStatus, 2000); // Refresh after connection attempt
                    } else {
                        showMessage(data.message, 'error');
                    }
                });
        }

        function testMqttConnection() {
            saveMqttConfig(); // Save first, then check status
            setTimeout(() => {
                refreshMqttStatus();
            }, 3000);
        }

        function updateMqttStatus() {
            const enabled = document.getElementById('mqtt_enabled').checked;
            const inputs = ['mqtt_broker_host', 'mqtt_broker_port', 'mqtt_device_id',
                          'mqtt_publish_interval', 'mqtt_username', 'mqtt_password', 'mqtt_discovery'];
            inputs.forEach(id => {
                document.getElementById(id).disabled = !enabled;
            });
        }

        function loadUnitName() {
            fetch('/api/unit/name')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('unit_name').value = data.name || 'Kate\'s Aquarium #7';
                });
        }

        function saveUnitName() {
            const unitName = document.getElementById('unit_name').value;

            if (!unitName || unitName.trim() === '') {
                showMessage('Please enter a unit name', 'error');
                return;
            }

            const params = new URLSearchParams();
            params.append('name', unitName);

            fetch('/api/unit/name', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message + ' - Refresh page to see updated name in headers', 'success');
                    } else {
                        showMessage(data.message, 'error');
                    }
                });
        }

        // Initialize on page load
        initTheme();
        refreshReadings();
        refreshStatus();
        loadMqttConfig();
        refreshMqttStatus();
        loadUnitName();
        setInterval(refreshReadings, 5000);
        setInterval(refreshMqttStatus, 10000); // Update MQTT status every 10 seconds
    </script>

    <div style='text-align: center; padding: 20px; color: var(--text-secondary); font-size: 0.85em;'>
        Scott McLelslie to my beloved wife Kate 2026. Happy new year
    </div>
</body>
</html>
)rawliteral";

    return html;
}


String AquariumWebServer::generateChartsPage() {
    String html = String(CHARTS_PAGE_HTML);
    // Replace the hardcoded unit name with the actual unit name
    html.replace("Kate's Aquarium #7 Analytics", getUnitName() + " Analytics");
    return html;
}

void AquariumWebServer::handleExportCSV(AsyncWebServerRequest *request) {
    String csv = "";

    // Header with metadata
    csv += "# Aquarium Monitor Data Export\r\n";
    csv += "# Device: " + getUnitName() + " | Export time: ";

    time_t now = time(nullptr);
    if (now > 100000) {
        csv += ctime(&now);
    } else {
        csv += String(millis() / 1000);
        csv += " seconds since boot (NTP not synced)\r\n";
    }

    csv += "# WiFi: ";
    csv += wifiManager->getSSID();
    csv += "\r\n";
    csv += "# pH Calibration: ";
    csv += calibrationManager->hasValidPHCalibration() ? "Yes" : "No";
    csv += "\r\n";
    csv += "# EC Calibration: ";
    csv += calibrationManager->hasValidECCalibration() ? "Yes" : "No";
    csv += "\r\n";
    csv += "# Data Points: ";
    csv += String(historyCount);
    csv += "\r\n";
    csv += "# Interval: 5 seconds\r\n";
    csv += "#\r\n";

    // CSV Header
    csv += "Timestamp,Unix_Time,Temperature_C,ORP_mV,pH,EC_mS_cm,Valid\r\n";

    // Output data in chronological order
    int startIdx = historyCount < HISTORY_SIZE ? 0 : historyHead;

    for (int i = 0; i < historyCount; i++) {
        int idx = (startIdx + i) % HISTORY_SIZE;

        if (history[idx].valid) {
            // Format timestamp
            time_t ts = history[idx].timestamp;
            if (ts > 100000) {
                struct tm* timeinfo = localtime(&ts);
                char timeStr[32];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
                csv += timeStr;
            } else {
                csv += "N/A";
            }
            csv += ",";

            // Unix timestamp
            csv += String((long long)history[idx].timestamp);
            csv += ",";

            // Temperature
            csv += String(history[idx].temp_c, 2);
            csv += ",";

            // ORP
            csv += String(history[idx].orp_mv, 2);
            csv += ",";

            // pH
            csv += String(history[idx].ph, 2);
            csv += ",";

            // EC
            csv += String(history[idx].ec_ms_cm, 3);
            csv += ",";

            // Valid flag
            csv += "true\r\n";
        }
    }

    // Set appropriate headers for file download
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
    response->addHeader("Content-Disposition", "attachment; filename=aquarium-data.csv");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
}

void AquariumWebServer::handleExportJSON(AsyncWebServerRequest *request) {
    JsonDocument doc;

    time_t now = time(nullptr);

    // Device metadata
    doc["device"]["name"] = getUnitName();
    if (now > 100000) {
        doc["device"]["export_timestamp"] = (long long)now;
    } else {
        doc["device"]["export_timestamp"] = nullptr;
    }
    doc["device"]["uptime_seconds"] = millis() / 1000;
    doc["device"]["wifi_ssid"] = wifiManager->getSSID();
    doc["device"]["wifi_ip"] = wifiManager->getIPAddress();
    doc["device"]["ph_calibrated"] = calibrationManager->hasValidPHCalibration();
    doc["device"]["ec_calibrated"] = calibrationManager->hasValidECCalibration();
    doc["device"]["data_points"] = historyCount;
    doc["device"]["interval_seconds"] = 5;

    // Data array
    JsonArray dataArray = doc["data"].to<JsonArray>();

    int startIdx = historyCount < HISTORY_SIZE ? 0 : historyHead;
    int validCount = 0;

    for (int i = 0; i < historyCount; i++) {
        int idx = (startIdx + i) % HISTORY_SIZE;

        if (history[idx].valid) {
            validCount++;
            JsonObject point = dataArray.add<JsonObject>();
            point["timestamp"] = (long long)history[idx].timestamp;
            point["temp_c"] = serialized(String(history[idx].temp_c, 2));
            point["orp_mv"] = serialized(String(history[idx].orp_mv, 2));
            point["ph"] = serialized(String(history[idx].ph, 2));
            point["ec_ms_cm"] = serialized(String(history[idx].ec_ms_cm, 3));
            point["valid"] = true;
        }
    }

    // Summary
    doc["summary"]["total_points"] = validCount;

    String response;
    serializeJson(doc, response);

    AsyncWebServerResponse *asyncResponse = request->beginResponse(200, "application/json", response);
    asyncResponse->addHeader("Content-Disposition", "attachment; filename=aquarium-data.json");
    asyncResponse->addHeader("Cache-Control", "no-cache");
    request->send(asyncResponse);
}
