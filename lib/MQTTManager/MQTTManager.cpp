#include "MQTTManager.h"

// Preferences namespace and keys
static const char* PREF_NAMESPACE = "mqtt";
static const char* KEY_ENABLED = "enabled";
static const char* KEY_BROKER_HOST = "broker_host";
static const char* KEY_BROKER_PORT = "broker_port";
static const char* KEY_USERNAME = "username";
static const char* KEY_PASSWORD = "password";
static const char* KEY_DEVICE_ID = "device_id";
static const char* KEY_PUBLISH_INTERVAL = "pub_interval";
static const char* KEY_DISCOVERY_EN = "discovery_en";

MQTTManager::MQTTManager()
    : mqttClient(nullptr),
      lastPublishTime(0),
      lastReconnectAttempt(0),
      initialized(false),
      currentReconnectInterval(RECONNECT_INTERVAL) {

    // Initialize config with defaults
    config.enabled = false;
    strncpy(config.broker_host, "", sizeof(config.broker_host));
    config.broker_port = 1883;
    strncpy(config.username, "", sizeof(config.username));
    strncpy(config.password, "", sizeof(config.password));
    strncpy(config.device_id, "aquarium", sizeof(config.device_id));
    config.publish_interval_ms = 5000;  // Default 5 seconds
    config.discovery_enabled = false;
    config.timestamp = 0;
    strncpy(config.chip_id, "", sizeof(config.chip_id));
}

MQTTManager::~MQTTManager() {
    if (mqttClient) {
        if (mqttClient->connected()) {
            mqttClient->disconnect();
        }
        delete mqttClient;
    }
}

bool MQTTManager::begin() {
    Serial.println("[MQTT] Initializing MQTT Manager...");

    // Generate unique chip ID from MAC address
    generateChipId();

    // Load configuration from NVS
    loadMQTTConfig();

    // Create MQTT client
    mqttClient = new PubSubClient(wifiClient);

    if (!mqttClient) {
        lastError = "Failed to create MQTT client";
        Serial.println("[MQTT] ERROR: " + lastError);
        return false;
    }

    // Set callback for incoming messages
    mqttClient->setCallback(MQTTManager::messageCallback);

    // Set buffer size for larger payloads (Home Assistant Discovery)
    mqttClient->setBufferSize(512);

    initialized = true;

    // If enabled, attempt initial connection
    if (config.enabled && strlen(config.broker_host) > 0) {
        Serial.println("[MQTT] Auto-connecting to broker...");
        connect();
    } else {
        Serial.println("[MQTT] MQTT is disabled or not configured");
    }

    return true;
}

void MQTTManager::loop() {
    if (!initialized || !config.enabled) {
        return;
    }

    // Handle MQTT client loop
    if (mqttClient->connected()) {
        mqttClient->loop();
    } else {
        // Attempt reconnection with exponential backoff
        unsigned long now = millis();
        if (now - lastReconnectAttempt > currentReconnectInterval) {
            lastReconnectAttempt = now;
            if (attemptReconnect()) {
                // Reset interval on successful connection
                currentReconnectInterval = RECONNECT_INTERVAL;
                lastReconnectAttempt = 0;
            } else {
                // Exponential backoff on failure (double the interval, cap at MAX_RECONNECT_INTERVAL)
                currentReconnectInterval = min(currentReconnectInterval * 2, MAX_RECONNECT_INTERVAL);
            }
        }
    }
}

