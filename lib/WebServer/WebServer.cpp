#include "WebServer.h"
#include "WiFiManager.h"
#include "CalibrationManager.h"
#include "MQTTManager.h"
#include "TankSettingsManager.h"
#include "WarningManager.h"
#include "DerivedMetrics.h"
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
      tankSettingsManager(nullptr), warningManager(nullptr),
      raw_temp_mC(0), raw_orp_uV(0), raw_ugs_uV(0), raw_ec_nA(0), raw_ec_uV(0),
      temp_c(0), orp_mv(0), ph(0), ec_ms_cm(0), lastUpdate(0), dataValid(false),
      tds_ppm(0), co2_ppm(0), toxic_ammonia_ratio(0), nh3_ppm(0), max_do_mg_l(0), stocking_density(0),
      historyHead(0), historyCount(0), lastHistoryUpdate(0), ntpInitialized(false) {
    // Initialize history buffer
    for (int i = 0; i < HISTORY_SIZE; i++) {
        history[i].valid = false;
    }
}

void AquariumWebServer::setTankSettingsManager(TankSettingsManager* mgr) {
    tankSettingsManager = mgr;
}

void AquariumWebServer::setWarningManager(WarningManager* mgr) {
    warningManager = mgr;
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
    // Add derived metrics
    dp.tds_ppm = tds_ppm;
    dp.co2_ppm = co2_ppm;
    dp.toxic_ammonia_ratio = toxic_ammonia_ratio;
    dp.nh3_ppm = nh3_ppm;
    dp.max_do_mg_l = max_do_mg_l;
    dp.stocking_density = stocking_density;
    dp.valid = dataValid;

    // Add warning states
    if (warningManager != nullptr) {
        SensorWarningState states = warningManager->getSensorState();
        dp.temp_state = (uint8_t)states.temperature.state;
        dp.ph_state = (uint8_t)states.ph.state;
        dp.nh3_state = (uint8_t)states.nh3.state;
        dp.orp_state = (uint8_t)states.orp.state;
        dp.ec_state = (uint8_t)states.conductivity.state;
        dp.do_state = (uint8_t)states.dissolved_oxygen.state;
    } else {
        dp.temp_state = 0; // STATE_UNKNOWN
        dp.ph_state = 0;
        dp.nh3_state = 0;
        dp.orp_state = 0;
        dp.ec_state = 0;
        dp.do_state = 0;
    }

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

    // Derived metrics API endpoint
    server.on("/api/metrics/derived", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetDerivedMetrics(request);
    });

    // Tank settings API endpoints
    server.on("/api/settings/tank", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetTankSettings(request);
    });

    server.on("/api/settings/tank", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleSaveTankSettings(request);
    });

    // Fish profile API endpoints
    server.on("/api/settings/fish", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetFishList(request);
    });

    server.on("/api/settings/fish/add", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleAddFish(request);
    });

    server.on("/api/settings/fish/remove", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleRemoveFish(request);
    });

    server.on("/api/settings/fish/clear", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleClearFish(request);
    });

    // Warning profile API endpoints
    server.on("/api/warnings/profile", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetWarningProfile(request);
    });

    server.on("/api/warnings/profile", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleSaveWarningProfile(request);
    });

    server.on("/api/warnings/states", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetWarningStates(request);
    });

    // 404 handler
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not Found");
    });
}

