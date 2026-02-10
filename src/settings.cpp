#include "settings.h"

// Global settings instance
Settings settings;

// Constructor
Settings::Settings() {
    netmask = DEFAULT_NETMASK;
    useDHCP = boolToString(DEFAULT_DHCP_ENABLED);
    debugEnabled = boolToString(DEFAULT_DEBUG_ENABLED);
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
// Only updates if firmware was actually UPGRADED (new compile-time version is NEWER than stored version)
// This allows OTA updates to take effect while still detecting firmware rebuilds in development
void Settings::syncVersions() {
    String compiledFw = getCompiledFirmwareVersion();
    String compiledFs = getCompiledFilesystemVersion();
    
    bool fwUpdated = false;
    bool fsUpdated = false;
    
    // Check if STORED version differs from COMPILED version
    preferences.begin(NVS_NAMESPACE_CONFIG, true);
    String storedFw = preferences.getString("fw_version", "");
    String storedFs = preferences.getString("fs_version", "");
    preferences.end();
    
    // For firmware: only override if compiled version is NEWER (not on mismatch)
    if (storedFw.length() == 0) {
        // No stored version yet - use compiled version
        Serial.println("[Settings] No stored firmware version - using compiled version: " + compiledFw);
        firmwareVersion = compiledFw;
        fwUpdated = true;
    } else if (storedFw != compiledFw) {
        // Versions differ - check if compiled is newer
        if (compareVersions(compiledFw, storedFw)) {
            // Compiled version is newer - this is a rebuild/upgrade
            Serial.println("[Settings] Firmware rebuild detected: " + storedFw + " -> " + compiledFw);
            firmwareVersion = compiledFw;
            fwUpdated = true;
        } else {
            // Stored version is same/newer - keep OTA update, use stored version
            Serial.println("[Settings] OTA version detected, keeping: " + storedFw + " (compiled: " + compiledFw + ")");
            firmwareVersion = storedFw;
        }
    } else {
        // Versions match - keep stored version
        firmwareVersion = storedFw;
    }
    
    // For filesystem: only override if compiled version is NEWER (not on mismatch)
    if (storedFs.length() == 0) {
        // No stored version yet - use compiled version
        Serial.println("[Settings] No stored filesystem version - using compiled version: " + compiledFs);
        filesystemVersion = compiledFs;
        fsUpdated = true;
    } else if (storedFs != compiledFs) {
        // Versions differ - check if compiled is newer
        if (compareVersions(compiledFs, storedFs)) {
            // Compiled version is newer - this is a rebuild/upgrade
            Serial.println("[Settings] Filesystem rebuild detected: " + storedFs + " -> " + compiledFs);
            filesystemVersion = compiledFs;
            fsUpdated = true;
        } else {
            // Stored version is same/newer - keep OTA update, use stored version
            Serial.println("[Settings] OTA version detected, keeping: " + storedFs + " (compiled: " + compiledFs + ")");
            filesystemVersion = storedFs;
        }
    } else {
        // Versions match - keep stored version
        filesystemVersion = storedFs;
    }
    
    if (fwUpdated || fsUpdated) {
        updateVersions();
        Serial.println("[Settings] Versions synchronized with firmware");
    } else {
        Serial.println("[Settings] No rebuild detected - keeping stored versions");
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
    // Only save if NTP is enabled
    if (!stringToBool(ntpEnabled)) {
        return;
    }
    
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

// Helper: Compare semantic versions (e.g., "2026.1.1.19" > "2026.1.1.18")
// Returns true if remoteVer > currentVer
bool Settings::compareVersions(String remoteVer, String currentVer) {
    if (remoteVer.length() == 0 || currentVer.length() == 0) return false;
    
    // Strip prefixes for comparison: fw-, fs-
    String remote = remoteVer;
    String current = currentVer;
    
    // Remove version prefixes (fw- and fs- only, no v prefix expected)
    if (remote.startsWith("fw-")) remote = remote.substring(3);
    if (remote.startsWith("fs-")) remote = remote.substring(3);
    
    if (current.startsWith("fw-")) current = current.substring(3);
    if (current.startsWith("fs-")) current = current.substring(3);
    
    // Format: 2026.1.1.00 (dotted format only)
    int r0 = 0, r1 = 0, r2 = 0, r3 = 0;
    int c0 = 0, c1 = 0, c2 = 0, c3 = 0;
    if (sscanf(remote.c_str(), "%d.%d.%d.%d", &r0, &r1, &r2, &r3) != 4) return false;
    if (sscanf(current.c_str(), "%d.%d.%d.%d", &c0, &c1, &c2, &c3) != 4) return false;

    if (r0 != c0) return r0 > c0;
    if (r1 != c1) return r1 > c1;
    if (r2 != c2) return r2 > c2;
    return r3 > c3;
}

// Boot time helpers (debug mode)
Settings::BootTime Settings::getLastBootTime() {
    BootTime result{0, 0, 0, 0, 0, 0, 0, false};
    preferences.begin(NVS_NAMESPACE_CONFIG, true);
    int year = preferences.getInt("bootY", 0);
    int month = preferences.getInt("bootM", 0);
    int day = preferences.getInt("bootD", 0);
    int hour = preferences.getInt("bootH", 0);
    int minute = preferences.getInt("bootMI", 0);
    int second = preferences.getInt("bootS", 0);
    int tzOffset = preferences.getInt("bootTz", 0);
    preferences.end();

    if (year > 1970 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        result.year = year;
        result.month = month;
        result.day = day;
        result.hour = hour;
        result.minute = minute;
        result.second = second;
        result.timezoneOffsetHours = tzOffset;
        result.valid = true;
    }

    return result;
}

void Settings::saveBootTime(int year, int month, int day, int hour, int minute, int second, int timezoneOffsetHours) {
    // Only save if debug is enabled
    if (!stringToBool(debugEnabled)) {
        return;
    }
    
    if (year <= 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return;
    }

    preferences.begin(NVS_NAMESPACE_CONFIG, false);
    preferences.putInt("bootY", year);
    preferences.putInt("bootM", month);
    preferences.putInt("bootD", day);
    preferences.putInt("bootH", hour);
    preferences.putInt("bootMI", minute);
    preferences.putInt("bootS", second);
    preferences.putInt("bootTz", timezoneOffsetHours);
    preferences.end();
}
