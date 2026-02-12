#include "web_server_routes.h"
#include "app_state.h"
#include "config.h"
#include "settings.h"
#include "utils.h"
#include "ntp_manager.h"
#include "i2c_manager.h"
#include "display_manager.h"
#include "lcd_manager.h"
#include "slave_controller.h"
#include "github_updater.h"
#include "md11_slave_update.h"
#include "LittleFS.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <time.h>

// ============================================================================
// FORWARD DECLARATIONS (internal helpers)
// ============================================================================

static void registerPageRoutes(AsyncWebServer& server);
static void registerSettingsRoutes(AsyncWebServer& server);
static void registerI2CApiRoutes(AsyncWebServer& server);
static void registerUpdateApiRoutes(AsyncWebServer& server);
static void registerFileApiRoutes(AsyncWebServer& server);

// ============================================================================
// PUBLIC: Register all STA-mode routes
// ============================================================================

void registerSTARoutes(AsyncWebServer& server) {
  registerPageRoutes(server);
  registerSettingsRoutes(server);
  registerI2CApiRoutes(server);
  registerUpdateApiRoutes(server);
  registerFileApiRoutes(server);

  // Serve static files (CSS, images, etc.) - must be last
  server.serveStatic("/", LittleFS, "/");
}

// ============================================================================
// PAGE ROUTES - Static HTML pages
// ============================================================================

static void registerPageRoutes(AsyncWebServer& server) {
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // File manager page
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/files.html", "text/html");
  });

  // Firmware update page
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/update.html", "text/html");
  });

  // I2C diagnostics page
  server.on("/i2c", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool debugEnabledBool = Settings::stringToBool(settings.debugEnabled);
    
    request->send(LittleFS, "/i2c.html", "text/html", false, [debugEnabledBool](const String& var) -> String {
      if (var == "DEBUG_ENABLED") {
        return debugEnabledBool ? "true" : "false";
      }
      return String();
    });
  });

  // I2C demo page
  server.on("/i2cdemo", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/i2cdemo.html", "text/html");
  });

  // Confirmation page with auto-refresh
  server.on("/confirm.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    String message = "Update Successful!";
    String messageClass = "text-success";
    
    // Get message from query parameter if provided
    if (request->hasParam("message")) {
      message = request->getParam("message")->value();
    }
    if (request->hasParam("type")) {
      String type = request->getParam("type")->value();
      if (type == "success") {
        messageClass = "text-success";
      } else if (type == "error") {
        messageClass = "text-error";
      } else if (type == "warning") {
        messageClass = "text-warning";
      }
    }
    
    request->send(LittleFS, "/confirm.html", "text/html", false, [message, messageClass](const String& var) -> String {
      if (var == "MESSAGE") {
        return message;
      }
      if (var == "MESSAGE_CLASS") {
        return messageClass;
      }
      if (var == "RELOAD_BUTTON") {
        return ""; // No button needed, auto-refresh handles it
      }
      return String();
    });
  });
}

// ============================================================================
// SETTINGS ROUTES - Settings page GET/POST, reboot, reset
// ============================================================================

