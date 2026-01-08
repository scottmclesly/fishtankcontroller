#include "WebServer.h"
#include "WiFiManager.h"
#include "CalibrationManager.h"
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

AquariumWebServer::AquariumWebServer(WiFiManager* wifiMgr, CalibrationManager* calMgr)
    : server(80), wifiManager(wifiMgr), calibrationManager(calMgr),
      raw_temp_mC(0), raw_orp_uV(0), raw_ugs_uV(0), raw_ec_nA(0), raw_ec_uV(0),
      temp_c(0), orp_mv(0), ph(0), ec_ms_cm(0), lastUpdate(0), dataValid(false) {
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

    // Calibration page
    server.on("/calibration", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleCalibrationPage(request);
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

    html += "<div class='status' style='text-align:center;'>";
    html += "✓ Connected to WiFi: <strong>" + wifiManager->getSSID() + "</strong> | ";
    html += "IP: <strong>" + wifiManager->getIPAddress() + "</strong><br>";
    html += "<a href='/calibration' style='color:#006494; text-decoration:none; font-weight:bold; margin-top:10px; display:inline-block;'>🔬 Calibration</a>";
    html += "</div>";

    String calWarning = "";
    if (!calibrationManager->hasValidPHCalibration() || !calibrationManager->hasValidECCalibration()) {
        calWarning = "<div class='warning'>⚠️ Sensors require calibration for accurate readings. ";
        calWarning += "<a href='/calibration' style='color:#856404; text-decoration:underline;'>Click here to calibrate</a></div>";
    }
    html += calWarning;

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
    if (!calibrationManager->hasValidPHCalibration()) {
        html += "<div class='warning' style='font-size: 0.7em; margin-top: 5px;'>⚠ Uncalibrated</div>";
    } else {
        html += "<div style='background: #d4edda; color: #155724; font-size: 0.7em; margin-top: 5px; padding: 3px; border-radius: 3px;'>✓ Calibrated</div>";
    }
    html += "</div>";

    // EC
    html += "<div class='sensor-card'>";
    html += "<div class='sensor-label'>Conductivity</div>";
    html += "<div class='sensor-value'><span id='ec'>";
    html += dataValid ? String(ec_ms_cm, 3) : "--";
    html += "</span> <span class='sensor-unit'>mS/cm</span></div>";
    if (!calibrationManager->hasValidECCalibration()) {
        html += "<div class='warning' style='font-size: 0.7em; margin-top: 5px;'>⚠ Uncalibrated</div>";
    } else {
        html += "<div style='background: #d4edda; color: #155724; font-size: 0.7em; margin-top: 5px; padding: 3px; border-radius: 3px;'>✓ Calibrated</div>";
    }
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

// ============================================================================
// Calibration Handlers
// ============================================================================

void AquariumWebServer::handleCalibrationPage(AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateCalibrationPage());
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

String AquariumWebServer::generateCalibrationPage() {
    String html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Sensor Calibration</title>
    <style>
        * { box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            max-width: 900px;
            margin: 0 auto;
            padding: 20px;
            background: #f0f8ff;
        }
        h1 { color: #006494; text-align: center; }
        h2 { color: #006494; margin-top: 30px; }
        .nav {
            text-align: center;
            margin: 20px 0;
            padding: 10px;
            background: white;
            border-radius: 10px;
        }
        .nav a {
            margin: 0 10px;
            color: #006494;
            text-decoration: none;
            font-weight: bold;
        }
        .card {
            background: white;
            border-radius: 10px;
            padding: 20px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            margin: 20px 0;
        }
        .status {
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-weight: bold;
        }
        .status.calibrated { background: #d4edda; color: #155724; }
        .status.uncalibrated { background: #fff3cd; color: #856404; }
        .form-group { margin: 15px 0; }
        label {
            display: block;
            margin-bottom: 5px;
            color: #333;
            font-weight: bold;
        }
        input, select {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 1em;
        }
        button {
            background: #006494;
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 1em;
            margin: 5px;
        }
        button:hover { background: #004d73; }
        button.secondary { background: #6c757d; }
        button.secondary:hover { background: #5a6268; }
        button.danger { background: #dc3545; }
        button.danger:hover { background: #c82333; }
        .info {
            background: #d1ecf1;
            color: #0c5460;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-size: 0.9em;
        }
        .warning {
            background: #fff3cd;
            color: #856404;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .success {
            background: #d4edda;
            color: #155724;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .error {
            background: #f8d7da;
            color: #721c24;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .readings {
            background: #e9ecef;
            padding: 15px;
            border-radius: 5px;
            margin: 10px 0;
        }
        .readings div {
            margin: 5px 0;
            font-family: monospace;
        }
        .hidden { display: none; }
        .steps {
            background: #f8f9fa;
            padding: 15px;
            border-left: 4px solid #006494;
            margin: 10px 0;
        }
        .steps ol { margin: 10px 0; padding-left: 20px; }
        .steps li { margin: 5px 0; }
    </style>
</head>
<body>
    <h1>🔬 Sensor Calibration</h1>

    <div class='nav'>
        <a href='/'>← Dashboard</a>
        <a href='/calibration'>Calibration</a>
    </div>

    <div id='messages'></div>

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

    <script>
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

        // Initialize on page load
        refreshReadings();
        refreshStatus();
        setInterval(refreshReadings, 5000);
    </script>
</body>
</html>
)rawliteral";

    return html;
}