bool MQTTManager::saveMQTTConfig(const MQTTConfiguration& newConfig) {
    Serial.println("[MQTT] Saving MQTT configuration...");

    if (!preferences.begin(PREF_NAMESPACE, false)) {
        lastError = "Failed to open preferences for writing";
        Serial.println("[MQTT] ERROR: " + lastError);
        return false;
    }

    preferences.putBool(KEY_ENABLED, newConfig.enabled);
    preferences.putString(KEY_BROKER_HOST, newConfig.broker_host);
    preferences.putUShort(KEY_BROKER_PORT, newConfig.broker_port);
    preferences.putString(KEY_USERNAME, newConfig.username);
    preferences.putString(KEY_PASSWORD, newConfig.password);
    preferences.putString(KEY_DEVICE_ID, newConfig.device_id);
    preferences.putUShort(KEY_PUBLISH_INTERVAL, newConfig.publish_interval_ms);
    preferences.putBool(KEY_DISCOVERY_EN, newConfig.discovery_enabled);

    preferences.end();

    // Update local config
    config = newConfig;
    config.timestamp = millis();

    Serial.println("[MQTT] Configuration saved successfully");
    Serial.printf("[MQTT] Broker: %s:%d, Device ID: %s, Enabled: %s\n",
                  config.broker_host, config.broker_port, config.device_id,
                  config.enabled ? "YES" : "NO");

    // Reconnect if enabled
    if (config.enabled && initialized) {
        disconnect();
        connect();
    }

    return true;
}

MQTTConfiguration MQTTManager::getMQTTConfig() const {
    return config;
}

void MQTTManager::loadMQTTConfig() {
    Serial.println("[MQTT] Loading MQTT configuration from NVS...");

    if (!preferences.begin(PREF_NAMESPACE, true)) {
        Serial.println("[MQTT] No saved configuration found, using defaults");
        return;
    }

    config.enabled = preferences.getBool(KEY_ENABLED, false);
    preferences.getString(KEY_BROKER_HOST, config.broker_host, sizeof(config.broker_host));
    config.broker_port = preferences.getUShort(KEY_BROKER_PORT, 1883);
    preferences.getString(KEY_USERNAME, config.username, sizeof(config.username));
    preferences.getString(KEY_PASSWORD, config.password, sizeof(config.password));
    preferences.getString(KEY_DEVICE_ID, config.device_id, sizeof(config.device_id));
    config.publish_interval_ms = preferences.getUShort(KEY_PUBLISH_INTERVAL, 5000);
    config.discovery_enabled = preferences.getBool(KEY_DISCOVERY_EN, false);

    preferences.end();

    Serial.printf("[MQTT] Loaded config - Broker: %s:%d, Device ID: %s, Enabled: %s\n",
                  config.broker_host, config.broker_port, config.device_id,
                  config.enabled ? "YES" : "NO");
}

bool MQTTManager::connect() {
    if (!initialized) {
        lastError = "MQTT Manager not initialized";
        return false;
    }

    if (!config.enabled) {
        lastError = "MQTT is disabled";
        return false;
    }

    if (strlen(config.broker_host) == 0) {
        lastError = "No broker host configured";
        return false;
    }

    if (!WiFi.isConnected()) {
        lastError = "WiFi not connected";
        return false;
    }

    // Use topic device ID as MQTT client ID (includes chip ID for uniqueness)
    String clientId = getTopicDeviceId();

    Serial.printf("[MQTT] Connecting to broker %s:%d as '%s'...\n",
                  config.broker_host, config.broker_port, clientId.c_str());

    mqttClient->setServer(config.broker_host, config.broker_port);

    bool connected = false;

    // Connect with or without authentication
    if (strlen(config.username) > 0 && strlen(config.password) > 0) {
        connected = mqttClient->connect(clientId.c_str(), config.username, config.password);
    } else {
        connected = mqttClient->connect(clientId.c_str());
    }

    if (connected) {
        Serial.println("[MQTT] Connected successfully!");
        lastError = "";

        // Publish discovery messages if enabled
        if (config.discovery_enabled) {
            publishDiscovery();
        }

        return true;
    } else {
        int state = mqttClient->state();
        lastError = "Connection failed, state=" + String(state);
        Serial.println("[MQTT] " + lastError);
        return false;
    }
}

