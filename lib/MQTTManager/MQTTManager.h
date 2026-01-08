#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

struct MQTTConfiguration {
    bool enabled;
    char broker_host[64];
    uint16_t broker_port;
    char username[32];
    char password[32];
    char device_id[32];
    uint16_t publish_interval_ms;  // Publish frequency in milliseconds
    bool discovery_enabled;        // Home Assistant MQTT Discovery
    unsigned long timestamp;
};

struct SensorData {
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    bool valid;
};

class MQTTManager {
public:
    MQTTManager();
    ~MQTTManager();

    // Initialize MQTT manager
    bool begin();

    // Main loop function - must be called regularly
    void loop();

    // Configuration management
    bool saveMQTTConfig(const MQTTConfiguration& config);
    MQTTConfiguration getMQTTConfig() const;
    void loadMQTTConfig();

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    String getConnectionStatus() const;

    // Publishing
    bool publishSensorData(const SensorData& data);
    bool publishDiscovery();  // Home Assistant MQTT Discovery

    // Get last error message
    String getLastError() const;

private:
    WiFiClient wifiClient;
    PubSubClient* mqttClient;
    Preferences preferences;
    MQTTConfiguration config;

    unsigned long lastPublishTime;
    unsigned long lastReconnectAttempt;
    String lastError;
    bool initialized;

    static const unsigned long RECONNECT_INTERVAL = 5000;  // 5 seconds

    // Topic helpers
    String getBaseTopic() const;
    String getTelemetryTopic(const char* sensor) const;
    String getStateTopic(const char* state) const;
    String getDiscoveryTopic(const char* sensor) const;

    // Reconnection logic
    bool attemptReconnect();

    // Callback for subscribed messages
    static void messageCallback(char* topic, byte* payload, unsigned int length);
};

#endif // MQTT_MANAGER_H