static void registerSettingsRoutes(AsyncWebServer& server) {
  // Route for settings page with template processor
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool updatesEnabledBool = Settings::stringToBool(settings.updatesEnabled);
    bool otaEnabledBool = Settings::stringToBool(settings.otaEnabled);
    bool dhcpEnabledBool = Settings::stringToBool(settings.useDHCP);
    bool debugEnabledBool = Settings::stringToBool(settings.debugEnabled);
    bool ntpEnabledBool = Settings::stringToBool(settings.ntpEnabled);
    
    int timezoneOffsetHours = parseTimezoneOffset(settings.timezone);
    
    // Get current time and apply timezone offset
    time_t now = time(nullptr);
    time_t serverTimeWithOffset = now + (timezoneOffsetHours * 3600);
    struct tm timeinfo;
    gmtime_r(&serverTimeWithOffset, &timeinfo);
    char currentDateTime[30];
    snprintf(currentDateTime, sizeof(currentDateTime), "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    // Convert to milliseconds for JavaScript (live clock on client side)
    String serverTimeMs = String((long long)serverTimeWithOffset * 1000);
    
    String lastBootDisplay = "-";
    Settings::BootTime lastBoot = settings.getLastBootTime();
    if (lastBoot.valid) {
      struct tm bootTm = {};
      bootTm.tm_year = lastBoot.year - 1900;
      bootTm.tm_mon = lastBoot.month - 1;
      bootTm.tm_mday = lastBoot.day;
      bootTm.tm_hour = lastBoot.hour;
      bootTm.tm_min = lastBoot.minute;
      bootTm.tm_sec = lastBoot.second;
      bootTm.tm_isdst = -1;
      time_t bootTime = mktime(&bootTm);
      bootTime += (lastBoot.timezoneOffsetHours * 3600);
      struct tm* bootInfo = gmtime(&bootTime);
      char lastBootStr[30];
      snprintf(lastBootStr, sizeof(lastBootStr), "%04d-%02d-%02d %02d:%02d:%02d",
               bootInfo->tm_year + 1900, bootInfo->tm_mon + 1, bootInfo->tm_mday,
               bootInfo->tm_hour, bootInfo->tm_min, bootInfo->tm_sec);
      lastBootDisplay = String(lastBootStr);
    }
    
    request->send(LittleFS, "/settings.html", "text/html", false, [updatesEnabledBool, otaEnabledBool, dhcpEnabledBool, debugEnabledBool, ntpEnabledBool, currentDateTime, lastBootDisplay, serverTimeMs](const String& var) -> String {
      if (var == "SSID") {
        return settings.ssid;
      }
      if (var == "PASSWORD") {
        return "";  // Don't return actual password, use placeholder instead
      }
      if (var == "IP_ADDRESS") {
        return settings.ip;
      }
      if (var == "GATEWAY") {
        return settings.gateway;
      }
      if (var == "NETMASK") {
        return settings.netmask;
      }
      if (var == "DHCP_CHECKED") {
        return dhcpEnabledBool ? "checked" : "";
      }
      if (var == "DEBUG_CHECKED") {
        return debugEnabledBool ? "checked" : "";
      }
      if (var == "DEBUG_DISPLAY") {
        return debugEnabledBool ? "style=\"display: block;\"" : "style=\"display: none;\"";
      }
      if (var == "FW_VERSION") {
        return settings.firmwareVersion;
      }
      if (var == "FS_VERSION") {
        return settings.filesystemVersion;
      }
      if (var == "OTA_CHECKED") {
        return otaEnabledBool ? "checked" : "";
      }
      if (var == "UPDATES_CHECKED") {
        return updatesEnabledBool ? "checked" : "";
      }
      if (var == "NTP_CHECKED") {
        return ntpEnabledBool ? "checked" : "";
      }
      if (var == "TIMEZONE_GROUP_DISPLAY") {
        return ntpEnabledBool ? "" : "style=\"display: none;\"";
      }
      if (var == "NTP_TIMES_DISPLAY") {
        return ntpEnabledBool ? "style=\"margin-top: 10px;\"" : "style=\"margin-top: 10px; display: none;\"";
      }
      if (var == "TIMEZONE") {
        return settings.timezone;
      }
      if (var == "TIMEZONE_UTC0") {
        return (settings.timezone == "UTC+0") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC1") {
        return (settings.timezone == "UTC+1") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC2") {
        return (settings.timezone == "UTC+2") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC3") {
        return (settings.timezone == "UTC+3") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC4") {
        return (settings.timezone == "UTC+4") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC5") {
        return (settings.timezone == "UTC+5") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC6") {
        return (settings.timezone == "UTC+6") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC7") {
        return (settings.timezone == "UTC+7") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC8") {
        return (settings.timezone == "UTC+8") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC9") {
        return (settings.timezone == "UTC+9") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC10") {
        return (settings.timezone == "UTC+10") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC11") {
        return (settings.timezone == "UTC+11") ? "selected" : "";
      }
      if (var == "TIMEZONE_UTC12") {
        return (settings.timezone == "UTC+12") ? "selected" : "";
      }
      if (var == "UPDATES_DISPLAY") {
        return updatesEnabledBool ? "style=\"display: flex;\"" : "style=\"display: none;\"";
      }
      if (var == "UPDATES_BUTTON") {
        if (updatesEnabledBool) {
          return "<a href=\"/update\" class=\"btn-small btn-update-link\">Update</a>";
        }
        return "";
      }
      if (var == "UPDATE_URL") {
        return settings.updateUrl;
      }
      if (var == "GITHUB_TOKEN") {
        return settings.githubToken;
      }
      if (var == "FILE_MANAGER_VISIBILITY") {
        return debugEnabledBool ? "style=\"visibility: visible;\"" : "style=\"visibility: hidden;\"";
      }
      if (var == "CURRENT_DATETIME") {
        return String(currentDateTime);
      }
      if (var == "SERVER_TIME_MS") {
        return serverTimeMs;
      }
      if (var == "LAST_BOOT_TIME") {
        return lastBootDisplay;
      }
      return String();
    });
  });
  
  // Handle settings POST
  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool shouldReboot = false;
    bool configChanged = false;
    String message = "";
    
    Serial.println("Settings POST received");
    
    // SSID
    if (request->hasParam("ssid", true)) {
      String newSsid = request->getParam("ssid", true)->value();
      newSsid.trim();
      if (newSsid.length() > 0 && newSsid != settings.ssid) {
        settings.ssid = newSsid;
        configChanged = true;
        shouldReboot = true;
        Serial.println("SSID changed to: " + settings.ssid);
      }
    }
    
    // Password
    if (request->hasParam("password", true)) {
      String newPass = request->getParam("password", true)->value();
      if (newPass.length() > 0 && newPass != settings.password) {
        settings.password = newPass;
        configChanged = true;
        shouldReboot = true;
        // Save password to NVS
        preferences.begin("config", false);
        preferences.putString("pass", settings.password);
        preferences.end();
        Serial.println("Password changed");
      }
    }
    
    // DHCP checkbox
    String newDhcp = request->hasParam("dhcp", true) ? "on" : "off";
    String currentDhcp = Settings::stringToBool(settings.useDHCP) ? "on" : "off";
    if (newDhcp != currentDhcp) {
      settings.useDHCP = newDhcp;
      configChanged = true;
      shouldReboot = true;
      Serial.println("DHCP changed to: " + settings.useDHCP);
    }
    
    // Network settings
    if (request->hasParam("ip", true)) {
      String newIp = request->getParam("ip", true)->value();
      newIp.trim();
      if (newIp != settings.ip) {
        settings.ip = newIp;
        configChanged = true;
        shouldReboot = true;
        Serial.println("IP changed to: " + settings.ip);
      }
    }
    
    if (request->hasParam("gateway", true)) {
      String newGateway = request->getParam("gateway", true)->value();
      newGateway.trim();
      if (newGateway != settings.gateway) {
        settings.gateway = newGateway;
        configChanged = true;
        shouldReboot = true;
        Serial.println("Gateway changed to: " + settings.gateway);
      }
    }
    
    if (request->hasParam("netmask", true)) {
      String newNetmask = request->getParam("netmask", true)->value();
      newNetmask.trim();
      if (newNetmask != settings.netmask) {
        settings.netmask = newNetmask;
        configChanged = true;
        shouldReboot = true;
        Serial.println("Netmask changed to: " + settings.netmask);
      }
    }
    
    // Check debug checkbox
    if (request->hasParam("debug", true)) {
      if (settings.debugEnabled != "on" && settings.debugEnabled != "true") {
        settings.debugEnabled = "on";
        configChanged = true;
        Serial.println("Debug options enabled");
      }
    } else {
      if (settings.debugEnabled == "on" || settings.debugEnabled == "true") {
        settings.debugEnabled = "off";
        configChanged = true;
        Serial.println("Debug options disabled");
        
        // Also disable OTA when debug is disabled
        if (settings.otaEnabled == "on" || settings.otaEnabled == "true") {
          settings.otaEnabled = "off";
          Serial.println("OTA also disabled");
        }
      }
    }
    
    // FW Version (debug only)
    if (settings.debugEnabled == "on" || settings.debugEnabled == "true") {
      if (request->hasParam("fw_version", true)) {
        String newFwVersion = request->getParam("fw_version", true)->value();
        newFwVersion.trim();
        if (newFwVersion != settings.firmwareVersion) {
          settings.firmwareVersion = newFwVersion;
          configChanged = true;
          Serial.println("FW version changed to: " + settings.firmwareVersion);
        }
      }
      
      // FS Version (debug only)
      if (request->hasParam("fs_version", true)) {
        String newFsVersion = request->getParam("fs_version", true)->value();
        newFsVersion.trim();
        if (newFsVersion != settings.filesystemVersion) {
          settings.filesystemVersion = newFsVersion;
          configChanged = true;
          Serial.println("FS version changed to: " + settings.filesystemVersion);
        }
      }
    }
    
    // Check OTA checkbox (only if debug is enabled)
    if (settings.debugEnabled == "on" || settings.debugEnabled == "true") {
      if (request->hasParam("ota", true)) {
        if (settings.otaEnabled != "on" && settings.otaEnabled != "true") {
          settings.otaEnabled = "on";
          configChanged = true;
          Serial.println("OTA enabled");
        }
      } else {
        if (settings.otaEnabled == "on" || settings.otaEnabled == "true") {
          settings.otaEnabled = "off";
          configChanged = true;
          Serial.println("OTA disabled");
        }
      }
    }
    
    // Check updates checkbox
    if (request->hasParam("updates", true)) {
      if (settings.updatesEnabled != "on" && settings.updatesEnabled != "true") {
        settings.updatesEnabled = "on";
        configChanged = true;
        Serial.println("Software updates enabled");
      }
    } else {
      if (settings.updatesEnabled == "on" || settings.updatesEnabled == "true") {
        settings.updatesEnabled = "off";
        configChanged = true;
        Serial.println("Software updates disabled");
      }
    }

    // Check NTP checkbox
    if (request->hasParam("ntp", true)) {
      if (settings.ntpEnabled != "on" && settings.ntpEnabled != "true") {
        settings.ntpEnabled = "on";
        configChanged = true;
        Serial.println("NTP sync enabled");
      }
    } else {
      if (settings.ntpEnabled == "on" || settings.ntpEnabled == "true") {
        settings.ntpEnabled = "off";
        configChanged = true;
        Serial.println("NTP sync disabled");
      }
    }
    
    // Timezone
    if (request->hasParam("timezone", true)) {
      String newTz = request->getParam("timezone", true)->value();
      newTz.trim();
      if (newTz != settings.timezone) {
        settings.timezone = newTz;
        configChanged = true;
        Serial.println("Timezone changed to: " + settings.timezone);
      }
    }
    
    // Update URL
    if (request->hasParam("updateurl", true)) {
      String newUpdateUrl = request->getParam("updateurl", true)->value();
      newUpdateUrl.trim();
      if (newUpdateUrl != settings.updateUrl) {
        settings.updateUrl = newUpdateUrl;
        configChanged = true;
        Serial.println("Update URL changed to: " + settings.updateUrl);
      }
    }
    
    // GitHub Token
    if (request->hasParam("githubtoken", true)) {
      String newToken = request->getParam("githubtoken", true)->value();
      newToken.trim();
      if (newToken != settings.githubToken) {
        settings.githubToken = newToken;
        configChanged = true;
        Serial.println("GitHub token updated");
      }
    }
    
    // Save config changes if needed
    if (configChanged) {
      settings.save();
      if (!shouldReboot) {
        syncTimeIfEnabled(false);  // false = not boot sync, don't save boot time
      }
    }
    
    // Set default message if reboot needed but no message set
    if (shouldReboot && message.isEmpty()) {
      message = "Settings saved";
    }
    
    // Send confirmation page
    if (shouldReboot) {
      request->send(LittleFS, "/confirm.html", "text/html", false, [message, shouldReboot](const String& var) -> String {
        if (var == "MESSAGE") {
          return message;
        }
        if (var == "MESSAGE_CLASS") {
          return shouldReboot ? "text-error" : "";
        }
        if (var == "RELOAD_BUTTON") {
          if (shouldReboot) {
            return "<div class='form-actions-right'><input type='button' value='Done' onclick='window.location.href=\"/settings\";' class='btn-small btn-width-100'></div>";
          }
          return "";
        }
        return String();
      });
      
      if (shouldReboot) {
        // Schedule reboot after 2 seconds to allow browser to receive response
        rebootScheduled = true;
        rebootTime = millis();
      }
    } else {
      // No changes - just redirect back
      request->redirect("/settings");
    }
  });
  
  // Reboot endpoint
  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("Manual reboot requested");
    request->send(200, "text/plain", "Rebooting...");
    rebootScheduled = true;
    rebootTime = millis();
  });
  
  // Reset WiFi settings
  server.on("/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("WiFi reset requested");
    
    // Clear WiFi credentials from NVS
    preferences.begin("config", false);
    preferences.remove("ssid");
    preferences.remove("pass");
    preferences.remove("ip");
    preferences.remove("gateway");
    preferences.remove("netmask");
    preferences.remove("dhcp");
    preferences.end();
    
    request->send(200, "text/plain", "WiFi settings reset");
    
    // Schedule reboot
    rebootScheduled = true;
    rebootTime = millis();
  });
}

