#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// SETTINGS MODULE
// ============================================================================
// Centralized configuration management using NVS (Non-Volatile Storage)
// Handles loading, saving, and initialization of all device settings

class Settings {
public:
    struct StoredDate {
        int year;
        int month;
        int day;
        bool valid;
    };
    // Network settings
    String ssid;
    String password;
    String ip;
    String gateway;
    String netmask;
    String useDHCP;
    
    // Feature flags
    String debugEnabled;
    String gpioViewerEnabled;
    String otaEnabled;
    String updatesEnabled;
    
    // OTA settings
    String updateUrl;
    String githubToken;

    // Time sync
    String ntpEnabled;
    String timezone;
    
    // Version tracking
    String firmwareVersion;
    String filesystemVersion;
    
    // Constructor
    Settings();
    
    // Initialize NVS with default values on first boot
    void initialize();
    
    // Check if NVS has been initialized
    bool isInitialized();
    
    // Load all settings from NVS
    void load();
    
    // Save all settings to NVS
    void save();
    
    // Save only network settings
    void saveNetwork();
    
    // Save only feature settings
    void saveFeatures();
    
    // Update version tracking in NVS
    void updateVersions();
    
    // Sync version from compile-time defines with NVS storage
    void syncVersions();
    
    // Clear WiFi credentials (for reset)
    void clearWiFi();
    
    // Reset to factory defaults
    void reset();
    
    // Helper: Convert boolean to string format used in settings
    static String boolToString(bool value);
    
    // Helper: Convert string to boolean
    static bool stringToBool(const String& value);
    
    // Print current settings to Serial
    void print();

    // Stored date helpers (NTP fallback)
    StoredDate getStoredDate();
    void saveStoredDateIfNeeded(int year, int month, int day);

private:
    Preferences preferences;
    
    // Helper: Get compile-time version without prefix
    String getCompiledFirmwareVersion();
    String getCompiledFilesystemVersion();
};

// Global settings instance
extern Settings settings;

#endif // SETTINGS_H
