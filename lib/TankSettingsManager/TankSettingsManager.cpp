#include "TankSettingsManager.h"
#include <math.h>

TankSettingsManager::TankSettingsManager() {
    setDefaults();
}

void TankSettingsManager::begin() {
    if (!loadSettings()) {
        // If loading fails, use defaults
        Serial.println("TankSettingsManager: No saved settings found, using defaults");
        setDefaults();
        saveSettings();  // Save defaults to NVS
    } else {
        Serial.println("TankSettingsManager: Settings loaded successfully");
    }
}

void TankSettingsManager::setDefaults() {
    config.settings.tank_shape = RECTANGLE;
    config.settings.dimensions.length_cm = 0.0;
    config.settings.dimensions.width_cm = 0.0;
    config.settings.dimensions.height_cm = 0.0;
    config.settings.dimensions.radius_cm = 0.0;
    config.settings.calculated_volume_liters = 0.0;
    config.settings.manual_volume_liters = 0.0;
    config.settings.manual_kh_dkh = 4.0;  // Default KH
    config.settings.manual_tan_ppm = 0.0;  // Default TAN
    config.settings.tds_conversion_factor = 0.64;  // Default TDS factor
    config.settings.timestamp = millis();
    config.fish_count = 0;

    // Clear fish list
    for (int i = 0; i < 10; i++) {
        memset(config.fish_list[i].species, 0, 32);
        config.fish_list[i].count = 0;
        config.fish_list[i].avg_length_cm = 0.0;
    }
}

bool TankSettingsManager::saveSettings() {
    preferences.begin("tank_settings", false);  // Read-write mode

    // Save tank settings
    preferences.putUChar("shape", (uint8_t)config.settings.tank_shape);
    preferences.putFloat("length", config.settings.dimensions.length_cm);
    preferences.putFloat("width", config.settings.dimensions.width_cm);
    preferences.putFloat("height", config.settings.dimensions.height_cm);
    preferences.putFloat("radius", config.settings.dimensions.radius_cm);
    preferences.putFloat("calc_vol", config.settings.calculated_volume_liters);
    preferences.putFloat("manual_vol", config.settings.manual_volume_liters);
    preferences.putFloat("kh", config.settings.manual_kh_dkh);
    preferences.putFloat("tan", config.settings.manual_tan_ppm);
    preferences.putFloat("tds_factor", config.settings.tds_conversion_factor);
    preferences.putULong("timestamp", config.settings.timestamp);

    // Save fish count
    preferences.putUChar("fish_count", config.fish_count);

    // Save fish profiles
    for (int i = 0; i < config.fish_count && i < 10; i++) {
        char key[20];
        sprintf(key, "fish_%d_sp", i);
        preferences.putString(key, config.fish_list[i].species);

        sprintf(key, "fish_%d_cnt", i);
        preferences.putInt(key, config.fish_list[i].count);

        sprintf(key, "fish_%d_len", i);
        preferences.putFloat(key, config.fish_list[i].avg_length_cm);
    }

    preferences.end();

    Serial.println("TankSettingsManager: Settings saved to NVS");
    return true;
}

bool TankSettingsManager::loadSettings() {
    preferences.begin("tank_settings", true);  // Read-only mode

    // Check if settings exist
    if (!preferences.isKey("shape")) {
        preferences.end();
        return false;
    }

    // Load tank settings
    config.settings.tank_shape = (TankShape)preferences.getUChar("shape", RECTANGLE);
    config.settings.dimensions.length_cm = preferences.getFloat("length", 0.0);
    config.settings.dimensions.width_cm = preferences.getFloat("width", 0.0);
    config.settings.dimensions.height_cm = preferences.getFloat("height", 0.0);
    config.settings.dimensions.radius_cm = preferences.getFloat("radius", 0.0);
    config.settings.calculated_volume_liters = preferences.getFloat("calc_vol", 0.0);
    config.settings.manual_volume_liters = preferences.getFloat("manual_vol", 0.0);
    config.settings.manual_kh_dkh = preferences.getFloat("kh", 4.0);
    config.settings.manual_tan_ppm = preferences.getFloat("tan", 0.0);
    config.settings.tds_conversion_factor = preferences.getFloat("tds_factor", 0.64);
    config.settings.timestamp = preferences.getULong("timestamp", 0);

    // Load fish count
    config.fish_count = preferences.getUChar("fish_count", 0);

    // Load fish profiles
    for (int i = 0; i < config.fish_count && i < 10; i++) {
        char key[20];
        sprintf(key, "fish_%d_sp", i);
        String species = preferences.getString(key, "");
        strncpy(config.fish_list[i].species, species.c_str(), 31);
        config.fish_list[i].species[31] = '\0';

        sprintf(key, "fish_%d_cnt", i);
        config.fish_list[i].count = preferences.getInt(key, 0);

        sprintf(key, "fish_%d_len", i);
        config.fish_list[i].avg_length_cm = preferences.getFloat(key, 0.0);
    }

    preferences.end();
    return true;
}