// ============================================================================
// I2C API ROUTES - Scan, LED control, bootloader, register dump
// ============================================================================

// I2C slave register addresses (for test_i2c_slave.cpp on Arduino)
#define SLAVE_REG_LED_ONOFF_WEB    0x10  // 0=off, 1=on
#define SLAVE_REG_LED_BLINK_WEB    0x11  // 0=off, 1=1Hz, 2=4Hz
#define SLAVE_REG_LED_STATUS_WEB   0x20  // Read status
#define SLAVE_REG_ENTER_BOOT_WEB   0x99  // Bootloader with safety code
#define SLAVE_BOOT_MAGIC_WEB       0xB0

static void registerI2CApiRoutes(AsyncWebServer& server) {
  // API: Get Twiboot bootloader status
  // IMPORTANT: Only use ping() to detect bootloader at 0x14.
  // Do NOT call queryBootloaderVersion() here - it sends command byte 0x01
  // which equals CMD_SWITCH_APPLICATION in Twiboot and kicks the bootloader out!
  server.on("/api/twi/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    
    I2CManager& manager = I2CManager::getInstance();
    
    if (manager.ping(0x14, I2C_BUS_SLAVE)) {
      doc["connected"] = true;
      doc["signature"] = "1E 95 0F";  // ATmega328P signature
      doc["version"] = "twiboot";    // Don't query - it would send exit command
    } else {
      doc["connected"] = false;
      
      if (manager.ping(0x30, I2C_BUS_SLAVE)) {
        doc["appConnected"] = true;
        doc["hint"] = "Arduino in normal mode. Click 'Enter Bootloader' to activate firmware update mode.";
      } else {
        doc["appConnected"] = false;
        doc["hint"] = "Arduino not responding on either 0x30 (app) or 0x14 (bootloader)";
      }
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  // API: Upload hex file to bootloader
  server.on("/api/twi/upload", HTTP_POST, 
    [](AsyncWebServerRequest *request) {}, 
    nullptr, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      // Accumulate data in chunks
      static String uploadBuffer;
      
      // First chunk - reset buffer
      if (index == 0) {
        uploadBuffer = "";
        uploadBuffer.reserve(total + 1);
        Serial.println("[Twiboot API] Upload started, total size: " + String(total) + " bytes");
      }
      
      // Append chunk to buffer
      for (size_t i = 0; i < len; i++) {
        uploadBuffer += (char)data[i];
      }
      
      Serial.printf("[Twiboot API] Received chunk: %d/%d bytes (%.1f%%)\n", 
                   index + len, total, ((index + len) * 100.0) / total);
      
      // Only process when we have all data
      if (index + len != total) {
        return;
      }
      
      Serial.println("[Twiboot API] All data received, processing...");
      
      if (!md11SlaveUpdater) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Twiboot not initialized\"}");
        uploadBuffer = "";
        return;
      }
      
      // Parse JSON body with hex content
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, uploadBuffer);
      
      if (error) {
        Serial.println("[Twiboot API] JSON parse error: " + String(error.c_str()));
        Serial.println("[Twiboot API] Buffer length: " + String(uploadBuffer.length()));
        Serial.println("[Twiboot API] First 100 chars: " + uploadBuffer.substring(0, 100));
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        uploadBuffer = "";
        return;
      }
      
      String hexContent = doc["hexContent"].as<String>();
      uploadBuffer = "";  // Free memory
      
      if (hexContent.length() == 0) {
        Serial.println("[Twiboot API] Empty hex content");
        request->send(400, "application/json", "{\"success\":false,\"error\":\"No hex content\"}");
        return;
      }
      
      Serial.println("[Twiboot API] Hex content size: " + String(hexContent.length()) + " bytes");
      
      // NOTE: Do NOT query bootloader version here - it may kick bootloader out of programming mode!
      // User must click "Enter Bootloader" button first to activate bootloader.
      Serial.println("[Twiboot API] Starting firmware upload (bootloader must already be active)...");
      
      // Upload hex file
      if (!md11SlaveUpdater->uploadHexFile(hexContent)) {
        String error = md11SlaveUpdater->getLastError();
        Serial.println("[Twiboot API] Upload failed: " + error);
        request->send(500, "application/json", 
          "{\"success\":false,\"error\":\"" + error + "\"}");
        return;
      }
      
      Serial.println("[Twiboot API] Upload successful!");
      request->send(200, "application/json", "{\"success\":true,\"message\":\"Firmware updated\"}");
    }
  );

  // API: Scan I2C bus
  server.on("/api/i2c/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool debugEnabledBool = Settings::stringToBool(settings.debugEnabled);
    
    if (!debugEnabledBool) {
      request->send(403, "application/json", "{\"error\":\"Debug mode required\"}");
      return;
    }
    
    JsonDocument doc;
    
    // Scan Bus 0 (Display Bus - GPIO8/9 @ 100kHz)
    JsonArray bus0Devices = doc["bus0"]["devices"].to<JsonArray>();
    doc["bus0"]["name"] = "Bus 0: Display (GPIO8/9 @ 100kHz)";
    doc["bus0"]["speed"] = "100 kHz";
    doc["bus0"]["pins"] = "GPIO8(SDA), GPIO9(SCL)";
    
    for (uint8_t address = 0x03; address < 0x78; address++) {
      if (I2CManager::getInstance().ping(address, I2C_BUS_DISPLAY)) {
        JsonObject device = bus0Devices.add<JsonObject>();
        
        char hexAddr[5];
        sprintf(hexAddr, "0x%02X", address);
        device["address"] = hexAddr;
        device["decimal"] = address;
        device["bus"] = 0;
        
        // Add device name if known
        String deviceName = "Unknown";
        if (address == 0x3C || address == 0x3D) deviceName = "SSD1306 OLED Display";
        else if (address == 0x27 || address == 0x3F) deviceName = "PCF8574 LCD 16x2";
        else if (address == 0x36) deviceName = "Seesaw Rotary Encoder";
        else if (address == 0x38) deviceName = "AHT10 Temperature & Humidity Sensor";
        else if (address == 0x76 || address == 0x77) deviceName = "BMP280/BME280 Sensor";
        else if (address == 0x68) deviceName = "MPU6050/DS3231 RTC";
        else if (address == 0x48) deviceName = "ADS1115 ADC";
        else if (address == 0x20) deviceName = "PCF8574 I/O Expander";
        
        device["name"] = deviceName;
      }
      yield();
    }
    doc["bus0"]["count"] = bus0Devices.size();
    
    // Scan Bus 1 (Slave Bus - GPIO5/6 @ 100kHz)
    JsonArray bus1Devices = doc["bus1"]["devices"].to<JsonArray>();
    doc["bus1"]["name"] = "Bus 1: Slave (GPIO5/6 @ 100kHz)";
    doc["bus1"]["speed"] = "100 kHz";
    doc["bus1"]["pins"] = "GPIO5(SDA), GPIO6(SCL)";
    
    for (uint8_t address = 0x03; address < 0x78; address++) {
      if (I2CManager::getInstance().ping(address, I2C_BUS_SLAVE)) {
        JsonObject device = bus1Devices.add<JsonObject>();
        
        char hexAddr[5];
        sprintf(hexAddr, "0x%02X", address);
        device["address"] = hexAddr;
        device["decimal"] = address;
        device["bus"] = 1;
        
        // Add device name if known
        String deviceName = "Unknown";
        if (address == 0x30) deviceName = "MS11 Slave Controller (ATmega328P)";
        else if (address == 0x14) deviceName = "Twiboot Bootloader (ATmega328P)";
        
        device["name"] = deviceName;
      }
      yield();
    }
    doc["bus1"]["count"] = bus1Devices.size();
    
    // Total summary
    doc["totalDevices"] = bus0Devices.size() + bus1Devices.size();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // API: I2C LED control via test_i2c_slave.cpp registers
  server.on("/api/i2c/led", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool debugEnabledBool = Settings::stringToBool(settings.debugEnabled);
    if (!debugEnabledBool) {
      request->send(403, "application/json", "{\"error\":\"Debug mode required\"}");
      return;
    }

    // Get action from POST body or query params
    String action = "";
    if (request->hasParam("action", true)) {
      action = request->getParam("action", true)->value();
    } else if (request->hasParam("action")) {
      action = request->getParam("action")->value();
    }
    
    if (action.length() == 0) {
      request->send(400, "application/json", "{\"error\":\"Missing action parameter\"}");
      return;
    }
    
    // Map action to LED control commands
    uint8_t reg = SLAVE_REG_LED_ONOFF_WEB;
    uint8_t val = 0;
    String statusText = "";
    
    if (action == "on") {
      reg = SLAVE_REG_LED_ONOFF_WEB;
      val = 1;
      statusText = "LED on";
    } else if (action == "blink1") {
      reg = SLAVE_REG_LED_BLINK_WEB;
      val = 1;
      statusText = "Blinking 1 Hz";
    } else if (action == "blink4") {
      reg = SLAVE_REG_LED_BLINK_WEB;
      val = 2;
      statusText = "Blinking 4 Hz";
    } else if (action == "blink0") {
      reg = SLAVE_REG_LED_BLINK_WEB;
      val = 0;
      statusText = "Blink stopped";
    } else {
      request->send(400, "application/json", "{\"error\":\"Invalid action\"}");
      return;
    }

    uint8_t cmdBuffer[2] = {reg, val};
    
    Serial.printf("[API] LED command: action=%s, REG=0x%02X, VAL=0x%02X\n", 
                  action.c_str(), reg, val);

    I2CManager& manager = I2CManager::getInstance();
    if (manager.write(0x30, cmdBuffer, 2)) {
      JsonDocument doc;
      doc["success"] = true;
      doc["action"] = action;
      doc["statusText"] = statusText;
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
    } else {
      JsonDocument doc;
      doc["success"] = false;
      doc["error"] = "I2C write failed: " + manager.getLastError();
      String response;
      serializeJson(doc, response);
      request->send(500, "application/json", response);
    }
  });

  // API: Enter bootloader mode via test_i2c_slave.cpp register protocol
  server.on("/api/i2c/bootloader", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool debugEnabledBool = Settings::stringToBool(settings.debugEnabled);
    if (!debugEnabledBool) {
      request->send(403, "application/json", "{\"error\":\"Debug mode required\"}");
      return;
    }

    JsonDocument doc;
    
    uint8_t bootCmd[2] = {SLAVE_REG_ENTER_BOOT_WEB, SLAVE_BOOT_MAGIC_WEB};
    
    Serial.printf("[API] Sending bootloader command to 0x30: REG=0x%02X, VAL=0x%02X\n", 
                  bootCmd[0], bootCmd[1]);

    I2CManager& manager = I2CManager::getInstance();
    if (!manager.write(0x30, bootCmd, 2)) {
      Serial.println("[API] Failed to send bootloader command via I2C");
      doc["success"] = false;
      doc["error"] = "Failed to send bootloader command: " + manager.getLastError();
      String response;
      serializeJson(doc, response);
      request->send(500, "application/json", response);
      return;
    }
    
    Serial.println("[API] Bootloader command sent. Arduino will reboot into bootloader.");
    
    doc["success"] = true;
    doc["message"] = "Bootloader command sent (Arduino rebooting)";
    doc["bootloaderVersion"] = "activating...";
    doc["hint"] = "Bootloader startup takes ~5 seconds. Polling will detect when ready.";
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // API: Exit bootloader mode - sends CMD_SWITCH_APPLICATION + BOOTTYPE_APPLICATION to Twiboot
  server.on("/api/i2c/exit-bootloader", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool debugEnabledBool = Settings::stringToBool(settings.debugEnabled);
    if (!debugEnabledBool) {
      request->send(403, "application/json", "{\"error\":\"Debug mode required\"}");
      return;
    }

    JsonDocument doc;
    I2CManager& manager = I2CManager::getInstance();
    
    uint8_t exitCmd[2] = {0x01, 0x80};
    if (manager.write(0x14, exitCmd, 2)) {
      doc["success"] = true;
      doc["message"] = "Bootloader exit command sent";
      doc["hint"] = "Arduino will return to application mode";
      Serial.println("[API] Bootloader exit command sent (0x01 + 0x80)");
    } else {
      doc["success"] = false;
      doc["error"] = "Failed to send exit command - bootloader not responding at 0x14";
      Serial.println("[API] ERROR: Bootloader exit command failed");
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // API: Reset Arduino - exits bootloader if active, otherwise reports status
  server.on("/api/i2c/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool debugEnabledBool = Settings::stringToBool(settings.debugEnabled);
    if (!debugEnabledBool) {
      request->send(403, "application/json", "{\"error\":\"Debug mode required\"}");
      return;
    }

    I2CManager& manager = I2CManager::getInstance();
    JsonDocument doc;
    
    bool bootloaderActive = manager.ping(0x14, I2C_BUS_SLAVE);
    bool appActive = manager.ping(0x30, I2C_BUS_SLAVE);
    
    if (bootloaderActive) {
      uint8_t exitCmd[2] = {0x01, 0x80};
      manager.write(0x14, exitCmd, 2);
      Serial.println("[API] Reset: sent exit bootloader command (0x01+0x80)");
      delay(500);
      
      bool appNow = manager.ping(0x30, I2C_BUS_SLAVE);
      doc["success"] = true;
      doc["message"] = appNow ? "Arduino reset - app running" : "Exit command sent, app starting...";
    } else if (appActive) {
      doc["success"] = true;
      doc["message"] = "Arduino app is already running (address 0x30)";
      Serial.println("[API] Reset: app already running at 0x30");
    } else {
      doc["success"] = false;
      doc["error"] = "Arduino not responding on 0x30 (app) or 0x14 (bootloader)";
      Serial.println("[API] Reset: no response on either address");
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // API: Get device register dump
  server.on("/api/i2c/registers", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool debugEnabledBool = Settings::stringToBool(settings.debugEnabled);
    
    if (!debugEnabledBool) {
      request->send(403, "application/json", "{\"error\":\"Debug mode required\"}");
      return;
    }
    
    if (!request->hasParam("address")) {
      request->send(400, "application/json", "{\"error\":\"Missing address parameter\"}");
      return;
    }
    
    if (!request->hasParam("bus")) {
      request->send(400, "application/json", "{\"error\":\"Missing bus parameter\"}");
      return;
    }
    
    uint8_t address = request->getParam("address")->value().toInt();
    uint8_t busNum = request->getParam("bus")->value().toInt();
    I2CBus bus = (busNum == 0) ? I2C_BUS_DISPLAY : I2C_BUS_SLAVE;
    
    JsonDocument doc;
    JsonArray registers = doc["registers"].to<JsonArray>();
    
    unsigned long scanStart = millis();
    int errorCount = 0;
    unsigned long responseStart = 0;
    unsigned long responseTime = 0;
    
    // Test device response
    responseStart = micros();
    bool devicePresent = I2CManager::getInstance().ping(address, bus);
    responseTime = (micros() - responseStart) / 1000;
    
    if (devicePresent) {
      for (uint16_t reg = 0; reg < 256; reg++) {
        uint8_t value = 0xFF;
        
        bool success = false;
        if (bus == I2C_BUS_DISPLAY) {
          uint8_t regAddr = (uint8_t)reg;
          if (I2CManager::getInstance().displayWrite(address, &regAddr, 1, 50)) {
            success = I2CManager::getInstance().displayRead(address, &value, 1, 50);
          }
        } else {
          success = I2CManager::getInstance().readRegister(address, (uint8_t)reg, value, 50);
        }
        
        if (!success) {
          value = 0xFF;
          errorCount++;
        }
        
        registers.add(value);
        yield();
      }
    } else {
      errorCount = 256;
    }
    
    unsigned long scanDuration = millis() - scanStart;
    
    // Metrics
    doc["scanDuration"] = scanDuration;
    doc["responseTime"] = responseTime;
    doc["busSpeed"] = 100;
    doc["errors"] = errorCount;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
}

// ============================================================================
// UPDATE API ROUTES - OTA firmware/filesystem updates via GitHub
// ============================================================================

static void registerUpdateApiRoutes(AsyncWebServer& server) {
  // API: Get update status
  server.on("/api/update/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = githubUpdater->handleStatusRequest(
      settings.firmwareVersion, 
      settings.filesystemVersion,
      Settings::stringToBool(settings.updatesEnabled),
      Settings::stringToBool(settings.debugEnabled),
      (settings.githubToken.length() > 0)
    );
    request->send(200, "application/json", response);
  });
  
  // API: Check for updates
  server.on("/api/update/check", HTTP_POST, [](AsyncWebServerRequest *request) {
    String response = githubUpdater->handleCheckRequest(
      settings.updateUrl, settings.githubToken, 
      settings.firmwareVersion, settings.filesystemVersion
    );
    request->send(200, "application/json", response);
  });
  
  // API: Install update
  server.on("/api/update/install", HTTP_POST, [](AsyncWebServerRequest *request) {
    String type = "both";
    if (request->hasParam("type", true)) {
      type = request->getParam("type", true)->value();
    }
    
    bool shouldReboot = false;
    String response = githubUpdater->handleInstallRequest(
      type, settings.githubToken,
      settings.firmwareVersion, settings.filesystemVersion,
      shouldReboot
    );
    
    request->send(200, "application/json", response);
    
    if (shouldReboot) {
      Serial.println("Update successful, rebooting in 1 second...");
      delay(1000);
      ESP.restart();
    }
  });
  
  // API: Reinstall current version (debug only)
  server.on("/api/update/reinstall", HTTP_POST, [](AsyncWebServerRequest *request) {
    String type = "both";
    if (request->hasParam("type", true)) {
      type = request->getParam("type", true)->value();
    }
    
    bool shouldReboot = false;
    String response = githubUpdater->handleReinstallRequest(
      type, settings.githubToken,
      settings.firmwareVersion, settings.filesystemVersion,
      Settings::stringToBool(settings.debugEnabled),
      shouldReboot
    );
    
    request->send(200, "application/json", response);
    
    if (shouldReboot) {
      Serial.println("Reinstall successful, rebooting in 1 second...");
      delay(1000);
      ESP.restart();
    }
  });
}

// ============================================================================
// FILE API ROUTES - LittleFS file manager
// ============================================================================

static void registerFileApiRoutes(AsyncWebServer& server) {
  // API: List all files
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    bool first = true;
    
    while (file) {
      if (!file.isDirectory()) {
        if (!first) json += ",";
        String filename = String(file.name());
        if (!filename.startsWith("/")) filename = "/" + filename;
        json += "{\"name\":\"" + filename + "\",\"size\":" + String(file.size()) + "}";
        first = false;
      }
      file = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
  });
  
  // API: Read file
  server.on("/api/file", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("path")) {
      String path = request->getParam("path")->value();
      if (!path.startsWith("/")) path = "/" + path;
      if (LittleFS.exists(path)) {
        request->send(LittleFS, path, "text/plain");
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "Missing path parameter");
    }
  });
  
  // API: Write file
  server.on("/api/file", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("path", true) && request->hasParam("content", true)) {
      String path = request->getParam("path", true)->value();
      if (!path.startsWith("/")) path = "/" + path;
      String content = request->getParam("content", true)->value();
      
      File file = LittleFS.open(path, "w");
      if (file) {
        file.print(content);
        file.close();
        request->send(200, "text/plain", "File saved");
      } else {
        request->send(500, "text/plain", "Error writing file");
      }
    } else {
      request->send(400, "text/plain", "Missing parameters");
    }
  });
  
  // API: Delete file
  server.on("/api/file", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (request->hasParam("path")) {
      String path = request->getParam("path")->value();
      if (!path.startsWith("/")) path = "/" + path;
      if (LittleFS.exists(path)) {
        if (LittleFS.remove(path)) {
          request->send(200, "text/plain", "File deleted");
        } else {
          request->send(500, "text/plain", "Error deleting file");
        }
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "Missing path parameter");
    }
  });
  
  // API: Upload file
  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200);
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile;
    
    if (index == 0) {
      String path = "/" + filename;
      uploadFile = LittleFS.open(path, "w");
    }
    
    if (uploadFile) {
      uploadFile.write(data, len);
    }
    
    if (final) {
      if (uploadFile) {
        uploadFile.close();
      }
    }
  });
}

