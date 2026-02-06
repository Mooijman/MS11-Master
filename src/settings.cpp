#include "settings.h"

// Global settings instance
Settings settings;

// Constructor
Settings::Settings() {
    netmask = DEFAULT_NETMASK;
    useDHCP = boolToString(DEFAULT_DHCP_ENABLED);
    debugEnabled = boolToString(DEFAULT_DEBUG_ENABLED);
    gpioViewerEnabled = boolToString(DEFAULT_GPIO_VIEWER_ENABLED);
    otaEnabled = boolToString(DEFAULT_OTA_ENABLED);
    updatesEnabled = boolToString(DEFAULT_UPDATES_ENABLED);
    updateUrl = DEFAULT_UPDATE_URL;
    ntpEnabled = boolToString(DEFAULT_NTP_ENABLED);
    timezone = DEFAULT_TIMEZONE;
}

// Initialize NVS with default values on first boot
void Settings::initialize() {
    if (isInitialized()) {
        Serial.println("[Settings] NVS already initialized");
        return;
    }
    
    Serial.println("[Settings] First boot detected - initializing NVS with default values...");
    
    preferences.begin(NVS_NAMESPACE_CONFIG, false);
    
    // No network defaults - WiFi manager will start on first boot
    // SSID, password, IP, gateway and DHCP remain empty
    
    // Feature defaults (security-conscious)
    preferences.putString("debug", boolToString(DEFAULT_DEBUG_ENABLED));
    preferences.putString("gpioviewer", boolToString(DEFAULT_GPIO_VIEWER_ENABLED));
    preferences.putString("ota", boolToString(DEFAULT_OTA_ENABLED));
    preferences.putString("updates", boolToString(DEFAULT_UPDATES_ENABLED));
    preferences.putString("dhcp", boolToString(DEFAULT_DHCP_ENABLED));
    preferences.putString("netmask", DEFAULT_NETMASK);
    preferences.putString("ntp", boolToString(DEFAULT_NTP_ENABLED));
    preferences.putString("timezone", DEFAULT_TIMEZONE);
    
    // OTA defaults
    preferences.putString("updateUrl", DEFAULT_UPDATE_URL);
    preferences.putString("githubToken", "");
    
    // Version defaults - use compile-time versions
    preferences.putString("fw_version", getCompiledFirmwareVersion());
    preferences.putString("fs_version", getCompiledFilesystemVersion());
    
    preferences.end();
    
    Serial.println("[Settings] NVS initialized with defaults");
}

// Check if NVS has been initialized
bool Settings::isInitialized() {
    preferences.begin(NVS_NAMESPACE_CONFIG, true);
    bool hasConfig = preferences.isKey("ota");  // Use 'ota' key as initialization marker
    preferences.end();
    return hasConfig;
}

// Load all settings from NVS
void Settings::load() {
    preferences.begin(NVS_NAMESPACE_CONFIG, true);  // Read-only
    
    // Network settings
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("pass", "");
    ip = preferences.getString("ip", "");
    gateway = preferences.getString("gateway", "");
    netmask = preferences.getString("netmask", DEFAULT_NETMASK);
    useDHCP = preferences.getString("dhcp", boolToString(DEFAULT_DHCP_ENABLED));
    
    // Feature flags
    debugEnabled = preferences.getString("debug", boolToString(DEFAULT_DEBUG_ENABLED));
    gpioViewerEnabled = preferences.getString("gpioviewer", boolToString(DEFAULT_GPIO_VIEWER_ENABLED));
    otaEnabled = preferences.getString("ota", boolToString(DEFAULT_OTA_ENABLED));
    updatesEnabled = preferences.getString("updates", boolToString(DEFAULT_UPDATES_ENABLED));
    ntpEnabled = preferences.getString("ntp", boolToString(DEFAULT_NTP_ENABLED));
    timezone = preferences.getString("timezone", DEFAULT_TIMEZONE);
    
    // OTA settings
    updateUrl = preferences.getString("updateUrl", DEFAULT_UPDATE_URL);
    githubToken = preferences.getString("githubToken", "");
    
    // Version tracking
    firmwareVersion = preferences.getString("fw_version", getCompiledFirmwareVersion());
    filesystemVersion = preferences.getString("fs_version", getCompiledFilesystemVersion());
    
    preferences.end();
    
    Serial.println("[Settings] Loaded from NVS");
}

