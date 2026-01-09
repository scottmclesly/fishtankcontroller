#ifndef TANK_SETTINGS_MANAGER_H
#define TANK_SETTINGS_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// Tank shape enumeration
enum TankShape {
    RECTANGLE = 0,
    CUBE = 1,
    CYLINDER = 2,
    CUSTOM = 3
};

// Tank dimensions structure
struct TankDimensions {
    float length_cm;
    float width_cm;
    float height_cm;
    float radius_cm;
};

// Fish profile structure
struct FishProfile {
    char species[32];
    int count;
    float avg_length_cm;
};

// Tank settings structure
struct TankSettings {
    TankShape tank_shape;
    TankDimensions dimensions;
    float calculated_volume_liters;
    float manual_volume_liters;  // Override for custom shapes
    float manual_kh_dkh;  // Carbonate hardness (default 4.0)
    float manual_tan_ppm;  // Total ammonia nitrogen (default 0.0)
    float tds_conversion_factor;  // TDS conversion factor (default 0.64)
    unsigned long timestamp;
};

// Tank configuration with fish profiles
struct TankSettingsConfiguration {
    TankSettings settings;
    FishProfile fish_list[10];  // Max 10 species
    uint8_t fish_count;
};

class TankSettingsManager {
public:
    TankSettingsManager();

    // Initialize and load settings from NVS
    void begin();

    // Save settings to NVS
    bool saveSettings();

    // Load settings from NVS
    bool loadSettings();

    // Calculate volume based on shape and dimensions
    float calculateVolume();

    // Get total stocking length (sum of all fish lengths)
    float getTotalStockingLength();

    // Getters
    TankSettings& getSettings();
    TankSettingsConfiguration& getConfiguration();
    FishProfile* getFishList();
    uint8_t getFishCount();

    // Tank settings setters
    void setTankShape(TankShape shape);
    void setDimensions(float length, float width, float height, float radius);
    void setManualVolume(float volume_liters);
    void setKH(float kh_dkh);
    void setTAN(float tan_ppm);
    void setTDSFactor(float factor);

    // Fish profile management
    bool addFish(const char* species, int count, float avg_length_cm);
    bool removeFish(uint8_t index);
    void clearFish();

private:
    TankSettingsConfiguration config;
    Preferences preferences;

    // Helper methods
    void setDefaults();
    float calculateRectangleVolume();
    float calculateCubeVolume();
    float calculateCylinderVolume();
};

#endif // TANK_SETTINGS_MANAGER_H