// ============================================================================
// AP MODE ROUTES - Captive portal for WiFi configuration
// ============================================================================

void registerAPRoutes(AsyncWebServer& server, DNSServer& dnsServer) {
  // Web Server Root URL - Captive Portal
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/wifimanager.html", "text/html");
  });

  // Catch-all for captive portal - redirect everything to root
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(LittleFS, "/wifimanager.html", "text/html");
  });
  
  // WiFi scan endpoint - return cached results
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    unsigned long currentTime = millis();
    
    // If no cached results or cache expired, trigger async scan
    if (cachedScanResults.isEmpty() || (currentTime - lastScanTime > WIFI_SCAN_CACHE_INTERVAL)) {
      if (!scanInProgress) {
        scanInProgress = true;
        WiFi.scanNetworks(true);
        lastScanTime = currentTime;
      }
      
      // Check if scan completed
      int n = WiFi.scanComplete();
      if (n >= 0) {
        String json = "{\"networks\":[";
        for (int i = 0; i < n; ++i) {
          if (i) json += ",";
          json += "{";
          json += "\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\"";
          json += ",\"rssi\":" + String(WiFi.RSSI(i));
          json += ",\"encryption\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
          json += "}";
        }
        json += "]}";
        cachedScanResults = json;
        WiFi.scanDelete();
        scanInProgress = false;
      } else if (n == WIFI_SCAN_RUNNING) {
        if (cachedScanResults.isEmpty()) {
          request->send(200, "application/json", "{\"networks\":[],\"scanning\":true}");
          return;
        }
      }
    }
    
    request->send(200, "application/json", cachedScanResults);
  });
  
  // WiFi configuration form POST handler
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    bool settingsChanged = false;
    for(int i=0;i<params;i++){
      const AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        if (p->name() == PARAM_INPUT_1) {
          settings.ssid = p->value().c_str();
          Serial.print("SSID set to: ");
          Serial.println(settings.ssid);
          settingsChanged = true;
        }
        if (p->name() == PARAM_INPUT_2) {
          settings.password = p->value().c_str();
          Serial.println("Password updated (not logged for security)");
          preferences.begin("config", false);
          preferences.putString("pass", settings.password);
          preferences.end();
        }
        if (p->name() == PARAM_INPUT_3) {
          settings.ip = p->value().c_str();
          Serial.print("IP Address set to: ");
          Serial.println(settings.ip);
          settingsChanged = true;
        }
        if (p->name() == PARAM_INPUT_4) {
          settings.gateway = p->value().c_str();
          Serial.print("Gateway set to: ");
          Serial.println(settings.gateway);
          settingsChanged = true;
        }
        if (p->name() == PARAM_INPUT_5) {
          settings.useDHCP = p->value().c_str();
          Serial.print("DHCP set to: ");
          Serial.println(settings.useDHCP);
          settingsChanged = true;
        }
      }
    }
    // If DHCP checkbox was not checked, it won't be in POST data
    bool dhcpChecked = false;
    for(int i=0;i<params;i++){
      const AsyncWebParameter* p = request->getParam(i);
      if(p->isPost() && p->name() == PARAM_INPUT_5){
        dhcpChecked = true;
        break;
      }
    }
    if(!dhcpChecked){
      settings.useDHCP = "false";
      settingsChanged = true;
      Serial.println("DHCP set to: false");
    }
    
    if (settingsChanged) {
      settings.saveNetwork();
    }
    
    String responseMsg = "Done. ESP will restart and connect to your router";
    if (settings.useDHCP == "on" || settings.useDHCP == "true") {
      responseMsg += " using DHCP.";
    } else {
      responseMsg += " using IP address: " + settings.ip;
    }
    request->send(200, "text/plain", responseMsg);
    delay(3000);
    ESP.restart();
  });
  
  // Serve static files (CSS, images, etc.) - must be last
  server.serveStatic("/", LittleFS, "/");
}