void AquariumWebServer::handleRoot(AsyncWebServerRequest *request) {
    if (wifiManager->isAPMode()) {
        String html = generateProvisioningPage();
        Serial.printf("Provisioning page length: %d bytes\n", html.length());
        request->send(200, "text/html", html);
    } else {
        // Redirect to charts page as the main interface
        handleChartsPage(request);
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

    // Calculate derived metrics (if tank settings manager is available)
    if (tankSettingsManager != nullptr) {
        TankSettings& settings = tankSettingsManager->getSettings();

        // TDS from EC
        tds_ppm = DerivedMetrics::calculateTDS(ec_ms_cm, settings.tds_conversion_factor);

        // CO2 from pH and KH
        co2_ppm = DerivedMetrics::calculateCO2(ph, settings.manual_kh_dkh);

        // Toxic ammonia ratio and actual NH3
        toxic_ammonia_ratio = DerivedMetrics::calculateToxicAmmoniaRatio(temp_c, ph);
        nh3_ppm = DerivedMetrics::calculateActualNH3(settings.manual_tan_ppm, toxic_ammonia_ratio);

        // Maximum dissolved oxygen
        max_do_mg_l = DerivedMetrics::calculateMaxDO(temp_c);

        // Stocking density
        float total_fish_length = tankSettingsManager->getTotalStockingLength();
        float tank_volume = settings.calculated_volume_liters;
        if (tank_volume <= 0.0 && settings.manual_volume_liters > 0.0) {
            tank_volume = settings.manual_volume_liters;
        }
        stocking_density = DerivedMetrics::calculateStockingDensity(total_fish_length, tank_volume);
    } else {
        // No tank settings available, use defaults
        tds_ppm = DerivedMetrics::calculateTDS(ec_ms_cm, 0.64);
        co2_ppm = DerivedMetrics::calculateCO2(ph, 4.0);
        toxic_ammonia_ratio = DerivedMetrics::calculateToxicAmmoniaRatio(temp_c, ph);
        nh3_ppm = 0.0;
        max_do_mg_l = DerivedMetrics::calculateMaxDO(temp_c);
        stocking_density = 0.0;
    }

    // Evaluate warning states (if warning manager is available)
    if (warningManager != nullptr) {
        warningManager->evaluateTemperature(temp_c);
        warningManager->evaluatePH(ph);
        warningManager->evaluateNH3(nh3_ppm);
        warningManager->evaluateORP(orp_mv);
        // Convert EC to µS/cm for evaluation
        warningManager->evaluateConductivity(ec_ms_cm * 1000.0);
        warningManager->evaluateDO(max_do_mg_l);
    }

    lastUpdate = millis();
    dataValid = true;
}

String AquariumWebServer::generateHomePage() {
    String html;
    html.reserve(24000);  // Pre-allocate memory for large HTML page (increased for derived metrics)
    html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
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
    html += "  --tds-color: #3b82f6;";
    html += "  --co2-color: #10b981;";
    html += "  --nh3-color: #f59e0b;";
    html += "  --do-color: #06b6d4;";
    html += "  --stock-color: #8b5cf6;";
    html += "  --warning-bg: #7f6003;";
    html += "  --warning-text: #fff3cd;";
    html += "  --success-bg: #2e7d32;";
    html += "  --success-text: #c8e6c9;";
    html += "  --danger-bg: #dc2626;";
    html += "  --danger-text: #fecaca;";
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
    html += ".nav a, .nav button, .theme-toggle { padding: 10px 20px; background: var(--bg-primary); color: var(--text-primary); text-decoration: none; border-radius: 8px; border: 1px solid var(--border-color); transition: all 0.3s ease; font-size: 0.9em; font-weight: 500; cursor: pointer; }";
    html += ".nav a:hover, .nav button:hover, .theme-toggle:hover { background: var(--color-primary); color: var(--bg-primary); box-shadow: 0 0 20px var(--glow); transform: translateY(-2px); }";
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
    html += ".alert-badge { font-size: 0.75em; margin-top: 8px; padding: 6px 12px; border-radius: 6px; display: inline-block; font-weight: 600; }";
    html += ".alert-danger { background: var(--danger-bg); color: var(--danger-text); }";
    html += ".alert-success { background: var(--success-bg); color: var(--success-text); }";
    html += ".alert-warning { background: var(--warning-bg); color: var(--warning-text); }";
    html += ".section-title { font-size: 1.3em; font-weight: 600; color: var(--text-primary); margin: 30px 0 15px 0; padding-left: 10px; border-left: 4px solid var(--color-primary); }";
    html += ".warning-banner { background: var(--warning-bg); color: var(--warning-text); padding: 15px; border-radius: 10px; margin: 20px 0; border: 1px solid var(--border-color); }";
    html += ".warning-banner a { color: var(--warning-text); text-decoration: underline; font-weight: bold; }";
    html += ".info-footer { text-align: center; padding: 15px; background: var(--bg-card); border-radius: 10px; margin-top: 20px; border: 1px solid var(--border-color); font-size: 0.85em; color: var(--text-secondary); }";
    // Warning system CSS
    html += ".warning-badge { display: inline-flex; align-items: center; gap: 6px; padding: 6px 12px; border-radius: 8px; font-size: 0.85em; font-weight: 600; margin-left: 10px; }";
    html += ".warning-badge.critical { background: var(--danger-bg); color: var(--danger-text); }";
    html += ".warning-badge.warning { background: var(--warning-bg); color: var(--warning-text); }";
    html += ".warning-badge.normal { background: var(--success-bg); color: var(--success-text); }";
    html += ".sensor-card.state-normal { border-color: var(--border-color); }";
    html += ".sensor-card.state-warning { border-color: #f59e0b; box-shadow: 0 0 15px rgba(245, 158, 11, 0.3); animation: warning-pulse 2s ease-in-out infinite; }";
    html += ".sensor-card.state-critical { border-color: #ef4444; box-shadow: 0 0 20px rgba(239, 68, 68, 0.5); animation: critical-pulse 1.5s ease-in-out infinite; }";
    html += "@keyframes warning-pulse { 0%, 100% { box-shadow: 0 0 15px rgba(245, 158, 11, 0.3); } 50% { box-shadow: 0 0 25px rgba(245, 158, 11, 0.6); } }";
    html += "@keyframes critical-pulse { 0%, 100% { box-shadow: 0 0 20px rgba(239, 68, 68, 0.5); } 50% { box-shadow: 0 0 35px rgba(239, 68, 68, 0.8); } }";
    html += ".sensor-card.state-warning .sensor-label::after { content: ' ⚠'; color: #f59e0b; }";
    html += ".sensor-card.state-critical .sensor-label::after { content: ' 🔴'; }";
    html += ".sensor-card[title] { cursor: help; }";
    html += "</style>";
    html += "<script>";
    html += "function initTheme() {";
    html += "  const savedTheme = localStorage.getItem('theme') || 'dark';";
    html += "  document.documentElement.setAttribute('data-theme', savedTheme);";
    html += "  updateThemeIcon(savedTheme);";
    html += "}\n";
    html += "function toggleTheme() {";
    html += "  const current = document.documentElement.getAttribute('data-theme') || 'dark';";
    html += "  const newTheme = current === 'light' ? 'dark' : 'light';";
    html += "  document.documentElement.setAttribute('data-theme', newTheme);";
    html += "  localStorage.setItem('theme', newTheme);";
    html += "  updateThemeIcon(newTheme);";
    html += "}\n";
    html += "function updateThemeIcon(theme) {";
    html += "  // Theme toggle button removed from this page";
    html += "  // Theme is now managed in calibration settings";
    html += "}\n";
    html += "const ConnectionState = {\n";
    html += "  failureCount: 0,\n";
    html += "  successCount: 0,\n";
    html += "  isConnected: true,\n";
    html += "  lastStatusChange: 0,\n";
    html += "  backoffLevel: 0,\n";
    html += "  retryTimer: null,\n";
    html += "  retryCountdown: 0,\n";
    html += "  FAILURE_THRESHOLD: 2,\n";
    html += "  SUCCESS_THRESHOLD: 1,\n";
    html += "  DEBOUNCE_MS: 1000,\n";
    html += "  BACKOFF_INTERVALS: [2000, 5000, 10000, 30000],\n";
    html += "  currentDataInterval: 2000,\n";
    html += "  currentMetricsInterval: 2000,\n";
    html += "  currentMqttInterval: 5000,\n";
    html += "  currentWarningsInterval: 2000,\n";
    html += "  dataIntervalId: null,\n";
    html += "  metricsIntervalId: null,\n";
    html += "  mqttIntervalId: null,\n";
    html += "  warningsIntervalId: null,\n";
    html += "  recordSuccess() {\n";
    html += "    this.successCount++;\n";
    html += "    this.failureCount = 0;\n";
    html += "    this.backoffLevel = 0;\n";
    html += "    if (!this.isConnected && this.successCount >= this.SUCCESS_THRESHOLD) {\n";
    html += "      this.setConnected(true);\n";
    html += "      this.restoreNormalPolling();\n";
    html += "    }\n";
    html += "  },\n";
    html += "  recordFailure() {\n";
    html += "    this.failureCount++;\n";
    html += "    this.successCount = 0;\n";
    html += "    if (this.isConnected && this.failureCount >= this.FAILURE_THRESHOLD) {\n";
    html += "      this.setConnected(false);\n";
    html += "      this.startBackoff();\n";
    html += "    } else if (!this.isConnected) {\n";
    html += "      this.increaseBackoff();\n";
    html += "    }\n";
    html += "  },\n";
    html += "  setConnected(connected) {\n";
    html += "    const now = Date.now();\n";
    html += "    if (now - this.lastStatusChange < this.DEBOUNCE_MS) return;\n";
    html += "    this.isConnected = connected;\n";
    html += "    this.lastStatusChange = now;\n";
    html += "    const statusDot = document.getElementById('statusDot');\n";
    html += "    const statusText = document.getElementById('statusText');\n";
    html += "    if (connected) {\n";
    html += "      statusDot.style.background = '#10b981';\n";
    html += "      statusText.textContent = 'Connected';\n";
    html += "      this.stopRetryCountdown();\n";
    html += "    } else {\n";
    html += "      statusDot.style.background = '#f59e0b';\n";
    html += "      this.startRetryCountdown();\n";
    html += "    }\n";
    html += "  },\n";
    html += "  startBackoff() {\n";
    html += "    this.backoffLevel = 0;\n";
    html += "    this.adjustPollingIntervals();\n";
    html += "  },\n";
    html += "  increaseBackoff() {\n";
    html += "    if (this.backoffLevel < this.BACKOFF_INTERVALS.length - 1) {\n";
    html += "      this.backoffLevel++;\n";
    html += "      this.adjustPollingIntervals();\n";
    html += "    }\n";
    html += "  },\n";
    html += "  adjustPollingIntervals() {\n";
    html += "    const backoffMs = this.BACKOFF_INTERVALS[this.backoffLevel];\n";
    html += "    if (this.dataIntervalId) clearInterval(this.dataIntervalId);\n";
    html += "    if (this.metricsIntervalId) clearInterval(this.metricsIntervalId);\n";
    html += "    if (this.mqttIntervalId) clearInterval(this.mqttIntervalId);\n";
    html += "    if (this.warningsIntervalId) clearInterval(this.warningsIntervalId);\n";
    html += "    this.currentDataInterval = Math.max(2000, backoffMs);\n";
    html += "    this.currentMetricsInterval = Math.max(2000, backoffMs);\n";
    html += "    this.currentMqttInterval = Math.max(5000, backoffMs);\n";
    html += "    this.currentWarningsInterval = Math.max(2000, backoffMs);\n";
    html += "    this.dataIntervalId = setInterval(updateData, this.currentDataInterval);\n";
    html += "    this.metricsIntervalId = setInterval(updateDerivedMetrics, this.currentMetricsInterval);\n";
    html += "    this.mqttIntervalId = setInterval(updateMqttStatus, this.currentMqttInterval);\n";
    html += "    this.warningsIntervalId = setInterval(updateWarningStates, this.currentWarningsInterval);\n";
    html += "    console.log('Polling adjusted: backoff=' + backoffMs + 'ms');\n";
    html += "  },\n";
    html += "  restoreNormalPolling() {\n";
    html += "    if (this.dataIntervalId) clearInterval(this.dataIntervalId);\n";
    html += "    if (this.metricsIntervalId) clearInterval(this.metricsIntervalId);\n";
    html += "    if (this.mqttIntervalId) clearInterval(this.mqttIntervalId);\n";
    html += "    if (this.warningsIntervalId) clearInterval(this.warningsIntervalId);\n";
    html += "    this.currentDataInterval = 2000;\n";
    html += "    this.currentMetricsInterval = 2000;\n";
    html += "    this.currentMqttInterval = 5000;\n";
    html += "    this.currentWarningsInterval = 2000;\n";
    html += "    this.dataIntervalId = setInterval(updateData, 2000);\n";
    html += "    this.metricsIntervalId = setInterval(updateDerivedMetrics, 2000);\n";
    html += "    this.mqttIntervalId = setInterval(updateMqttStatus, 5000);\n";
    html += "    this.warningsIntervalId = setInterval(updateWarningStates, 2000);\n";
    html += "    console.log('Polling restored to normal intervals');\n";
    html += "  },\n";
    html += "  startRetryCountdown() {\n";
    html += "    this.stopRetryCountdown();\n";
    html += "    const self = this;\n";
    html += "    const updateCountdown = function() {\n";
    html += "      const nextRetryInterval = Math.min(self.currentDataInterval, self.currentMetricsInterval, self.currentMqttInterval, self.currentWarningsInterval);\n";
    html += "      self.retryCountdown = Math.ceil(nextRetryInterval / 1000);\n";
    html += "      const statusText = document.getElementById('statusText');\n";
    html += "      const tick = function() {\n";
    html += "        if (self.retryCountdown > 0 && !self.isConnected) {\n";
    html += "          statusText.textContent = 'Connection Error (retry in ' + self.retryCountdown + 's)';\n";
    html += "          self.retryCountdown--;\n";
    html += "          self.retryTimer = setTimeout(tick, 1000);\n";
    html += "        } else if (!self.isConnected) {\n";
    html += "          statusText.textContent = 'Connection Error (retrying...)';\n";
    html += "          setTimeout(updateCountdown, nextRetryInterval);\n";
    html += "        }\n";
    html += "      };\n";
    html += "      tick();\n";
    html += "    };\n";
    html += "    updateCountdown();\n";
    html += "  },\n";
    html += "  stopRetryCountdown() {\n";
    html += "    if (this.retryTimer) {\n";
    html += "      clearTimeout(this.retryTimer);\n";
    html += "      this.retryTimer = null;\n";
    html += "    }\n";
    html += "  }\n";
    html += "};\n";
    html += "function updateData() {";
    html += "  fetch('/api/sensors')";
    html += "    .then(response => {";
    html += "      if (!response.ok) throw new Error('HTTP ' + response.status);";
    html += "      return response.json();";
    html += "    })";
    html += "    .then(data => {";
    html += "      if (data.valid) {";
    html += "        document.getElementById('temp').textContent = data.temperature_c.toFixed(2);";
    html += "        document.getElementById('orp').textContent = data.orp_mv.toFixed(2);";
    html += "        document.getElementById('ph').textContent = data.ph.toFixed(2);";
    html += "        document.getElementById('ec').textContent = data.ec_ms_cm.toFixed(3);";
    html += "      }";
    html += "      ConnectionState.recordSuccess();";
    html += "    })";
    html += "    .catch(err => {";
    html += "      console.error('Update failed:', err);";
    html += "      ConnectionState.recordFailure();";
    html += "    })";
    html += "    .finally(() => {";
    html += "      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();";
    html += "    });";
    html += "}\n";
    html += "function updateMqttStatus() {";
    html += "  fetch('/api/mqtt/status')";
    html += "    .then(response => {";
    html += "      if (!response.ok) throw new Error('HTTP ' + response.status);";
    html += "      return response.json();";
    html += "    })";
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
    html += "    })";
    html += "    .catch(err => {";
    html += "      console.error('MQTT status update failed:', err);";
    html += "    });";
    html += "}\n";
    html += "function updateDerivedMetrics() {";
    html += "  fetch('/api/metrics/derived')";
    html += "    .then(response => {";
    html += "      if (!response.ok) throw new Error('HTTP ' + response.status);";
    html += "      return response.json();";
    html += "    })";
    html += "    .then(data => {";
    html += "      if (data.valid) {";
    html += "        document.getElementById('tds').textContent = parseFloat(data.tds_ppm).toFixed(1);";
    html += "        document.getElementById('co2').textContent = parseFloat(data.co2_ppm).toFixed(2);";
    html += "        document.getElementById('nh3_ratio').textContent = (parseFloat(data.nh3_fraction) * 100).toFixed(2);";
    html += "        document.getElementById('nh3_ppm').textContent = parseFloat(data.nh3_ppm).toFixed(4);";
    html += "        document.getElementById('max_do').textContent = parseFloat(data.max_do_mg_l).toFixed(2);";
    html += "        document.getElementById('stock').textContent = parseFloat(data.stocking_density).toFixed(2);";
    html += "        const co2Val = parseFloat(data.co2_ppm);";
    html += "        const co2Card = document.getElementById('co2Card');";
    html += "        if (co2Val >= 15 && co2Val <= 30) {";
    html += "          co2Card.style.setProperty('--card-color', '#10b981');";
    html += "        } else if (co2Val < 15) {";
    html += "          co2Card.style.setProperty('--card-color', '#f59e0b');";
    html += "        } else {";
    html += "          co2Card.style.setProperty('--card-color', '#ef4444');";
    html += "        }";
    html += "        const nh3Val = parseFloat(data.nh3_ppm);";
    html += "        const nh3Alert = document.getElementById('nh3Alert');";
    html += "        if (nh3Val > 0.05) {";
    html += "          nh3Alert.style.display = 'inline-block';";
    html += "        } else {";
    html += "          nh3Alert.style.display = 'none';";
    html += "        }";
    html += "        const stockVal = parseFloat(data.stocking_density);";
    html += "        const stockCard = document.getElementById('stockCard');";
    html += "        if (stockVal < 1.0) {";
    html += "          stockCard.style.setProperty('--card-color', '#10b981');";
    html += "        } else if (stockVal <= 2.0) {";
    html += "          stockCard.style.setProperty('--card-color', '#f59e0b');";
    html += "        } else {";
    html += "          stockCard.style.setProperty('--card-color', '#ef4444');";
    html += "        }";
    html += "      }";
    html += "      ConnectionState.recordSuccess();";
    html += "    })";
    html += "    .catch(err => {";
    html += "      console.error('Derived metrics update failed:', err);";
    html += "      ConnectionState.recordFailure();";
    html += "    });";
    html += "}\n";
    html += "function updateWarningStates() {";
    html += "  fetch('/api/warnings/states')";
    html += "    .then(response => {";
    html += "      if (!response.ok) throw new Error('HTTP ' + response.status);";
    html += "      return response.json();";
    html += "    })";
    html += "    .then(data => {";
    html += "      const warningCount = data.warning_count || 0;";
    html += "      const criticalCount = data.critical_count || 0;";
    html += "      let badgeEl = document.getElementById('warningBadge');";
    html += "      if (!badgeEl) {";
    html += "        badgeEl = document.createElement('span');";
    html += "        badgeEl.id = 'warningBadge';";
    html += "        document.querySelector('h1').appendChild(badgeEl);";
    html += "      }";
    html += "      if (criticalCount > 0) {";
    html += "        badgeEl.className = 'warning-badge critical';";
    html += "        badgeEl.textContent = '🚨 ' + criticalCount + ' Critical';";
    html += "        badgeEl.style.display = 'inline-flex';";
    html += "      } else if (warningCount > 0) {";
    html += "        badgeEl.className = 'warning-badge warning';";
    html += "        badgeEl.textContent = '⚠ ' + warningCount + ' Warning' + (warningCount > 1 ? 's' : '');";
    html += "        badgeEl.style.display = 'inline-flex';";
    html += "      } else {";
    html += "        badgeEl.style.display = 'none';";
    html += "      }";
    html += "      updateCardState('tempCard', data.temperature, 'Temperature');";
    html += "      updateCardState('phCard', data.ph, 'pH');";
    html += "      updateCardState('nh3Card', data.nh3, 'NH3');";
    html += "      updateCardState('orpCard', data.orp, 'ORP');";
    html += "      updateCardState('ecCard', data.conductivity, 'EC');";
    html += "      updateCardState('doCard', data.dissolved_oxygen, 'DO');";
    html += "      ConnectionState.recordSuccess();";
    html += "    })";
    html += "    .catch(err => {";
    html += "      console.error('Warning states update failed:', err);";
    html += "      ConnectionState.recordFailure();";
    html += "    });";
    html += "}\n";
    html += "function updateCardState(cardId, stateData, metricName) {";
    html += "  const card = document.getElementById(cardId);";
    html += "  if (!card || !stateData) return;";
    html += "  card.className = 'sensor-card';";
    html += "  card.removeAttribute('title');";
    html += "  const stateCode = stateData.state_code || 0;";
    html += "  if (stateCode === 3) {";
    html += "    card.className = 'sensor-card state-critical';";
    html += "    card.title = metricName + ': CRITICAL - ' + stateData.state;";
    html += "  } else if (stateCode === 2) {";
    html += "    card.className = 'sensor-card state-warning';";
    html += "    card.title = metricName + ': WARNING - ' + stateData.state;";
    html += "  } else if (stateCode === 1) {";
    html += "    card.className = 'sensor-card state-normal';";
    html += "  }";
    html += "}";
    html += "</script>";
    html += "</head>";
    html += "<body>";

    html += "<div class='header'>";
    html += "<h1>🐠 " + getUnitName() + " Monitor</h1>";
    html += "<div class='nav'>";
    html += "<a href='/charts'>Charts</a>";
    html += "<button class='theme-toggle' onclick='window.location.href=\"/calibration\"' title='Calibration'>⚙️</button>";
    html += "</div></div>";

    html += "<div class='status-bar'>";
    html += "<div class='status-item'><div class='status-dot' id='statusDot'></div><span id='statusText'>Connected</span></div>";
    html += "<div class='status-item'><span>WiFi: <strong>" + wifiManager->getSSID() + "</strong></span></div>";
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
    html += "<div class='sensor-card' id='tempCard' style='--card-color: var(--temp-color)'>";
    html += "<div class='sensor-label'>Temperature</div>";
    html += "<div class='sensor-value'><span id='temp'>";
    html += dataValid ? String(temp_c, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>°Celsius</div>";
    html += "</div>";

    // ORP
    html += "<div class='sensor-card' id='orpCard' style='--card-color: var(--orp-color)'>";
    html += "<div class='sensor-label'>ORP</div>";
    html += "<div class='sensor-value'><span id='orp'>";
    html += dataValid ? String(orp_mv, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>Millivolts</div>";
    html += "</div>";

    // pH
    html += "<div class='sensor-card' id='phCard' style='--card-color: var(--ph-color)'>";
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
    html += "<div class='sensor-card' id='ecCard' style='--card-color: var(--ec-color)'>";
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

    // Derived Metrics Section
    html += "<div class='section-title'>Derived Water Quality Metrics</div>";
    html += "<div class='sensor-grid'>";

    // TDS
    html += "<div class='sensor-card' style='--card-color: var(--tds-color)'>";
    html += "<div class='sensor-label'>TDS (Total Dissolved Solids)</div>";
    html += "<div class='sensor-value'><span id='tds'>";
    html += dataValid ? String(tds_ppm, 1) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>ppm</div>";
    html += "</div>";

    // CO2 with color coding
    html += "<div class='sensor-card' id='co2Card' style='--card-color: var(--co2-color)'>";
    html += "<div class='sensor-label'>Dissolved CO2</div>";
    html += "<div class='sensor-value'><span id='co2'>";
    html += dataValid ? String(co2_ppm, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>ppm</div>";
    html += "<div class='sensor-status' style='font-size:0.7em;color:var(--text-tertiary)'>15-30 ppm optimal</div>";
    html += "</div>";

    // Toxic Ammonia Ratio
    html += "<div class='sensor-card' id='nh3Card' style='--card-color: var(--nh3-color)'>";
    html += "<div class='sensor-label'>Toxic NH₃ %</div>";
    html += "<div class='sensor-value'><span id='nh3_ratio'>";
    html += dataValid ? String(toxic_ammonia_ratio * 100.0, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>%</div>";
    String nh3AlertDisplay = (dataValid && nh3_ppm > 0.05) ? "" : "style='display:none'";
    html += "<div id='nh3Alert' class='alert-badge alert-danger' " + nh3AlertDisplay + ">⚠ NH₃ > 0.05 ppm</div>";
    html += "<div style='font-size:0.7em;color:var(--text-tertiary);margin-top:5px'>NH₃ ppm: <span id='nh3_ppm'>";
    html += dataValid ? String(nh3_ppm, 4) : "--";
    html += "</span></div>";

    // Show TAN requirement note if TAN is not set
    float current_tan = 0.0;
    if (tankSettingsManager != nullptr) {
        current_tan = tankSettingsManager->getSettings().manual_tan_ppm;
    }
    if (current_tan <= 0.0) {
        html += "<div id='nh3_tan_note' class='sensor-status' style='font-size:0.7em;color:var(--text-tertiary)'>⚠ Set TAN in settings for actual NH₃ ppm</div>";
    } else {
        html += "<div id='nh3_tan_note' class='sensor-status' style='font-size:0.7em;color:var(--text-tertiary)'>Fraction of TAN as toxic NH₃</div>";
    }
    html += "</div>";

    // Maximum Dissolved Oxygen
    html += "<div class='sensor-card' id='doCard' style='--card-color: var(--do-color)'>";
    html += "<div class='sensor-label'>Max O2 Saturation</div>";
    html += "<div class='sensor-value'><span id='max_do'>";
    html += dataValid ? String(max_do_mg_l, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>mg/L</div>";
    html += "<div class='sensor-status' style='font-size:0.7em;color:var(--text-tertiary)'>At current temp</div>";
    html += "</div>";

    // Stocking Density with color coding
    html += "<div class='sensor-card' id='stockCard' style='--card-color: var(--stock-color)'>";
    html += "<div class='sensor-label'>Stocking Density</div>";
    html += "<div class='sensor-value'><span id='stock'>";
    html += dataValid ? String(stocking_density, 2) : "--";
    html += "</span></div>";
    html += "<div class='sensor-unit'>cm/L</div>";
    html += "<div class='sensor-status' style='font-size:0.7em;color:var(--text-tertiary)'>&lt;1 light, 1-2 moderate, &gt;2 heavy</div>";
    html += "</div>";

    html += "</div>";

    html += "<div class='info-footer'>Auto-refresh every 2 seconds | Real-time monitoring active<br>&copy; Scott McLelslie to my beloved wife Kate 2026. Happy new year</div>";

    html += "<script>\n";
    html += "initTheme();\n";
    html += "updateData();\n";
    html += "updateDerivedMetrics();\n";
    html += "updateMqttStatus();\n";
    html += "updateWarningStates();\n";
    html += "ConnectionState.dataIntervalId = setInterval(updateData, 2000);\n";
    html += "ConnectionState.metricsIntervalId = setInterval(updateDerivedMetrics, 2000);\n";
    html += "ConnectionState.mqttIntervalId = setInterval(updateMqttStatus, 5000);\n";
    html += "ConnectionState.warningsIntervalId = setInterval(updateWarningStates, 2000);\n";
    html += "</script>";
    html += "</body>";
    html += "</html>";

    return html;
}

String AquariumWebServer::generateProvisioningPage() {
    String html;
    html.reserve(8000);  // Pre-allocate memory
    html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
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
    html += "</head>";
    html += "<body>";
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

    html += "</body>";
    html += "</html>";

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
    // ArduinoJson v7 automatically manages memory for large documents
    // Handles up to 288 data points with 11 fields each
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
            // Primary sensors - direct assignment for reliable serialization
            point["temp"] = history[idx].temp_c;
            point["orp"] = history[idx].orp_mv;
            point["ph"] = history[idx].ph;
            point["ec"] = history[idx].ec_ms_cm;
            // Derived metrics
            point["tds"] = history[idx].tds_ppm;
            point["co2"] = history[idx].co2_ppm;
            point["nh3_fraction"] = history[idx].toxic_ammonia_ratio;  // Fraction (0-1), UI multiplies by 100
            point["nh3_ppm"] = history[idx].nh3_ppm;
            point["max_do"] = history[idx].max_do_mg_l;
            point["stocking"] = history[idx].stocking_density;  // Fixed: matches client-side field name
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
        .nav a, .nav button, .theme-toggle {
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
        .nav a:hover, .nav button:hover, .theme-toggle:hover {
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
        /* Tab Navigation Styles */
        .tabs {
            display: flex;
            gap: 10px;
            margin-bottom: 20px;
            border-bottom: 2px solid var(--border-color);
            background: var(--bg-card);
            padding: 10px;
            border-radius: 10px 10px 0 0;
        }
        .tab-button {
            padding: 12px 24px;
            background: transparent;
            border: none;
            border-bottom: 3px solid transparent;
            color: var(--text-secondary);
            cursor: pointer;
            font-size: 1em;
            font-weight: 600;
            transition: all 0.3s ease;
        }
        .tab-button:hover {
            color: var(--color-primary);
            background: var(--bg-primary);
            border-radius: 8px 8px 0 0;
        }
        .tab-button.active {
            color: var(--color-primary);
            border-bottom-color: var(--color-primary);
        }
        .tab-content {
            display: none;
        }
        .tab-content.active {
            display: block;
        }
        /* About Modal Styles */
        .modal-backdrop {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.7);
            z-index: 1000;
            justify-content: center;
            align-items: center;
            padding: 20px;
            overflow-y: auto;
        }
        .modal-backdrop:not(.hidden) {
            display: flex;
        }
        .modal-container {
            background: var(--bg-card);
            border-radius: 15px;
            max-width: 700px;
            width: 100%;
            max-height: 90vh;
            overflow-y: auto;
            position: relative;
            border: 1px solid var(--border-color);
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
        }
        .modal-header {
            padding: 25px 25px 20px;
            border-bottom: 1px solid var(--border-color);
            position: sticky;
            top: 0;
            background: var(--bg-card);
            z-index: 10;
        }
        .modal-title {
            font-size: 1.8em;
            background: linear-gradient(135deg, var(--color-primary), var(--color-secondary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            font-weight: 700;
            margin: 0;
        }
        .modal-close {
            position: absolute;
            top: 20px;
            right: 20px;
            background: transparent;
            border: none;
            font-size: 1.8em;
            cursor: pointer;
            color: var(--text-secondary);
            width: 35px;
            height: 35px;
            display: flex;
            align-items: center;
            justify-content: center;
            border-radius: 8px;
            transition: all 0.3s ease;
        }
        .modal-close:hover {
            background: var(--color-danger);
            color: white;
            transform: rotate(90deg);
        }
        .modal-content {
            padding: 25px;
        }
        .modal-section {
            margin-bottom: 25px;
        }
        .modal-section h3 {
            color: var(--color-primary);
            font-size: 1.2em;
            margin-bottom: 10px;
        }
        .modal-section p, .modal-section ul, .modal-section ol {
            color: var(--text-primary);
            line-height: 1.6;
            margin: 10px 0;
        }
        .modal-section ul, .modal-section ol {
            padding-left: 25px;
        }
        .modal-section li {
            margin: 8px 0;
        }
        .modal-section a {
            color: var(--color-primary);
            text-decoration: none;
            border-bottom: 1px solid transparent;
            transition: all 0.2s ease;
        }
        .modal-section a:hover {
            border-bottom-color: var(--color-primary);
        }
        .modal-section .critical {
            color: var(--color-danger);
            font-weight: bold;
        }
        .modal-footer {
            padding: 20px 25px;
            border-top: 1px solid var(--border-color);
            text-align: center;
            color: var(--text-secondary);
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class='header'>
        <h1>🔬 Configuration & Calibration</h1>
        <div class='nav'>
            <a href='/'>Back</a>
            <button onclick='showAboutModal()' title='About'>?</button>
            <button onclick='exportCSV()' title='Export data as CSV'>CSV</button>
            <button onclick='exportJSON()' title='Export data as JSON'>JSON</button>
        </div>
    </div>

    <div id='messages'></div>

    <!-- About Modal -->
    <div id='aboutModal' class='modal-backdrop hidden' onclick='if(event.target === this) closeAboutModal()'>
        <div class='modal-container'>
            <div class='modal-header'>
                <h2 class='modal-title'>About Fish Tank Controller</h2>
                <button class='modal-close' onclick='closeAboutModal()' title='Close'>×</button>
            </div>
            <div class='modal-content'>
                <div class='modal-section'>
                    <h3>About</h3>
                    <p>ESP32-based wireless aquarium controller for freshwater/saltwater tanks. Monitors pH, ORP, EC, temperature using the Sentron POET sensor. Features real-time telemetry, data export, MQTT integration with Home Assistant, and web-based calibration.</p>
                </div>

                <div class='modal-section'>
                    <h3>Quickstart</h3>
                    <ol>
                        <li>Flash firmware to ESP32-C3/S3</li>
                        <li>Connect to "AquariumSetup" WiFi AP</li>
                        <li>Configure WiFi credentials</li>
                        <li>Access <a href='http://aquarium.local' target='_blank' rel='noopener noreferrer'>http://aquarium.local</a></li>
                        <li>Calibrate pH and EC sensors (Settings → Calibration)</li>
                    </ol>
                </div>

                <div class='modal-section'>
                    <h3>Operations Manual</h3>
                    <ul>
                        <li>Dashboard shows real-time sensor readings and derived metrics</li>
                        <li>Charts page displays historical trends (24-hour history)</li>
                        <li>Calibration page handles pH (1-point/2-point) and EC calibration</li>
                        <li>MQTT configuration enables Home Assistant integration</li>
                        <li>Data export available in CSV/JSON formats</li>
                        <li>Theme toggle for dark/light modes</li>
                        <li class='critical'>CRITICAL: Always calibrate sensors before relying on readings</li>
                        <li class='critical'>CRITICAL: This device manages life-support equipment - monitor regularly</li>
                    </ul>
                </div>

                <div class='modal-section'>
                    <h3>Licensing</h3>
                    <p>This project is licensed under the <strong>Apache License 2.0</strong>. You are free to use, modify, distribute, and use commercially. Attribution is required. The FishTankController name and branding are protected trademarks. See LICENSE, TRADEMARK.md, and COMMERCIAL.md in the repository for full details.</p>
                </div>

                <div class='modal-section'>
                    <h3>Copyright & Project</h3>
                    <p>© 2026 <a href='https://www.mcleslie.com/' target='_blank' rel='noopener noreferrer'>Scott McLelslie</a></p>
                    <p>Project: <a href='https://github.com/scottmclesly/fishtankcontroller' target='_blank' rel='noopener noreferrer'>fishtankcontroller on GitHub</a></p>
                </div>

                <div class='modal-section'>
                    <p style='text-align: center; font-style: italic;'>Dedicated with love to <a href='https://www.katrinbarshe.com/' target='_blank' rel='noopener noreferrer'>Katrin Barshe</a></p>
                </div>
            </div>
        </div>
    </div>

    <!-- Tab Navigation -->
    <div class='tabs'>
        <button class='tab-button active' onclick='switchTab("calibration")'>🔬 Sensor Calibration</button>
        <button class='tab-button' onclick='switchTab("tank")'>🐠 Tank Settings</button>
        <button class='tab-button' onclick='switchTab("mqtt")'>📡 MQTT Configuration</button>
        <button class='tab-button' onclick='switchTab("warnings")'>⚠️ Warning Thresholds</button>
    </div>

    <!-- Calibration Tab Content -->
    <div id='calibration-tab' class='tab-content active'>

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

    <!-- Theme Configuration Card -->
    <div class='card'>
        <h2>Theme Settings</h2>
        <div class='info'>
            <strong>Choose your preferred theme:</strong><br>
            Select between light and dark mode for all pages.
        </div>

        <div class='form-group'>
            <label>Theme:</label>
            <div style='display: flex; gap: 10px; margin-top: 10px;'>
                <button onclick='setTheme("light")' style='flex: 1;'>☀️ Light Mode</button>
                <button onclick='setTheme("dark")' style='flex: 1;'>🌙 Dark Mode</button>
            </div>
            <div id='currentTheme' style='margin-top: 10px; font-size: 0.9em; color: var(--text-secondary);'></div>
        </div>
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

    </div> <!-- End Calibration Tab -->

    <!-- Tank Settings Tab Content -->
    <div id='tank-tab' class='tab-content'>

    <!-- Tank Configuration Card -->
    <div class='card'>
        <h2>Tank Configuration</h2>
        <div class='info'>
            <strong>Configure your aquarium:</strong><br>
            Set tank dimensions to calculate volume and track stocking density.
        </div>

        <div class='form-group'>
            <label>Tank Shape:</label>
            <select id='tank_shape' onchange='updateDimensionInputs()'>
                <option value='0'>Rectangle</option>
                <option value='1'>Cube</option>
                <option value='2'>Cylinder</option>
                <option value='3'>Custom (Manual Volume)</option>
            </select>
        </div>

        <div id='rectangle_inputs'>
            <div class='form-group'>
                <label>Length (cm):</label>
                <input type='number' step='0.1' id='tank_length' placeholder='e.g., 100' value='0'>
            </div>
            <div class='form-group'>
                <label>Width (cm):</label>
                <input type='number' step='0.1' id='tank_width' placeholder='e.g., 50' value='0'>
            </div>
            <div class='form-group'>
                <label>Height (cm):</label>
                <input type='number' step='0.1' id='tank_height' placeholder='e.g., 60' value='0'>
            </div>
        </div>

        <div id='cylinder_inputs' style='display:none;'>
            <div class='form-group'>
                <label>Radius (cm):</label>
                <input type='number' step='0.1' id='tank_radius' placeholder='e.g., 25' value='0'>
            </div>
            <div class='form-group'>
                <label>Height (cm):</label>
                <input type='number' step='0.1' id='tank_height_cyl' placeholder='e.g., 60' value='0'>
            </div>
        </div>

        <div id='cube_inputs' style='display:none;'>
            <div class='form-group'>
                <label>Side Length (cm):</label>
                <input type='number' step='0.1' id='tank_cube_side' placeholder='e.g., 50' value='0'>
            </div>
        </div>

        <div id='custom_inputs' style='display:none;'>
            <div class='form-group'>
                <label>Manual Volume (Liters):</label>
                <input type='number' step='0.1' id='tank_manual_volume' placeholder='e.g., 300' value='0'>
            </div>
        </div>

        <button onclick='calculateVolume()'>Calculate Volume</button>
        <div id='volume_display' style='margin-top: 15px; padding: 10px; background: var(--info-bg); color: var(--info-text); border-radius: 5px; display: none;'>
            <strong>Calculated Volume:</strong> <span id='calculated_volume'>0</span> Liters
        </div>

        <button onclick='saveTankSettings()' style='margin-top: 15px;'>Save Tank Settings</button>
    </div>

    <!-- Water Parameters Card -->
    <div class='card'>
        <h2>Water Parameters</h2>
        <div class='info'>
            <strong>Set water chemistry parameters:</strong><br>
            These values are used to calculate derived metrics like CO2 and toxic ammonia.
        </div>

        <div class='form-group'>
            <label>Carbonate Hardness (KH) in dKH:</label>
            <input type='number' step='0.1' id='tank_kh' placeholder='e.g., 4.0' value='4.0'>
            <small>Used for CO2 calculation. Default: 4.0 dKH</small>
        </div>

        <div class='form-group'>
            <label>Total Ammonia Nitrogen (TAN) in ppm:</label>
            <input type='number' step='0.01' id='tank_tan' placeholder='e.g., 0.0' value='0.0'>
            <small>Used for toxic NH3 calculation. Default: 0.0 ppm</small>
        </div>

        <div class='form-group'>
            <label>TDS Conversion Factor:</label>
            <input type='number' step='0.01' id='tank_tds_factor' placeholder='e.g., 0.64' value='0.64'>
            <small>Typical: 0.5-0.7. Default: 0.64 for freshwater</small>
        </div>

        <button onclick='saveWaterParams()'>Save Water Parameters</button>
    </div>

    <!-- Fish Profile Card -->
    <div class='card'>
        <h2>Fish Profile (Stocking Calculator)</h2>
        <div class='info'>
            <strong>Track your fish population:</strong><br>
            Add fish to calculate stocking density. Rule of thumb: 1 cm fish per 1-2 liters.
        </div>

        <h3>Add Fish</h3>
        <div class='form-group'>
            <label>Species Name:</label>
            <input type='text' id='fish_species' placeholder='e.g., Neon Tetra' maxlength='31'>
        </div>
        <div class='form-group'>
            <label>Count:</label>
            <input type='number' id='fish_count' placeholder='e.g., 10' min='1' value='1'>
        </div>
        <div class='form-group'>
            <label>Average Length (cm):</label>
            <input type='number' step='0.1' id='fish_length' placeholder='e.g., 4.0'>
        </div>
        <button onclick='addFish()'>Add Fish</button>

        <h3>Current Fish List</h3>
        <div id='fish_list' style='margin-top: 10px;'>
            <div style='color: var(--text-secondary);'>No fish added yet</div>
        </div>
        <div id='total_stocking' style='margin-top: 15px; padding: 10px; background: var(--info-bg); color: var(--info-text); border-radius: 5px; display: none;'>
            <strong>Total Stocking Length:</strong> <span id='stocking_length'>0</span> cm
        </div>

        <button onclick='clearAllFish()' class='danger' style='margin-top: 15px;'>Clear All Fish</button>
    </div>

    </div> <!-- End Tank Settings Tab -->

    <!-- MQTT Configuration Tab Content -->
    <div id='mqtt-tab' class='tab-content'>

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
            updateThemeDisplay();
        }

        function setTheme(theme) {
            document.documentElement.setAttribute('data-theme', theme);
            localStorage.setItem('theme', theme);
            updateThemeDisplay();
            showMessage('Theme changed to ' + theme + ' mode', 'success');
        }

        function updateThemeDisplay() {
            const theme = document.documentElement.getAttribute('data-theme') || 'dark';
            const display = document.getElementById('currentTheme');
            if (display) {
                display.textContent = 'Current theme: ' + (theme === 'light' ? '☀️ Light Mode' : '🌙 Dark Mode');
            }
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

        async function exportCSV() {
            try {
                const response = await fetch('/api/export/csv');
                const blob = await response.blob();
                const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
                const filename = `aquarium-data-${timestamp}.csv`;
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
                showMessage('CSV export successful', 'success');
            } catch (error) {
                console.error('CSV export failed:', error);
                showMessage('Failed to export CSV. Please try again.', 'error');
            }
        }

        async function exportJSON() {
            try {
                const response = await fetch('/api/export/json');
                const blob = await response.blob();
                const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
                const filename = `aquarium-data-${timestamp}.json`;
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
                showMessage('JSON export successful', 'success');
            } catch (error) {
                console.error('JSON export failed:', error);
                showMessage('Failed to export JSON. Please try again.', 'error');
            }
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

    </div> <!-- End MQTT Tab -->

    <!-- Warning Thresholds Tab Content -->
    <div id='warnings-tab' class='tab-content'>

    <!-- Warning Profile Card -->
    <div class='card'>
        <h2>Warning Thresholds Configuration</h2>
        <div id='warningStatus' class='status'>Loading...</div>

        <div class='info'>
            <strong>Species-Aware Safety Monitoring:</strong><br>
            Set warning and critical thresholds for all water parameters. The system will automatically alert you when values approach or exceed safe ranges for your tank type.
        </div>

        <div class='form-group'>
            <label>Tank Type Profile:</label>
            <select id='tank_type' onchange='loadWarningProfile()'>
                <option value='0'>Freshwater Community</option>
                <option value='1'>Freshwater Planted</option>
                <option value='2'>Saltwater Fish-Only</option>
                <option value='3'>Reef</option>
                <option value='4'>Custom</option>
            </select>
            <small>Presets include species-appropriate threshold defaults</small>
        </div>

        <button onclick='saveWarningProfile()' class='primary'>Save Tank Type</button>

        <div class='info' style='margin-top: 20px; background: var(--bg-status);'>
            <strong>Warning States:</strong><br>
            • <span style='color: #10b981;'>● NORMAL</span> - Parameter within safe range<br>
            • <span style='color: #f59e0b;'>● WARNING</span> - Approaching unsafe levels (yellow pulse on dashboard)<br>
            • <span style='color: #ef4444;'>● CRITICAL</span> - Dangerous levels requiring immediate action (red pulse)<br>
        </div>
    </div>

    <!-- Current Thresholds Display -->
    <div class='card'>
        <h2>Current Threshold Values</h2>
        <div id='thresholdDisplay'>
            <p style='color: var(--text-secondary);'>Select a tank type above to view thresholds...</p>
        </div>
    </div>

    <script>
        // Load warning profile on page load
        function loadWarningProfile() {
            fetch('/api/warnings/profile')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('tank_type').value = data.tank_type_code;

                    const statusDiv = document.getElementById('warningStatus');
                    statusDiv.className = 'status calibrated';
                    statusDiv.textContent = '✓ Active Profile: ' + data.tank_type;

                    // Display thresholds
                    const thresholdDiv = document.getElementById('thresholdDisplay');
                    thresholdDiv.innerHTML = `
                        <h3>Temperature</h3>
                        <p>⚠ Warning: ${data.temperature.warn_low}°C - ${data.temperature.warn_high}°C</p>
                        <p>🔴 Critical: ${data.temperature.crit_low}°C - ${data.temperature.crit_high}°C</p>

                        <h3 style='margin-top: 15px;'>pH</h3>
                        <p>⚠ Warning: ${data.ph.warn_low} - ${data.ph.warn_high}</p>
                        <p>🔴 Critical: ${data.ph.crit_low} - ${data.ph.crit_high}</p>
                        <p>Rate limits: ${data.ph.delta_warn_per_24h}/day (warn), ${data.ph.delta_crit_per_24h}/day (crit)</p>

                        <h3 style='margin-top: 15px;'>Toxic Ammonia (NH₃)</h3>
                        <p>⚠ Warning: > ${data.nh3.warn_high} ppm</p>
                        <p>🔴 Critical: > ${data.nh3.crit_high} ppm</p>

                        <h3 style='margin-top: 15px;'>ORP</h3>
                        <p>⚠ Warning: ${data.orp.warn_low}mV - ${data.orp.warn_high}mV</p>
                        <p>🔴 Critical: ${data.orp.crit_low}mV - ${data.orp.crit_high}mV</p>

                        <h3 style='margin-top: 15px;'>Conductivity</h3>
                        <p>⚠ Warning: ${data.conductivity.warn_low_us_cm}µS/cm - ${data.conductivity.warn_high_us_cm}µS/cm</p>
                        <p>🔴 Critical: ${data.conductivity.crit_low_us_cm}µS/cm - ${data.conductivity.crit_high_us_cm}µS/cm</p>

                        <h3 style='margin-top: 15px;'>Dissolved Oxygen</h3>
                        <p>⚠ Warning: < ${data.dissolved_oxygen.warn_low} mg/L</p>
                        <p>🔴 Critical: < ${data.dissolved_oxygen.crit_low} mg/L</p>
                    `;
                })
                .catch(err => {
                    document.getElementById('warningStatus').textContent = 'Error loading profile';
                    console.error(err);
                });
        }

        function saveWarningProfile() {
            const tankType = document.getElementById('tank_type').value;

            const params = new URLSearchParams();
            params.append('tank_type', tankType);

            fetch('/api/warnings/profile', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message, 'success');
                        loadWarningProfile();
                    } else {
                        showMessage(data.error, 'error');
                    }
                })
                .catch(err => {
                    showMessage('Failed to save profile: ' + err, 'error');
                });
        }

        // Auto-load on tab switch
        if (document.getElementById('warnings-tab').classList.contains('active')) {
            loadWarningProfile();
        }
    </script>

    </div> <!-- End Warnings Tab -->

    <div style='text-align: center; padding: 20px; color: var(--text-secondary); font-size: 0.85em;'>
        &copy; Scott McLelslie to my beloved wife Kate 2026. Happy new year
    </div>

    <script>
        // Tab switching function
        function switchTab(tabName) {
            // Hide all tab contents
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.classList.remove('active');
            });
            // Remove active class from all buttons
            document.querySelectorAll('.tab-button').forEach(btn => {
                btn.classList.remove('active');
            });

            // Show selected tab
            document.getElementById(tabName + '-tab').classList.add('active');
            // Activate button
            event.target.classList.add('active');

            // Load tank settings when switching to tank tab
            if (tabName === 'tank') {
                loadTankSettings();
                loadFishList();
            }
            // Load warning profile when switching to warnings tab
            if (tabName === 'warnings') {
                loadWarningProfile();
            }
        }

        // Update dimension inputs based on tank shape
        function updateDimensionInputs() {
            const shape = parseInt(document.getElementById('tank_shape').value);
            document.getElementById('rectangle_inputs').style.display = (shape === 0) ? 'block' : 'none';
            document.getElementById('cube_inputs').style.display = (shape === 1) ? 'block' : 'none';
            document.getElementById('cylinder_inputs').style.display = (shape === 2) ? 'block' : 'none';
            document.getElementById('custom_inputs').style.display = (shape === 3) ? 'block' : 'none';
        }

        // Calculate tank volume
        function calculateVolume() {
            const shape = parseInt(document.getElementById('tank_shape').value);
            let volume = 0;

            if (shape === 0) { // Rectangle
                const length = parseFloat(document.getElementById('tank_length').value) || 0;
                const width = parseFloat(document.getElementById('tank_width').value) || 0;
                const height = parseFloat(document.getElementById('tank_height').value) || 0;
                volume = (length * width * height) / 1000.0; // cm³ to liters
            } else if (shape === 1) { // Cube
                const side = parseFloat(document.getElementById('tank_cube_side').value) || 0;
                volume = (side * side * side) / 1000.0;
            } else if (shape === 2) { // Cylinder
                const radius = parseFloat(document.getElementById('tank_radius').value) || 0;
                const height = parseFloat(document.getElementById('tank_height_cyl').value) || 0;
                volume = (Math.PI * radius * radius * height) / 1000.0;
            } else if (shape === 3) { // Custom
                volume = parseFloat(document.getElementById('tank_manual_volume').value) || 0;
            }

            document.getElementById('calculated_volume').textContent = volume.toFixed(2);
            document.getElementById('volume_display').style.display = 'block';
        }

        // Save tank settings
        function saveTankSettings() {
            const shape = document.getElementById('tank_shape').value;
            const length = parseFloat(document.getElementById('tank_length').value) || 0;
            const width = parseFloat(document.getElementById('tank_width').value) || 0;
            const height = parseFloat(document.getElementById('tank_height').value) || 0;
            const radius = parseFloat(document.getElementById('tank_radius').value) || 0;
            const manual_volume = parseFloat(document.getElementById('tank_manual_volume').value) || 0;

            const params = new URLSearchParams();
            params.append('tank_shape', shape);
            params.append('length', length);
            params.append('width', width);
            params.append('height', height);
            params.append('radius', radius);
            params.append('manual_volume', manual_volume);

            fetch('/api/settings/tank', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message + ' (Volume: ' + data.calculated_volume.toFixed(2) + ' L)', 'success');
                    } else {
                        showMessage(data.error || 'Failed to save tank settings', 'error');
                    }
                })
                .catch(err => showMessage('Error saving tank settings', 'error'));
        }

        // Save water parameters
        function saveWaterParams() {
            const kh = parseFloat(document.getElementById('tank_kh').value) || 4.0;
            const tan = parseFloat(document.getElementById('tank_tan').value) || 0.0;
            const tds_factor = parseFloat(document.getElementById('tank_tds_factor').value) || 0.64;

            const params = new URLSearchParams();
            params.append('kh', kh);
            params.append('tan', tan);
            params.append('tds_factor', tds_factor);

            fetch('/api/settings/tank', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message, 'success');
                    } else {
                        showMessage(data.error || 'Failed to save water parameters', 'error');
                    }
                })
                .catch(err => showMessage('Error saving water parameters', 'error'));
        }

        // Load tank settings
        function loadTankSettings() {
            fetch('/api/settings/tank')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('tank_shape').value = data.tank_shape || 0;
                    document.getElementById('tank_length').value = data.dimensions.length_cm || 0;
                    document.getElementById('tank_width').value = data.dimensions.width_cm || 0;
                    document.getElementById('tank_height').value = data.dimensions.height_cm || 0;
                    document.getElementById('tank_radius').value = data.dimensions.radius_cm || 0;
                    document.getElementById('tank_cube_side').value = data.dimensions.length_cm || 0;
                    document.getElementById('tank_height_cyl').value = data.dimensions.height_cm || 0;
                    document.getElementById('tank_manual_volume').value = data.manual_volume_liters || 0;
                    document.getElementById('tank_kh').value = data.manual_kh_dkh || 4.0;
                    document.getElementById('tank_tan').value = data.manual_tan_ppm || 0.0;
                    document.getElementById('tank_tds_factor').value = data.tds_conversion_factor || 0.64;
                    updateDimensionInputs();
                    if (data.calculated_volume_liters > 0) {
                        document.getElementById('calculated_volume').textContent = data.calculated_volume_liters.toFixed(2);
                        document.getElementById('volume_display').style.display = 'block';
                    }
                })
                .catch(err => console.error('Error loading tank settings:', err));
        }

        // Add fish
        function addFish() {
            const species = document.getElementById('fish_species').value.trim();
            const count = parseInt(document.getElementById('fish_count').value) || 1;
            const length = parseFloat(document.getElementById('fish_length').value) || 0;

            if (!species || length <= 0) {
                showMessage('Please enter species name and length', 'error');
                return;
            }

            const params = new URLSearchParams();
            params.append('species', species);
            params.append('count', count);
            params.append('avg_length', length);

            fetch('/api/settings/fish/add', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message, 'success');
                        document.getElementById('fish_species').value = '';
                        document.getElementById('fish_length').value = '';
                        loadFishList();
                    } else {
                        showMessage(data.error || 'Failed to add fish', 'error');
                    }
                })
                .catch(err => showMessage('Error adding fish', 'error'));
        }

        // Load fish list
        function loadFishList() {
            fetch('/api/settings/fish')
                .then(r => r.json())
                .then(data => {
                    const listDiv = document.getElementById('fish_list');
                    if (data.fish && data.fish.length > 0) {
                        let html = '<table style=\"width:100%; border-collapse: collapse;\">';
                        html += '<tr style=\"border-bottom: 1px solid var(--border-color); font-weight: bold;\">';
                        html += '<td>Species</td><td>Count</td><td>Avg Length</td><td>Action</td></tr>';
                        data.fish.forEach((fish, idx) => {
                            html += '<tr style=\"border-bottom: 1px solid var(--border-color); padding: 5px 0;\">';
                            html += '<td>' + fish.species + '</td>';
                            html += '<td>' + fish.count + '</td>';
                            html += '<td>' + fish.avg_length_cm.toFixed(1) + ' cm</td>';
                            html += '<td><button class=\"danger\" onclick=\"removeFish(' + idx + ')\" style=\"padding: 4px 8px; font-size: 0.85em;\">Remove</button></td>';
                            html += '</tr>';
                        });
                        html += '</table>';
                        listDiv.innerHTML = html;

                        document.getElementById('stocking_length').textContent = data.total_stocking_length.toFixed(1);
                        document.getElementById('total_stocking').style.display = 'block';
                    } else {
                        listDiv.innerHTML = '<div style=\"color: var(--text-secondary);\">No fish added yet</div>';
                        document.getElementById('total_stocking').style.display = 'none';
                    }
                })
                .catch(err => console.error('Error loading fish list:', err));
        }

        // Remove fish
        function removeFish(index) {
            const params = new URLSearchParams();
            params.append('index', index);

            fetch('/api/settings/fish/remove', { method: 'POST', body: params })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message, 'success');
                        loadFishList();
                    } else {
                        showMessage(data.error || 'Failed to remove fish', 'error');
                    }
                })
                .catch(err => showMessage('Error removing fish', 'error'));
        }

        // Clear all fish
        function clearAllFish() {
            if (!confirm('Are you sure you want to clear all fish?')) return;

            fetch('/api/settings/fish/clear', { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message, 'success');
                        loadFishList();
                    } else {
                        showMessage(data.error || 'Failed to clear fish', 'error');
                    }
                })
                .catch(err => showMessage('Error clearing fish', 'error'));
        }

        // About Modal Functions
        function showAboutModal() {
            const modal = document.getElementById('aboutModal');
            modal.classList.remove('hidden');
            document.body.style.overflow = 'hidden'; // Prevent background scrolling
        }

        function closeAboutModal() {
            const modal = document.getElementById('aboutModal');
            modal.classList.add('hidden');
            document.body.style.overflow = ''; // Restore scrolling
        }

        // ESC key to close modal
        document.addEventListener('keydown', function(e) {
            if (e.key === 'Escape') {
                const modal = document.getElementById('aboutModal');
                if (!modal.classList.contains('hidden')) {
                    closeAboutModal();
                }
            }
        });
    </script>
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
    csv += "Timestamp,Unix_Time,Temperature_C,ORP_mV,pH,EC_mS_cm,TDS_ppm,CO2_ppm,NH3_Ratio_%,NH3_ppm,Max_DO_mg_L,Stocking_cm_L,Temp_State,pH_State,NH3_State,ORP_State,EC_State,DO_State,Valid\r\n";

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

            // Derived metrics
            csv += String(history[idx].tds_ppm, 1);
            csv += ",";
            csv += String(history[idx].co2_ppm, 2);
            csv += ",";
            csv += String(history[idx].toxic_ammonia_ratio * 100.0, 2);
            csv += ",";
            csv += String(history[idx].nh3_ppm, 4);
            csv += ",";
            csv += String(history[idx].max_do_mg_l, 2);
            csv += ",";
            csv += String(history[idx].stocking_density, 2);
            csv += ",";

            // Warning states
            csv += String(history[idx].temp_state);
            csv += ",";
            csv += String(history[idx].ph_state);
            csv += ",";
            csv += String(history[idx].nh3_state);
            csv += ",";
            csv += String(history[idx].orp_state);
            csv += ",";
            csv += String(history[idx].ec_state);
            csv += ",";
            csv += String(history[idx].do_state);
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
            // Primary sensors
            point["temp_c"] = serialized(String(history[idx].temp_c, 2));
            point["orp_mv"] = serialized(String(history[idx].orp_mv, 2));
            point["ph"] = serialized(String(history[idx].ph, 2));
            point["ec_ms_cm"] = serialized(String(history[idx].ec_ms_cm, 3));
            // Derived metrics
            point["tds_ppm"] = serialized(String(history[idx].tds_ppm, 1));
            point["co2_ppm"] = serialized(String(history[idx].co2_ppm, 2));
            point["nh3_ratio_pct"] = serialized(String(history[idx].toxic_ammonia_ratio * 100.0, 2));
            point["nh3_ppm"] = serialized(String(history[idx].nh3_ppm, 4));
            point["max_do_mg_l"] = serialized(String(history[idx].max_do_mg_l, 2));
            point["stocking_density"] = serialized(String(history[idx].stocking_density, 2));
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