void MQTTManager::disconnect() {
    if (mqttClient && mqttClient->connected()) {
        Serial.println("[MQTT] Disconnecting from broker...");
        mqttClient->disconnect();
    }
}

bool MQTTManager::isConnected() const {
    return initialized && config.enabled && mqttClient && mqttClient->connected();
}

String MQTTManager::getConnectionStatus() const {
    if (!initialized) {
        return "Not initialized";
    }
    if (!config.enabled) {
        return "Disabled";
    }
    if (!WiFi.isConnected()) {
        return "WiFi disconnected";
    }
    if (!mqttClient) {
        return "Client error";
    }
    if (mqttClient->connected()) {
        return "Connected";
    }

    int state = mqttClient->state();
    switch (state) {
        case -4: return "Connection timeout";
        case -3: return "Connection lost";
        case -2: return "Connect failed";
        case -1: return "Disconnected";
        case 1: return "Bad protocol";
        case 2: return "Bad client ID";
        case 3: return "Unavailable";
        case 4: return "Bad credentials";
        case 5: return "Unauthorized";
        default: return "Unknown (" + String(state) + ")";
    }
}

bool MQTTManager::publishSensorData(const SensorData& data) {
    if (!isConnected()) {
        return false;
    }

    // Check publish interval
    unsigned long now = millis();
    if (now - lastPublishTime < config.publish_interval_ms) {
        return true;  // Not an error, just skipping
    }

    bool success = true;

    // Publish individual sensor topics
    if (data.valid) {
        char payload[32];

        // Primary sensors
        // Temperature
        snprintf(payload, sizeof(payload), "%.2f", data.temp_c);
        success &= mqttClient->publish(getTelemetryTopic("temperature").c_str(), payload, true);

        // ORP
        snprintf(payload, sizeof(payload), "%.1f", data.orp_mv);
        success &= mqttClient->publish(getTelemetryTopic("orp").c_str(), payload, true);

        // pH
        snprintf(payload, sizeof(payload), "%.2f", data.ph);
        success &= mqttClient->publish(getTelemetryTopic("ph").c_str(), payload, true);

        // EC
        snprintf(payload, sizeof(payload), "%.3f", data.ec_ms_cm);
        success &= mqttClient->publish(getTelemetryTopic("ec").c_str(), payload, true);

        // Derived metrics
        // TDS
        snprintf(payload, sizeof(payload), "%.1f", data.tds_ppm);
        success &= mqttClient->publish(getTelemetryTopic("tds").c_str(), payload, true);

        // CO2
        snprintf(payload, sizeof(payload), "%.2f", data.co2_ppm);
        success &= mqttClient->publish(getTelemetryTopic("co2").c_str(), payload, true);

        // NH3 Fraction (as percentage 0-100)
        // Topic name is self-describing: "nh3_fraction_percent"
        snprintf(payload, sizeof(payload), "%.2f", data.nh3_ratio * 100.0);
        success &= mqttClient->publish(getTelemetryTopic("nh3_fraction_percent").c_str(), payload, true);

        // NH3 PPM (toxic ammonia concentration)
        snprintf(payload, sizeof(payload), "%.3f", data.nh3_ppm);
        success &= mqttClient->publish(getTelemetryTopic("nh3_ppm").c_str(), payload, true);

        // Max DO
        snprintf(payload, sizeof(payload), "%.2f", data.max_do_mg_l);
        success &= mqttClient->publish(getTelemetryTopic("max_do").c_str(), payload, true);

        // Stocking Density
        snprintf(payload, sizeof(payload), "%.2f", data.stocking_density);
        success &= mqttClient->publish(getTelemetryTopic("stocking").c_str(), payload, true);
    }

    // Publish warning state topics
    if (data.valid) {
        char statePayload[16];

        // Temperature state
        snprintf(statePayload, sizeof(statePayload), "%d", data.temp_state);
        success &= mqttClient->publish(getTelemetryTopic("temp_state").c_str(), statePayload, true);

        // pH state
        snprintf(statePayload, sizeof(statePayload), "%d", data.ph_state);
        success &= mqttClient->publish(getTelemetryTopic("ph_state").c_str(), statePayload, true);

        // NH3 state
        snprintf(statePayload, sizeof(statePayload), "%d", data.nh3_state);
        success &= mqttClient->publish(getTelemetryTopic("nh3_state").c_str(), statePayload, true);

        // ORP state
        snprintf(statePayload, sizeof(statePayload), "%d", data.orp_state);
        success &= mqttClient->publish(getTelemetryTopic("orp_state").c_str(), statePayload, true);

        // EC state
        snprintf(statePayload, sizeof(statePayload), "%d", data.ec_state);
        success &= mqttClient->publish(getTelemetryTopic("ec_state").c_str(), statePayload, true);

        // DO state
        snprintf(statePayload, sizeof(statePayload), "%d", data.do_state);
        success &= mqttClient->publish(getTelemetryTopic("do_state").c_str(), statePayload, true);
    }

    // Also publish combined JSON payload
    JsonDocument doc;
    // Primary sensors
    doc["temperature_c"] = data.temp_c;
    doc["orp_mv"] = data.orp_mv;
    doc["ph"] = data.ph;
    doc["ec_ms_cm"] = data.ec_ms_cm;
    // Derived metrics
    doc["tds_ppm"] = data.tds_ppm;
    doc["co2_ppm"] = data.co2_ppm;
    doc["nh3_ratio"] = data.nh3_ratio;
    doc["nh3_ppm"] = data.nh3_ppm;
    doc["max_do_mg_l"] = data.max_do_mg_l;
    doc["stocking_density"] = data.stocking_density;
    // Warning states
    doc["temp_state"] = data.temp_state;
    doc["ph_state"] = data.ph_state;
    doc["nh3_state"] = data.nh3_state;
    doc["orp_state"] = data.orp_state;
    doc["ec_state"] = data.ec_state;
    doc["do_state"] = data.do_state;
    doc["valid"] = data.valid;
    doc["timestamp"] = now;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    success &= mqttClient->publish(getTelemetryTopic("sensors").c_str(),
                                    jsonPayload.c_str(), true);

    if (success) {
        lastPublishTime = now;
    }

    return success;
}