// Save all settings to NVS
void Settings::save() {
    preferences.begin(NVS_NAMESPACE_CONFIG, false);  // Read-write
    
    // Network settings
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.putString("ip", ip);
    preferences.putString("gateway", gateway);
    preferences.putString("netmask", netmask);
    preferences.putString("dhcp", useDHCP);
    
    // Feature flags
    preferences.putString("debug", debugEnabled);
    preferences.putString("gpioviewer", gpioViewerEnabled);
    preferences.putString("ota", otaEnabled);
    preferences.putString("updates", updatesEnabled);
    preferences.putString("ntp", ntpEnabled);
    preferences.putString("timezone", timezone);
    
    // OTA settings
    preferences.putString("updateUrl", updateUrl);
    preferences.putString("githubToken", githubToken);
    
    // Version tracking
    preferences.putString("fw_version", firmwareVersion);
    preferences.putString("fs_version", filesystemVersion);
    
    preferences.end();
    
    Serial.println("[Settings] Saved to NVS");
}

// Save only network settings
void Settings::saveNetwork() {
    preferences.begin(NVS_NAMESPACE_CONFIG, false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.putString("ip", ip);
    preferences.putString("gateway", gateway);
    preferences.putString("netmask", netmask);
    preferences.putString("dhcp", useDHCP);
    preferences.end();
    Serial.println("[Settings] Network settings saved");
}

// Save only feature settings
void Settings::saveFeatures() {
    preferences.begin(NVS_NAMESPACE_CONFIG, false);
    preferences.putString("debug", debugEnabled);
    preferences.putString("gpioviewer", gpioViewerEnabled);
    preferences.putString("ota", otaEnabled);
    preferences.putString("updates", updatesEnabled);
    preferences.putString("ntp", ntpEnabled);
    preferences.putString("timezone", timezone);
    preferences.putString("updateUrl", updateUrl);
    preferences.putString("githubToken", githubToken);
    preferences.end();
    Serial.println("[Settings] Feature settings saved");
}

// Update version tracking in NVS
void Settings::updateVersions() {
    preferences.begin(NVS_NAMESPACE_CONFIG, false);
    preferences.putString("fw_version", firmwareVersion);
    preferences.putString("fs_version", filesystemVersion);
    preferences.end();
    Serial.println("[Settings] Versions updated in NVS");
}

// Sync version from compile-time defines with NVS storage
void Settings::syncVersions() {
    String compiledFw = getCompiledFirmwareVersion();
    String compiledFs = getCompiledFilesystemVersion();
    
    bool updated = false;
    
    if (firmwareVersion != compiledFw) {
        Serial.println("[Settings] Firmware version updated: " + firmwareVersion + " -> " + compiledFw);
        firmwareVersion = compiledFw;
        updated = true;
    }
    
    if (filesystemVersion != compiledFs) {
        Serial.println("[Settings] Filesystem version updated: " + filesystemVersion + " -> " + compiledFs);
        filesystemVersion = compiledFs;
        updated = true;
    }
    
    if (updated) {
        updateVersions();
        Serial.println("[Settings] Versions synchronized with firmware");
    }
    
    Serial.println("[Settings] Current versions:");
    Serial.println("  Firmware: " + firmwareVersion);
    Serial.println("  Filesystem: " + filesystemVersion);
}

// Clear WiFi credentials (for reset)
void Settings::clearWiFi() {
    preferences.begin(NVS_NAMESPACE_CONFIG, false);
    preferences.remove("ssid");
    preferences.remove("pass");
    preferences.remove("ip");
    preferences.remove("gateway");
    preferences.putString("dhcp", boolToString(DEFAULT_DHCP_ENABLED));
    preferences.end();
    
    ssid = "";
    password = "";
    ip = "";
    gateway = "";
    useDHCP = boolToString(DEFAULT_DHCP_ENABLED);
    
    Serial.println("[Settings] WiFi credentials cleared");
}

// Reset to factory defaults
void Settings::reset() {
    preferences.begin(NVS_NAMESPACE_CONFIG, false);
    preferences.clear();
    preferences.end();
    
    Serial.println("[Settings] NVS cleared - factory reset");
    
    // Reinitialize with defaults
    initialize();
    load();
}

// Helper: Convert boolean to string format used in settings
String Settings::boolToString(bool value) {
    return value ? "true" : "false";
}

// Helper: Convert string to boolean
bool Settings::stringToBool(const String& value) {
    return (value == "true" || value == "on" || value == "1");
}

// Print current settings to Serial
void Settings::print() {
    Serial.println("\n[Settings] Current Configuration:");
    Serial.println("  Network:");
    Serial.println("    SSID: " + (ssid.length() > 0 ? ssid : "(not set)"));
    Serial.println("    IP: " + (ip.length() > 0 ? ip : "DHCP"));
    Serial.println("    Gateway: " + gateway);
    Serial.println("    Netmask: " + netmask);
    Serial.println("    DHCP: " + useDHCP);
    Serial.println("  Features:");
    Serial.println("    Debug: " + debugEnabled);
    Serial.println("    GPIO Viewer: " + gpioViewerEnabled);
    Serial.println("    OTA: " + otaEnabled);
    Serial.println("    Updates: " + updatesEnabled);
    Serial.println("  OTA:");
    Serial.println("    URL: " + updateUrl);
    Serial.println(String("    Token: ") + (githubToken.length() > 0 ? "***" : "(not set)"));
    Serial.println("  Time:");
    Serial.println("    NTP: " + ntpEnabled);
    Serial.println("    Timezone: " + timezone);
    Serial.println("  Versions:");
    Serial.println("    Firmware: " + firmwareVersion);
    Serial.println("    Filesystem: " + filesystemVersion);
    Serial.println();
}

// Stored date helpers (NTP fallback)
Settings::StoredDate Settings::getStoredDate() {
    StoredDate result{0, 0, 0, false};
    preferences.begin(NVS_NAMESPACE_CONFIG, true);
    int year = preferences.getInt("dateY", 0);
    int month = preferences.getInt("dateM", 0);
    int day = preferences.getInt("dateD", 0);
    preferences.end();

    if (year > 1970 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        result.year = year;
        result.month = month;
        result.day = day;
        result.valid = true;
    }

    return result;
}

void Settings::saveStoredDateIfNeeded(int year, int month, int day) {
    if (year <= 1970 || month < 1 || month > 12 || day < 1 || day > 31) {
        return;
    }

    const uint32_t currentKey = (uint32_t)(year * 10000 + month * 100 + day);

    preferences.begin(NVS_NAMESPACE_CONFIG, false);
    int storedYear = preferences.getInt("dateY", 0);
    int storedMonth = preferences.getInt("dateM", 0);
    int storedDay = preferences.getInt("dateD", 0);
    uint32_t lastSavedKey = preferences.getUInt("dateSaved", 0);

    const uint32_t storedKey = (uint32_t)(storedYear * 10000 + storedMonth * 100 + storedDay);

    if (storedKey != currentKey && lastSavedKey != currentKey) {
        preferences.putInt("dateY", year);
        preferences.putInt("dateM", month);
        preferences.putInt("dateD", day);
        preferences.putUInt("dateSaved", currentKey);
    }

    preferences.end();
}

// Helper: Get compile-time version without prefix
String Settings::getCompiledFirmwareVersion() {
    String ver = String(FIRMWARE_VERSION);
    return ver.substring(3);  // Remove "fw-" prefix
}

String Settings::getCompiledFilesystemVersion() {
    String ver = String(FILESYSTEM_VERSION);
    return ver.substring(3);  // Remove "fs-" prefix
}