float TankSettingsManager::calculateVolume() {
    float volume = 0.0;

    switch (config.settings.tank_shape) {
        case RECTANGLE:
            volume = calculateRectangleVolume();
            break;
        case CUBE:
            volume = calculateCubeVolume();
            break;
        case CYLINDER:
            volume = calculateCylinderVolume();
            break;
        case CUSTOM:
            volume = config.settings.manual_volume_liters;
            break;
    }

    config.settings.calculated_volume_liters = volume;
    return volume;
}

float TankSettingsManager::calculateRectangleVolume() {
    // Volume = length * width * height (in cm³) / 1000 (to liters)
    return (config.settings.dimensions.length_cm *
            config.settings.dimensions.width_cm *
            config.settings.dimensions.height_cm) / 1000.0;
}

float TankSettingsManager::calculateCubeVolume() {
    // Volume = length³ (in cm³) / 1000 (to liters)
    float side = config.settings.dimensions.length_cm;
    return (side * side * side) / 1000.0;
}

float TankSettingsManager::calculateCylinderVolume() {
    // Volume = π * radius² * height (in cm³) / 1000 (to liters)
    float radius = config.settings.dimensions.radius_cm;
    float height = config.settings.dimensions.height_cm;
    return (M_PI * radius * radius * height) / 1000.0;
}

float TankSettingsManager::getTotalStockingLength() {
    float total = 0.0;
    for (int i = 0; i < config.fish_count && i < 10; i++) {
        total += config.fish_list[i].count * config.fish_list[i].avg_length_cm;
    }
    return total;
}

// Getters
TankSettings& TankSettingsManager::getSettings() {
    return config.settings;
}

TankSettingsConfiguration& TankSettingsManager::getConfiguration() {
    return config;
}

FishProfile* TankSettingsManager::getFishList() {
    return config.fish_list;
}

uint8_t TankSettingsManager::getFishCount() {
    return config.fish_count;
}

// Setters
void TankSettingsManager::setTankShape(TankShape shape) {
    config.settings.tank_shape = shape;
    config.settings.timestamp = millis();
}

void TankSettingsManager::setDimensions(float length, float width, float height, float radius) {
    config.settings.dimensions.length_cm = length;
    config.settings.dimensions.width_cm = width;
    config.settings.dimensions.height_cm = height;
    config.settings.dimensions.radius_cm = radius;
    config.settings.timestamp = millis();
}

void TankSettingsManager::setManualVolume(float volume_liters) {
    config.settings.manual_volume_liters = volume_liters;
    config.settings.timestamp = millis();
}

void TankSettingsManager::setKH(float kh_dkh) {
    config.settings.manual_kh_dkh = kh_dkh;
    config.settings.timestamp = millis();
}

void TankSettingsManager::setTAN(float tan_ppm) {
    config.settings.manual_tan_ppm = tan_ppm;
    config.settings.timestamp = millis();
}

void TankSettingsManager::setTDSFactor(float factor) {
    config.settings.tds_conversion_factor = factor;
    config.settings.timestamp = millis();
}

// Fish profile management
bool TankSettingsManager::addFish(const char* species, int count, float avg_length_cm) {
    if (config.fish_count >= 10) {
        Serial.println("TankSettingsManager: Cannot add fish, maximum of 10 species reached");
        return false;
    }

    strncpy(config.fish_list[config.fish_count].species, species, 31);
    config.fish_list[config.fish_count].species[31] = '\0';
    config.fish_list[config.fish_count].count = count;
    config.fish_list[config.fish_count].avg_length_cm = avg_length_cm;
    config.fish_count++;
    config.settings.timestamp = millis();

    Serial.printf("TankSettingsManager: Added fish '%s' (%d @ %.1fcm)\n", species, count, avg_length_cm);
    return true;
}

bool TankSettingsManager::removeFish(uint8_t index) {
    if (index >= config.fish_count) {
        Serial.println("TankSettingsManager: Invalid fish index");
        return false;
    }

    // Shift remaining fish down
    for (int i = index; i < config.fish_count - 1; i++) {
        config.fish_list[i] = config.fish_list[i + 1];
    }

    // Clear last entry
    memset(&config.fish_list[config.fish_count - 1], 0, sizeof(FishProfile));
    config.fish_count--;
    config.settings.timestamp = millis();

    Serial.printf("TankSettingsManager: Removed fish at index %d\n", index);
    return true;
}

void TankSettingsManager::clearFish() {
    for (int i = 0; i < 10; i++) {
        memset(&config.fish_list[i], 0, sizeof(FishProfile));
    }
    config.fish_count = 0;
    config.settings.timestamp = millis();

    Serial.println("TankSettingsManager: Cleared all fish profiles");
}