bool MQTTManager::publishDiscovery() {
    if (!isConnected() || !config.discovery_enabled) {
        return false;
    }

    Serial.println("[MQTT] Publishing Home Assistant Discovery messages...");

    // Get device identifiers
    String topicDeviceId = getTopicDeviceId();  // e.g., "kates_aquarium_7-A1B2C3"
    String friendlyName = String(config.device_id);  // User's friendly name (e.g., "Kate's Aquarium #7")

    // Helper lambda to publish discovery for a sensor
    auto publishSensor = [this, &topicDeviceId, &friendlyName](const char* sensorName, const char* deviceClass,
                                 const char* unit, const char* icon) -> bool {
        JsonDocument doc;

        // Entity name uses friendly unit name for display
        doc["name"] = friendlyName + " " + String(sensorName);
        // Unique ID uses topic device ID (includes chip ID) to guarantee uniqueness
        doc["unique_id"] = topicDeviceId + "_" + String(sensorName);
        doc["state_topic"] = getTelemetryTopic(sensorName);
        doc["device_class"] = deviceClass;
        doc["unit_of_measurement"] = unit;
        doc["icon"] = icon;

        JsonObject device = doc["device"].to<JsonObject>();
        // Device identifier uses topic ID for uniqueness
        device["identifiers"][0] = topicDeviceId;
        // Device display name uses friendly name
        device["name"] = friendlyName;
        device["model"] = "POET Aquarium Controller";
        device["manufacturer"] = "DIY";

        String payload;
        serializeJson(doc, payload);

        String topic = getDiscoveryTopic(sensorName);
        return mqttClient->publish(topic.c_str(), payload.c_str(), true);
    };

    bool success = true;
    // Primary sensors
    success &= publishSensor("temperature", "temperature", "°C", "mdi:thermometer");
    success &= publishSensor("orp", "voltage", "mV", "mdi:flash");
    success &= publishSensor("ph", "", "pH", "mdi:ph");
    success &= publishSensor("ec", "voltage", "mS/cm", "mdi:water-percent");

    // Derived metrics
    success &= publishSensor("tds", "", "ppm", "mdi:water-opacity");
    success &= publishSensor("co2", "", "ppm", "mdi:molecule-co2");
    success &= publishSensor("nh3_fraction_percent", "", "%", "mdi:alert-circle");
    success &= publishSensor("nh3_ppm", "", "ppm", "mdi:biohazard");
    success &= publishSensor("max_do", "", "mg/L", "mdi:air-filter");
    success &= publishSensor("stocking", "", "cm/L", "mdi:fish");

    if (success) {
        Serial.println("[MQTT] Discovery messages published successfully");
    } else {
        Serial.println("[MQTT] Failed to publish some discovery messages");
    }

    return success;
}

