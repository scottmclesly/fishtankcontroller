#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

// Wi-Fi Configuration
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_AP_SSID "AquariumSetup"
#define WIFI_AP_PASSWORD "aquarium123"
#define WIFI_MAX_RETRY 3

class WiFiManager {
public:
    WiFiManager();

    // Initialize WiFi - attempts stored credentials, falls back to AP mode
    bool begin();

    // Connect to stored Wi-Fi credentials
    bool connectToWiFi();

    // Start provisioning AP mode
    void startProvisioningAP();

    // Save new Wi-Fi credentials
    bool saveCredentials(const String& ssid, const String& password);

    // Clear stored credentials
    void clearCredentials();

    // Check if credentials are stored
    bool hasCredentials();

    // Check if connected
    bool isConnected();

    // Check if in AP mode
    bool isAPMode();

    // Get current SSID
    String getSSID();

    // Get IP address
    String getIPAddress();

    // Get MAC address
    String getMACAddress();

private:
    Preferences preferences;
    bool apMode;
    String currentSSID;

    // Load credentials from NVS
    bool loadCredentials(String& ssid, String& password);
};

#endif // WIFI_MANAGER_H