// Derived metrics API handler
//
// API CONTRACT: All metrics returned as raw values (fractions, not percentages)
// - nh3_fraction: fraction (0.0-1.0) of TAN that exists as unionized NH₃
// - UI layer multiplies by 100 for percentage display
// - DO NOT multiply by 100 in this API - prevents double-multiplication bugs
void AquariumWebServer::handleGetDerivedMetrics(AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["tds_ppm"] = serialized(String(tds_ppm, 2));
    doc["co2_ppm"] = serialized(String(co2_ppm, 2));

    // NH3 fraction: MUST be 0.0-1.0 (fraction, not percentage)
    #ifdef DEBUG
    if (toxic_ammonia_ratio > 1.01 || toxic_ammonia_ratio < -0.01) {
        Serial.print("ERROR: NH3 fraction out of range: ");
        Serial.println(toxic_ammonia_ratio);
    }
    #endif
    doc["nh3_fraction"] = serialized(String(toxic_ammonia_ratio, 4));  // Fraction (0-1)
    doc["nh3_ppm"] = serialized(String(nh3_ppm, 4));
    doc["max_do_mg_l"] = serialized(String(max_do_mg_l, 2));
    doc["stocking_density"] = serialized(String(stocking_density, 2));
    doc["valid"] = dataValid;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// Tank settings API handlers
void AquariumWebServer::handleGetTankSettings(AsyncWebServerRequest *request) {
    if (tankSettingsManager == nullptr) {
        request->send(500, "application/json", "{\"error\":\"Tank settings manager not initialized\"}");
        return;
    }

    JsonDocument doc;
    TankSettings& settings = tankSettingsManager->getSettings();

    doc["tank_shape"] = (int)settings.tank_shape;
    doc["dimensions"]["length_cm"] = settings.dimensions.length_cm;
    doc["dimensions"]["width_cm"] = settings.dimensions.width_cm;
    doc["dimensions"]["height_cm"] = settings.dimensions.height_cm;
    doc["dimensions"]["radius_cm"] = settings.dimensions.radius_cm;
    doc["calculated_volume_liters"] = settings.calculated_volume_liters;
    doc["manual_volume_liters"] = settings.manual_volume_liters;
    doc["manual_kh_dkh"] = settings.manual_kh_dkh;
    doc["manual_tan_ppm"] = settings.manual_tan_ppm;
    doc["tds_conversion_factor"] = settings.tds_conversion_factor;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleSaveTankSettings(AsyncWebServerRequest *request) {
    if (tankSettingsManager == nullptr) {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Tank settings manager not initialized\"}");
        return;
    }

    // Parse form parameters
    if (request->hasParam("tank_shape", true)) {
        int shape = request->getParam("tank_shape", true)->value().toInt();
        tankSettingsManager->setTankShape((TankShape)shape);
    }

    if (request->hasParam("length", true)) {
        float length = request->getParam("length", true)->value().toFloat();
        float width = request->getParam("width", true)->value().toFloat();
        float height = request->getParam("height", true)->value().toFloat();
        float radius = request->getParam("radius", true)->value().toFloat();
        tankSettingsManager->setDimensions(length, width, height, radius);
    }

    if (request->hasParam("manual_volume", true)) {
        float volume = request->getParam("manual_volume", true)->value().toFloat();
        tankSettingsManager->setManualVolume(volume);
    }

    if (request->hasParam("kh", true)) {
        float kh = request->getParam("kh", true)->value().toFloat();
        tankSettingsManager->setKH(kh);
    }

    if (request->hasParam("tan", true)) {
        float tan = request->getParam("tan", true)->value().toFloat();
        tankSettingsManager->setTAN(tan);
    }

    if (request->hasParam("tds_factor", true)) {
        float factor = request->getParam("tds_factor", true)->value().toFloat();
        tankSettingsManager->setTDSFactor(factor);
    }

    // Calculate volume
    float volume = tankSettingsManager->calculateVolume();

    // Save to NVS
    tankSettingsManager->saveSettings();

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Tank settings saved successfully";
    doc["calculated_volume"] = volume;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// Fish profile API handlers
void AquariumWebServer::handleGetFishList(AsyncWebServerRequest *request) {
    if (tankSettingsManager == nullptr) {
        request->send(500, "application/json", "{\"error\":\"Tank settings manager not initialized\"}");
        return;
    }

    JsonDocument doc;
    JsonArray fishArray = doc["fish"].to<JsonArray>();

    FishProfile* fishList = tankSettingsManager->getFishList();
    uint8_t fishCount = tankSettingsManager->getFishCount();

    for (int i = 0; i < fishCount; i++) {
        JsonObject fish = fishArray.add<JsonObject>();
        fish["species"] = fishList[i].species;
        fish["count"] = fishList[i].count;
        fish["avg_length_cm"] = fishList[i].avg_length_cm;
    }

    doc["total_stocking_length"] = tankSettingsManager->getTotalStockingLength();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleAddFish(AsyncWebServerRequest *request) {
    if (tankSettingsManager == nullptr) {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Tank settings manager not initialized\"}");
        return;
    }

    if (!request->hasParam("species", true) || !request->hasParam("count", true) || !request->hasParam("avg_length", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing required parameters\"}");
        return;
    }

    String species = request->getParam("species", true)->value();
    int count = request->getParam("count", true)->value().toInt();
    float avg_length = request->getParam("avg_length", true)->value().toFloat();

    bool success = tankSettingsManager->addFish(species.c_str(), count, avg_length);

    if (success) {
        tankSettingsManager->saveSettings();

        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "Fish added successfully";
        doc["total_stocking_length"] = tankSettingsManager->getTotalStockingLength();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Failed to add fish (maximum 10 species)\"}");
    }
}

void AquariumWebServer::handleRemoveFish(AsyncWebServerRequest *request) {
    if (tankSettingsManager == nullptr) {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Tank settings manager not initialized\"}");
        return;
    }

    if (!request->hasParam("index", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing index parameter\"}");
        return;
    }

    int index = request->getParam("index", true)->value().toInt();
    bool success = tankSettingsManager->removeFish(index);

    if (success) {
        tankSettingsManager->saveSettings();

        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "Fish removed successfully";

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid fish index\"}");
    }
}

