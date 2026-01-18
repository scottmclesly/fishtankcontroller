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
    char device_id[32];              // User-assigned unit name (friendly name)
    uint16_t publish_interval_ms;    // Publish frequency in milliseconds
    bool discovery_enabled;          // Home Assistant MQTT Discovery
    unsigned long timestamp;
    char chip_id[7];                 // 6-char hex chip ID + null (derived from MAC, read-only)
};

struct SensorData {
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    // Derived metrics
    float tds_ppm;
    float co2_ppm;
    float nh3_ratio;
    float nh3_ppm;
    float max_do_mg_l;
    float stocking_density;
    bool valid;
    // Warning states (0=unknown, 1=normal, 2=warning, 3=critical)
    uint8_t temp_state;
    uint8_t ph_state;
    uint8_t nh3_state;
    uint8_t orp_state;
    uint8_t ec_state;
    uint8_t do_state;
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

    static const unsigned long RECONNECT_INTERVAL = 5000;  // Initial reconnect interval: 5 seconds
    static const unsigned long MAX_RECONNECT_INTERVAL = 60000;  // Maximum backoff: 60 seconds
    unsigned long currentReconnectInterval;  // Dynamic interval with exponential backoff

    // Topic helpers
    String getBaseTopic() const;
    String getTelemetryTopic(const char* sensor) const;
    String getStateTopic(const char* state) const;
    String getDiscoveryTopic(const char* sensor) const;

    // Device ID helpers
    void generateChipId();                     // Generate chip ID from MAC address
    String sanitizeForTopic(const String& name) const;  // Sanitize name for MQTT topics
    String getTopicDeviceId() const;           // Get sanitized device ID for topics

    // Reconnection logic
    bool attemptReconnect();

    // Callback for subscribed messages
    static void messageCallback(char* topic, byte* payload, unsigned int length);
};

#endif // MQTT_MANAGER_H