String MQTTManager::getLastError() const {
    return lastError;
}

void MQTTManager::generateChipId() {
    // Get MAC address and use last 3 bytes (6 hex chars) as unique ID
    uint64_t mac = ESP.getEfuseMac();
    snprintf(config.chip_id, sizeof(config.chip_id), "%06X",
             (uint32_t)(mac & 0xFFFFFF));
    Serial.printf("[MQTT] Generated chip ID: %s\n", config.chip_id);
}

String MQTTManager::sanitizeForTopic(const String& name) const {
    // Convert to lowercase, replace spaces and special chars with underscores
    // Only allow alphanumeric and underscores for MQTT topic compatibility
    String result = "";
    for (size_t i = 0; i < name.length() && result.length() < 24; i++) {
        char c = name.charAt(i);
        if (c >= 'A' && c <= 'Z') {
            result += (char)(c + 32);  // Convert to lowercase
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            result += c;
        } else if (c == ' ' || c == '-' || c == '_') {
            // Avoid consecutive underscores
            if (result.length() == 0 || result.charAt(result.length() - 1) != '_') {
                result += '_';
            }
        }
        // Skip other special characters (apostrophes, #, etc.)
    }
    // Remove trailing underscore
    while (result.length() > 0 && result.charAt(result.length() - 1) == '_') {
        result = result.substring(0, result.length() - 1);
    }
    // Default if empty
    if (result.length() == 0) {
        result = "aquarium";
    }
    return result;
}

String MQTTManager::getTopicDeviceId() const {
    // Format: sanitized_unit_name-CHIPID
    // Example: "kates_aquarium_7-A1B2C3"
    // This ensures uniqueness even if users name all units the same
    String sanitized = sanitizeForTopic(String(config.device_id));
    return sanitized + "-" + String(config.chip_id);
}

String MQTTManager::getBaseTopic() const {
    return String("aquarium/") + getTopicDeviceId();
}

String MQTTManager::getTelemetryTopic(const char* sensor) const {
    return getBaseTopic() + "/telemetry/" + sensor;
}

String MQTTManager::getStateTopic(const char* state) const {
    return getBaseTopic() + "/state/" + state;
}

String MQTTManager::getDiscoveryTopic(const char* sensor) const {
    return String("homeassistant/sensor/") + getTopicDeviceId() + "/" + sensor + "/config";
}

bool MQTTManager::attemptReconnect() {
    Serial.println("[MQTT] Attempting to reconnect...");
    return connect();
}

void MQTTManager::messageCallback(char* topic, byte* payload, unsigned int length) {
    // Handle incoming MQTT messages (for future command topics)
    Serial.printf("[MQTT] Message received on topic: %s\n", topic);

    // For now, just log the message
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    Serial.printf("[MQTT] Payload: %s\n", message);
}