void AquariumWebServer::handleClearFish(AsyncWebServerRequest *request) {
    if (tankSettingsManager == nullptr) {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Tank settings manager not initialized\"}");
        return;
    }

    tankSettingsManager->clearFish();
    tankSettingsManager->saveSettings();

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "All fish cleared successfully";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleGetWarningProfile(AsyncWebServerRequest *request) {
    if (warningManager == nullptr) {
        request->send(500, "application/json", "{\"error\":\"Warning manager not initialized\"}");
        return;
    }

    WarningProfile profile = warningManager->getProfile();

    JsonDocument doc;
    doc["tank_type"] = warningManager->getTankTypeString(profile.tank_type);
    doc["tank_type_code"] = (int)profile.tank_type;

    // Temperature thresholds
    JsonObject temp = doc["temperature"].to<JsonObject>();
    temp["warn_low"] = profile.temperature.warn_low;
    temp["warn_high"] = profile.temperature.warn_high;
    temp["crit_low"] = profile.temperature.crit_low;
    temp["crit_high"] = profile.temperature.crit_high;
    temp["delta_warn_per_hr"] = profile.temperature.delta_warn_per_hr;

    // pH thresholds
    JsonObject pH = doc["ph"].to<JsonObject>();
    pH["warn_low"] = profile.ph.warn_low;
    pH["warn_high"] = profile.ph.warn_high;
    pH["crit_low"] = profile.ph.crit_low;
    pH["crit_high"] = profile.ph.crit_high;
    pH["delta_warn_per_24h"] = profile.ph.delta_warn_per_24h;
    pH["delta_crit_per_24h"] = profile.ph.delta_crit_per_24h;

    // NH3 thresholds
    JsonObject nh3 = doc["nh3"].to<JsonObject>();
    nh3["warn_high"] = profile.nh3.warn_high;
    nh3["crit_high"] = profile.nh3.crit_high;

    // ORP thresholds
    JsonObject orp = doc["orp"].to<JsonObject>();
    orp["warn_low"] = profile.orp.warn_low;
    orp["warn_high"] = profile.orp.warn_high;
    orp["crit_low"] = profile.orp.crit_low;
    orp["crit_high"] = profile.orp.crit_high;

    // Conductivity thresholds
    JsonObject conductivity = doc["conductivity"].to<JsonObject>();
    conductivity["warn_low_us_cm"] = profile.conductivity.warn_low_us_cm;
    conductivity["warn_high_us_cm"] = profile.conductivity.warn_high_us_cm;
    conductivity["crit_low_us_cm"] = profile.conductivity.crit_low_us_cm;
    conductivity["crit_high_us_cm"] = profile.conductivity.crit_high_us_cm;

    // Salinity thresholds
    JsonObject salinity = doc["salinity"].to<JsonObject>();
    salinity["warn_low_psu"] = profile.salinity.warn_low_psu;
    salinity["warn_high_psu"] = profile.salinity.warn_high_psu;
    salinity["crit_low_psu"] = profile.salinity.crit_low_psu;
    salinity["crit_high_psu"] = profile.salinity.crit_high_psu;

    // Dissolved Oxygen thresholds
    JsonObject doThresh = doc["dissolved_oxygen"].to<JsonObject>();
    doThresh["warn_low"] = profile.dissolved_oxygen.warn_low;
    doThresh["crit_low"] = profile.dissolved_oxygen.crit_low;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AquariumWebServer::handleSaveWarningProfile(AsyncWebServerRequest *request) {
    if (warningManager == nullptr) {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Warning manager not initialized\"}");
        return;
    }

    // Check if tank_type parameter was provided
    if (request->hasParam("tank_type", true)) {
        int tankType = request->getParam("tank_type", true)->value().toInt();
        warningManager->setTankType((TankType)tankType);
        warningManager->saveProfile();

        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "Tank type updated successfully";

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
        return;
    }

    // Parse JSON body for custom thresholds
    // This would require async body parsing - for simplicity, we'll just support tank_type change for now
    // Full custom threshold editing can be added later if needed

    request->send(400, "application/json", "{\"success\":false,\"error\":\"No valid parameters provided\"}");
}

