#include "WiFiManager.h"

WiFiManager::WiFiManager() : apMode(false) {
}

bool WiFiManager::begin() {
    Serial.println("\n=== WiFi Manager Initializing ===");

    // Try to connect to stored credentials first
    if (hasCredentials()) {
        Serial.println("Found stored credentials, attempting connection...");
        if (connectToWiFi()) {
            return true;
        }
        Serial.println("Failed to connect with stored credentials");
    } else {
        Serial.println("No stored credentials found");
    }

    // If connection failed or no credentials, start AP mode
    Serial.println("Starting provisioning AP mode...");
    startProvisioningAP();
    return false;
}

bool WiFiManager::connectToWiFi() {
    String ssid, password;
    if (!loadCredentials(ssid, password)) {
        Serial.println("Failed to load credentials");
        return false;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttempt = millis();
    int retries = 0;

    while (WiFi.status() != WL_CONNECTED && retries < WIFI_MAX_RETRY) {
        unsigned long attemptStart = millis();

        while (WiFi.status() != WL_CONNECTED &&
               (millis() - attemptStart) < WIFI_CONNECT_TIMEOUT_MS) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() != WL_CONNECTED) {
            retries++;
            if (retries < WIFI_MAX_RETRY) {
                Serial.println();
                Serial.printf("Retry %d/%d...\n", retries + 1, WIFI_MAX_RETRY);
                WiFi.disconnect();
                delay(1000);
                WiFi.begin(ssid.c_str(), password.c_str());
            }
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi connected successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");

        currentSSID = ssid;
        apMode = false;
        return true;
    } else {
        Serial.println();
        Serial.println("Failed to connect to WiFi");
        WiFi.disconnect();
        return false;
    }
}

void WiFiManager::startProvisioningAP() {
    WiFi.mode(WIFI_AP);

    bool success = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

    if (success) {
        apMode = true;
        currentSSID = WIFI_AP_SSID;

        Serial.println("Provisioning AP started successfully!");
        Serial.print("AP SSID: ");
        Serial.println(WIFI_AP_SSID);
        Serial.print("AP Password: ");
        Serial.println(WIFI_AP_PASSWORD);
        Serial.print("AP IP Address: ");
        Serial.println(WiFi.softAPIP());
        Serial.println("Connect to this AP and navigate to http://192.168.4.1");
    } else {
        Serial.println("Failed to start AP mode!");
    }
}

bool WiFiManager::saveCredentials(const String& ssid, const String& password) {
    preferences.begin("wifi", false);

    bool success = preferences.putString("ssid", ssid) &&
                   preferences.putString("password", password);

    preferences.end();

    if (success) {
        Serial.println("WiFi credentials saved successfully");
    } else {
        Serial.println("Failed to save WiFi credentials");
    }

    return success;
}

void WiFiManager::clearCredentials() {
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    Serial.println("WiFi credentials cleared");
}

bool WiFiManager::hasCredentials() {
    preferences.begin("wifi", true);
    bool hasSSID = preferences.isKey("ssid");
    preferences.end();
    return hasSSID;
}

bool WiFiManager::loadCredentials(String& ssid, String& password) {
    preferences.begin("wifi", true);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    preferences.end();

    return (ssid.length() > 0);
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED && !apMode;
}

bool WiFiManager::isAPMode() {
    return apMode;
}

String WiFiManager::getSSID() {
    return currentSSID;
}

String WiFiManager::getIPAddress() {
    if (apMode) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

String WiFiManager::getMACAddress() {
    return WiFi.macAddress();
}