void AquariumWebServer::handleGetWarningStates(AsyncWebServerRequest *request) {
    if (warningManager == nullptr) {
        request->send(500, "application/json", "{\"error\":\"Warning manager not initialized\"}");
        return;
    }

    SensorWarningState states = warningManager->getSensorState();

    JsonDocument doc;

    // Temperature state
    JsonObject temp = doc["temperature"].to<JsonObject>();
    temp["value"] = temp_c;
    temp["state"] = warningManager->getStateString((WarningState)states.temperature.state);
    temp["state_code"] = (int)states.temperature.state;

    // pH state
    JsonObject pH = doc["ph"].to<JsonObject>();
    pH["value"] = ph;
    pH["state"] = warningManager->getStateString((WarningState)states.ph.state);
    pH["state_code"] = (int)states.ph.state;

    // NH3 state
    JsonObject nh3 = doc["nh3"].to<JsonObject>();
    nh3["value"] = nh3_ppm;
    nh3["state"] = warningManager->getStateString((WarningState)states.nh3.state);
    nh3["state_code"] = (int)states.nh3.state;

    // ORP state
    JsonObject orp = doc["orp"].to<JsonObject>();
    orp["value"] = orp_mv;
    orp["state"] = warningManager->getStateString((WarningState)states.orp.state);
    orp["state_code"] = (int)states.orp.state;

    // Conductivity state
    JsonObject conductivity = doc["conductivity"].to<JsonObject>();
    conductivity["value"] = ec_ms_cm * 1000.0;  // Convert to µS/cm
    conductivity["state"] = warningManager->getStateString((WarningState)states.conductivity.state);
    conductivity["state_code"] = (int)states.conductivity.state;

    // Dissolved Oxygen state
    JsonObject doState = doc["dissolved_oxygen"].to<JsonObject>();
    doState["value"] = max_do_mg_l;
    doState["state"] = warningManager->getStateString((WarningState)states.dissolved_oxygen.state);
    doState["state_code"] = (int)states.dissolved_oxygen.state;

    // Warning counts
    doc["warning_count"] = warningManager->getWarningCount();
    doc["critical_count"] = warningManager->getCriticalCount();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
