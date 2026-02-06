#include <gpio_viewer.h> // Must be the first include in your project
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <SSD1306.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include "images.h"
#include "config.h"
#include "settings.h"
#include "github_updater.h"
#include "wifi_manager.h"

// Global instances
Preferences preferences;

// GPIO Viewer pointer (only instantiated if enabled)
GPIOViewer* gpio_viewer = nullptr;

// OLED display instance (using config.h constants)
SSD1306 display(OLED_I2C_ADDRESS, OLED_SDA_PIN, OLED_SCL_PIN);

// GitHub updater instance
GitHubUpdater* githubUpdater = nullptr;

// WiFi manager instance
WiFiManager* wifiManager = nullptr;

// Create AsyncWebServer object
AsyncWebServer server(WEB_SERVER_PORT);

// DNS server for captive portal
DNSServer dnsServer;

// WiFi scan cache
String cachedScanResults = "";
unsigned long lastScanTime = 0;
bool scanInProgress = false;

// Compatibility layer - reference settings module variables
// These allow existing code to work without changes
String& ssid = settings.ssid;
String& pass = settings.password;
String& ip = settings.ip;
String& gateway = settings.gateway;
String& netmask = settings.netmask;
String& useDHCP = settings.useDHCP;
String& debugEnabled = settings.debugEnabled;
String& githubToken = settings.githubToken;
String& gpioViewerEnabled = settings.gpioViewerEnabled;
String& otaEnabled = settings.otaEnabled;
String& updatesEnabled = settings.updatesEnabled;
String& updateUrl = settings.updateUrl;
String& ntpEnabled = settings.ntpEnabled;

// Version tracking aliases
String& currentFirmwareVersion = settings.firmwareVersion;
String& currentFilesystemVersion = settings.filesystemVersion;

// Track if we're in AP mode
bool isAPMode = false;

// IP display timer
unsigned long ipDisplayTime = 0;
bool ipDisplayShown = false;
bool ipDisplayCleared = false;

// Delayed reboot timer
unsigned long rebootTime = 0;
bool rebootScheduled = false;

// Forward declaration
void performReboot();

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true, LITTLEFS_BASE_PATH, LITTLEFS_MAX_FILES, LITTLEFS_PARTITION_LABEL)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// Escape string for JSON
String jsonEscape(String str) {
  String result = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c == '"') {
      result += "\\\"";
    } else if (c == '\\') {
      result += "\\\\";
    } else if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c == '\t') {
      result += "\\t";
    } else {
      result += c;
    }
  }
  return result;
}

// NTP time sync (UTC)
void syncTimeIfEnabled() {
  if (!Settings::stringToBool(ntpEnabled)) {
    Serial.println("NTP sync disabled");
    return;
  }

  Serial.println("Starting NTP sync...");
  configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

  const unsigned long start = millis();
  time_t now = 0;
  while (now < NTP_VALID_TIME && (millis() - start) < NTP_SYNC_TIMEOUT) {
    time(&now);
    delay(100);
  }

  if (now >= NTP_VALID_TIME) {
    Serial.println(String("NTP time synced: ") + ctime(&now));
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    settings.saveStoredDateIfNeeded(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  } else {
    Serial.println("NTP sync timeout");
    Settings::StoredDate stored = settings.getStoredDate();
    if (stored.valid) {
      struct tm fallbackTime = {};
      fallbackTime.tm_year = stored.year - 1900;
      fallbackTime.tm_mon = stored.month - 1;
      fallbackTime.tm_mday = stored.day;
      fallbackTime.tm_hour = 0;
      fallbackTime.tm_min = 0;
      fallbackTime.tm_sec = 0;
      time_t fallback = mktime(&fallbackTime);
      if (fallback > 0) {
        struct timeval tv = { .tv_sec = fallback, .tv_usec = 0 };
        settimeofday(&tv, nullptr);
        Serial.println(String("NTP failed; using stored date: ") + stored.year + "-" + stored.month + "-" + stored.day);
      }
    }
  }
}


void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  // Initialize OLED display
  display.init();
  display.normalDisplay();  // Ensure normal display mode
  display.setFont(ArialMT_Plain_10);
  display.clear();
  
  // Show Waacs logo
  display.drawXbm(11, 16, 105, 21, Waacs_Logo_bits);
  display.display();
  delay(2000);
  display.clear();
  display.display();

  initLittleFS();

  // Initialize NVS with defaults on first boot
  settings.initialize();

  // Load values from NVS
  settings.load();
  
  // Sync versions with compile-time defines
  settings.syncVersions();
  
  // Print loaded settings
  settings.print();
  
  // Initialize GitHub updater
  githubUpdater = new GitHubUpdater(preferences, display);
  githubUpdater->loadUpdateInfo();
  
  Serial.println("OTA Update System Initialized");
  Serial.println("Firmware Version: " + currentFirmwareVersion);
  Serial.println("Filesystem Version: " + currentFilesystemVersion);
  
  if (useDHCP != "true" && useDHCP != "on") {
    Serial.println("IP: " + ip);
    Serial.println("Gateway: " + gateway);
  }

  // Initialize WiFi manager
  wifiManager = new WiFiManager(preferences);
  bool isDHCP = (useDHCP == "true" || useDHCP == "on");
  
  if(wifiManager->begin(ssid, pass, ip, gateway, netmask, isDHCP, WIFI_CONNECT_TIMEOUT)) {
    // WiFi connected successfully
    Serial.println("WiFi connected!");
    
    // Show WiFi logo on OLED
    display.clear();
    display.drawXbm(34, 14, 60, 36, WiFi_Logo_bits);
    display.display();
    delay(1000);
    
    // Show IP address + versions
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "IP: " + WiFi.localIP().toString());
    // Line 2 intentionally left blank
    display.drawString(0, 28, "fw-" + currentFirmwareVersion);
    display.drawString(0, 42, "fs-" + currentFilesystemVersion);
    display.display();
    
    // Start timer to clear display after 3 seconds
    ipDisplayTime = millis();
    ipDisplayShown = true;
    ipDisplayCleared = false;

    // Sync time if enabled
    syncTimeIfEnabled();
    
    // Initialize GPIO Viewer if enabled
    if (gpioViewerEnabled == "true" || gpioViewerEnabled == "on") {
      gpio_viewer = new GPIOViewer();
      gpio_viewer->setSamplingInterval(500);  // 500ms to reduce CPU usage
      gpio_viewer->setPort(5555);
      gpio_viewer->begin();
      Serial.println("GPIO Viewer started on port 5555");
    }
    
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html");
    });
    
    // Route for settings page with template processor
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
      String gpioViewerStatus = gpioViewerEnabled;
      bool gpioEnabled = (gpioViewerStatus == "true" || gpioViewerStatus == "on");
      String updatesStatus = updatesEnabled;
      bool updatesEnabledBool = (updatesStatus == "true" || updatesStatus == "on");
      String otaStatus = otaEnabled;
      bool otaEnabledBool = (otaStatus == "true" || otaStatus == "on");
      bool dhcpEnabledBool = (useDHCP == "true" || useDHCP == "on");
      bool debugEnabledBool = (debugEnabled == "true" || debugEnabled == "on");
      bool ntpEnabledBool = (ntpEnabled == "true" || ntpEnabled == "on");
      
      request->send(LittleFS, "/settings.html", "text/html", false, [gpioEnabled, updatesEnabledBool, otaEnabledBool, dhcpEnabledBool, debugEnabledBool, ntpEnabledBool](const String& var) -> String {
        if (var == "SSID") {
          return ssid;
        }
        if (var == "PASSWORD") {
          return "";  // Don't return actual password, use placeholder instead
        }
        if (var == "IP_ADDRESS") {
          return ip;
        }
        if (var == "GATEWAY") {
          return gateway;
        }
        if (var == "NETMASK") {
          return netmask;
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
          return currentFirmwareVersion;
        }
        if (var == "FS_VERSION") {
          return currentFilesystemVersion;
        }
        if (var == "GPIOVIEWER_CHECKED") {
          return gpioEnabled ? "checked" : "";
        }
        if (var == "GPIOVIEWER_BUTTON") {
          if (gpioEnabled) {
            String ipAddr = WiFi.localIP().toString();
            return "<a href=\"http://" + ipAddr + ":5555\" target=\"_blank\" class=\"btn-small btn-open\">Open</a>";
          }
          return "";
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
          return updateUrl;
        }
        if (var == "GITHUB_TOKEN") {
          return githubToken;
        }
        if (var == "FILE_MANAGER_VISIBILITY") {
          return debugEnabledBool ? "style=\"visibility: visible;\"" : "style=\"visibility: hidden;\"";
        }
        return String();
      });
    });
    
    // Handle settings POST
    server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
      bool shouldReboot = false;
      bool gpioDisabled = false;
      bool configChanged = false;
      String message = "";
      
      Serial.println("Settings POST received");
      
      // SSID
      if (request->hasParam("ssid", true)) {
        String newSsid = request->getParam("ssid", true)->value();
        newSsid.trim();
        if (newSsid.length() > 0 && newSsid != ssid) {
          ssid = newSsid;
          configChanged = true;
          shouldReboot = true;
          Serial.println("SSID changed to: " + ssid);
        }
      }
      
      // Password
      if (request->hasParam("password", true)) {
        String newPass = request->getParam("password", true)->value();
        if (newPass.length() > 0 && newPass != pass) {
          pass = newPass;
          configChanged = true;
          shouldReboot = true;
          // Save password to NVS
          preferences.begin("config", false);
          preferences.putString("pass", pass);
          preferences.end();
          Serial.println("Password changed");
        }
      }
      
      // DHCP checkbox
      String newDhcp = request->hasParam("dhcp", true) ? "on" : "off";
      String currentDhcp = (useDHCP == "true" || useDHCP == "on") ? "on" : "off";
      if (newDhcp != currentDhcp) {
        useDHCP = newDhcp;
        configChanged = true;
        shouldReboot = true;
        Serial.println("DHCP changed to: " + useDHCP);
      }
      
      // Network settings
      if (request->hasParam("ip", true)) {
        String newIp = request->getParam("ip", true)->value();
        newIp.trim();
        if (newIp != ip) {
          ip = newIp;
          configChanged = true;
          shouldReboot = true;
          Serial.println("IP changed to: " + ip);
        }
      }
      
      if (request->hasParam("gateway", true)) {
        String newGateway = request->getParam("gateway", true)->value();
        newGateway.trim();
        if (newGateway != gateway) {
          gateway = newGateway;
          configChanged = true;
          shouldReboot = true;
          Serial.println("Gateway changed to: " + gateway);
        }
      }
      
      if (request->hasParam("netmask", true)) {
        String newNetmask = request->getParam("netmask", true)->value();
        newNetmask.trim();
        if (newNetmask != netmask) {
          netmask = newNetmask;
          configChanged = true;
          shouldReboot = true;
          Serial.println("Netmask changed to: " + netmask);
        }
      }
      
      // Read current GPIO Viewer setting
      String currentGpioViewerSetting = gpioViewerEnabled;
      
      // Check debug checkbox
      if (request->hasParam("debug", true)) {
        // Checkbox is checked - enable debug
        if (debugEnabled != "on" && debugEnabled != "true") {
          debugEnabled = "on";
          configChanged = true;
          Serial.println("Debug options enabled");
        }
      } else {
        // Checkbox is not checked - disable debug
        if (debugEnabled == "on" || debugEnabled == "true") {
          debugEnabled = "off";
          configChanged = true;
          Serial.println("Debug options disabled");
          
          // Also disable GPIO Viewer when debug is disabled
          if (currentGpioViewerSetting == "on" || currentGpioViewerSetting == "true") {
            gpioViewerEnabled = "off";
            Serial.println("GPIO Viewer also disabled - powercycle required");
            message = "Powercycle device now!";
            gpioDisabled = true;
          }
          
          // Also disable OTA when debug is disabled
          if (otaEnabled == "on" || otaEnabled == "true") {
            otaEnabled = "off";
            Serial.println("OTA also disabled");
          }
        }
      }
      
      // FW Version
      if (request->hasParam("fw_version", true)) {
        String newFwVersion = request->getParam("fw_version", true)->value();
        newFwVersion.trim();
        if (newFwVersion != currentFirmwareVersion) {
          currentFirmwareVersion = newFwVersion;
          configChanged = true;
          Serial.println("FW version changed to: " + currentFirmwareVersion);
        }
      }
      
      // FS Version
      if (request->hasParam("fs_version", true)) {
        String newFsVersion = request->getParam("fs_version", true)->value();
        newFsVersion.trim();
        if (newFsVersion != currentFilesystemVersion) {
          currentFilesystemVersion = newFsVersion;
          configChanged = true;
          Serial.println("FS version changed to: " + currentFilesystemVersion);
        }
      }
      
      // Check gpioviewer checkbox (only if debug is enabled)
      if (debugEnabled == "on" || debugEnabled == "true") {
        if (request->hasParam("gpioviewer", true)) {
          // Checkbox is checked - enable GPIO Viewer
          if (currentGpioViewerSetting != "on" && currentGpioViewerSetting != "true") {
            gpioViewerEnabled = "on";
            configChanged = true;
            Serial.println("GPIO Viewer enabled - rebooting...");
            message = "GPIO Viewer enabled. Rebooting.";
            shouldReboot = true;
          }
        } else {
          // Checkbox is not checked - disable GPIO Viewer
          if (currentGpioViewerSetting == "on" || currentGpioViewerSetting == "true") {
            gpioViewerEnabled = "off";
            configChanged = true;
            Serial.println("GPIO Viewer disabled - powercycle required");
            message = "GPIO Viewer disabled. Powercycle device.";
            gpioDisabled = true;
          }
        }
        
        // Check OTA checkbox (only if debug is enabled)
        if (request->hasParam("ota", true)) {
          // Checkbox is checked - enable OTA
          if (otaEnabled != "on" && otaEnabled != "true") {
            otaEnabled = "on";
            configChanged = true;
            Serial.println("OTA enabled");
          }
        } else {
          // Checkbox is not checked - disable OTA
          if (otaEnabled == "on" || otaEnabled == "true") {
            otaEnabled = "off";
            configChanged = true;
            Serial.println("OTA disabled");
          }
        }
      }
      
      // Check updates checkbox
      if (request->hasParam("updates", true)) {
        // Checkbox is checked - enable updates
        if (updatesEnabled != "on" && updatesEnabled != "true") {
          updatesEnabled = "on";
          configChanged = true;
          Serial.println("Software updates enabled");
        }
      } else {
        // Checkbox is not checked - disable updates
        if (updatesEnabled == "on" || updatesEnabled == "true") {
          updatesEnabled = "off";
          configChanged = true;
          Serial.println("Software updates disabled");
        }
      }

      // Check NTP checkbox
      if (request->hasParam("ntp", true)) {
        if (ntpEnabled != "on" && ntpEnabled != "true") {
          ntpEnabled = "on";
          configChanged = true;
          Serial.println("NTP sync enabled");
        }
      } else {
        if (ntpEnabled == "on" || ntpEnabled == "true") {
          ntpEnabled = "off";
          configChanged = true;
          Serial.println("NTP sync disabled");
        }
      }
      
      // Update URL
      if (request->hasParam("updateurl", true)) {
        String newUpdateUrl = request->getParam("updateurl", true)->value();
        newUpdateUrl.trim();
        if (newUpdateUrl != updateUrl) {
          updateUrl = newUpdateUrl;
          configChanged = true;
          Serial.println("Update URL changed to: " + updateUrl);
        }
      }
      
      // GitHub Token
      if (request->hasParam("githubtoken", true)) {
        String newToken = request->getParam("githubtoken", true)->value();
        newToken.trim();
        if (newToken != githubToken) {
          githubToken = newToken;
          configChanged = true;
          Serial.println("GitHub token updated");
        }
      }
      
      // Save config changes if needed
      if (configChanged) {
        settings.save();
        if (!shouldReboot) {
          syncTimeIfEnabled();
        }
      }
      
      // Set default message if reboot needed but no message set
      if (shouldReboot && message.isEmpty()) {
        message = "Settings saved";
      }
      
      // Send confirmation page
      if (shouldReboot || gpioDisabled) {
        request->send(LittleFS, "/confirm.html", "text/html", false, [message, shouldReboot, gpioDisabled](const String& var) -> String {
          if (var == "MESSAGE") {
            return message;
          }
          if (var == "MESSAGE_CLASS") {
            return (gpioDisabled || shouldReboot) ? "text-error" : "";
          }
          if (var == "RELOAD_BUTTON") {
            if (shouldReboot) {
              return "<div class='form-actions-right'><input type='button' value='Done' onclick='window.location.href=\"/settings\";' class='btn-small btn-width-100'></div>";
            } else if (gpioDisabled) {
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
      bool debugEnabledBool = (debugEnabled == "true" || debugEnabled == "on");
      
      request->send(LittleFS, "/i2c.html", "text/html", false, [debugEnabledBool](const String& var) -> String {
        if (var == "DEBUG_ENABLED") {
          return debugEnabledBool ? "true" : "false";
        }
        return String();
      });
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
    
    // API: Scan I2C bus
    server.on("/api/i2c/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
      bool debugEnabledBool = (debugEnabled == "true" || debugEnabled == "on");
      
      if (!debugEnabledBool) {
        request->send(403, "application/json", "{\"error\":\"Debug mode required\"}");
        return;
      }
      
      JsonDocument doc;
      JsonArray devices = doc["devices"].to<JsonArray>();
      
      // Scan I2C bus (addresses 0x03 to 0x77)
      for (uint8_t address = 0x03; address < 0x78; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
          // Device found
          JsonObject device = devices.add<JsonObject>();
          
          char hexAddr[5];
          sprintf(hexAddr, "0x%02X", address);
          device["address"] = hexAddr;
          device["decimal"] = address;
          
          // Add device name if known
          String deviceName = "Unknown";
          if (address == 0x3C || address == 0x3D) deviceName = "SSD1306 OLED";
          else if (address == 0x76 || address == 0x77) deviceName = "BMP280/BME280";
          else if (address == 0x68) deviceName = "MPU6050/DS3231";
          else if (address == 0x48) deviceName = "ADS1115";
          else if (address == 0x20) deviceName = "PCF8574";
          
          device["name"] = deviceName;
        }
      }
      
      doc["count"] = devices.size();
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
    });
    
    // API: Get device register dump
    server.on("/api/i2c/registers", HTTP_GET, [](AsyncWebServerRequest *request) {
      bool debugEnabledBool = (debugEnabled == "true" || debugEnabled == "on");
      
      if (!debugEnabledBool) {
        request->send(403, "application/json", "{\"error\":\"Debug mode required\"}");
        return;
      }
      
      if (!request->hasParam("address")) {
        request->send(400, "application/json", "{\"error\":\"Missing address parameter\"}");
        return;
      }
      
      uint8_t address = request->getParam("address")->value().toInt();
      
      JsonDocument doc;
      JsonArray registers = doc["registers"].to<JsonArray>();
      
      unsigned long scanStart = millis();
      int errorCount = 0;
      unsigned long responseStart = 0;
      unsigned long responseTime = 0;
      
      // Test device response
      responseStart = micros();
      Wire.beginTransmission(address);
      uint8_t testError = Wire.endTransmission();
      responseTime = (micros() - responseStart) / 1000;
      
      if (testError == 0) {
        // Device responded, try to read registers
        for (uint16_t reg = 0; reg < 256; reg++) {
          Wire.beginTransmission(address);
          Wire.write(reg);
          uint8_t error = Wire.endTransmission(false); // Keep connection open
          
          if (error == 0) {
            Wire.requestFrom(address, (uint8_t)1);
            if (Wire.available()) {
              registers.add(Wire.read());
            } else {
              registers.add(0xFF); // No data available
              errorCount++;
            }
          } else {
            registers.add(0xFF); // Error reading
            errorCount++;
          }
          
          yield(); // Prevent watchdog timeout
        }
      } else {
        errorCount = 256;
      }
      
      unsigned long scanDuration = millis() - scanStart;
      
      // Metrics
      doc["scanDuration"] = scanDuration;
      doc["responseTime"] = responseTime;
      doc["busSpeed"] = 100; // Default I2C speed (can be read from Wire if needed)
      doc["errors"] = errorCount;
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
    });
    
    // API: Get update status
    server.on("/api/update/status", HTTP_GET, [](AsyncWebServerRequest *request) {
      settings.syncVersions();
      String response = githubUpdater->handleStatusRequest(
        currentFirmwareVersion, 
        currentFilesystemVersion,
        (updatesEnabled == "on" || updatesEnabled == "true"),
        (debugEnabled == "on" || debugEnabled == "true"),
        (githubToken.length() > 0)
      );
      request->send(200, "application/json", response);
    });
    
    // API: Check for updates
    server.on("/api/update/check", HTTP_POST, [](AsyncWebServerRequest *request) {
      String response = githubUpdater->handleCheckRequest(
        updateUrl, githubToken, 
        currentFirmwareVersion, currentFilesystemVersion
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
        type, githubToken,
        currentFirmwareVersion, currentFilesystemVersion,
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
        type, githubToken,
        currentFirmwareVersion, currentFilesystemVersion,
        (debugEnabled == "on" || debugEnabled == "true"),
        shouldReboot
      );
      
      request->send(200, "application/json", response);
      
      if (shouldReboot) {
        Serial.println("Reinstall successful, rebooting in 1 second...");
        delay(1000);
        ESP.restart();
      }
    });
    
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
        // Start of upload
        String path = "/" + filename;
        uploadFile = LittleFS.open(path, "w");
      }
      
      if (uploadFile) {
        uploadFile.write(data, len);
      }
      
      if (final) {
        // End of upload
        if (uploadFile) {
          uploadFile.close();
        }
      }
    });
    
    // Serve static files (CSS, images, etc.) - must be last
    server.serveStatic("/", LittleFS, "/");
    
    // Start ArduinoOTA if enabled
    if (otaEnabled == "on" || otaEnabled == "true") {
      ArduinoOTA.setHostname("ESP32-Base");
      ArduinoOTA.begin();
      Serial.println("ArduinoOTA started");
    } else {
      Serial.println("ArduinoOTA disabled");
    }
    
    server.begin();
    Serial.println("Web server started");
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    isAPMode = true;
    
    // Reset WiFi state properly before starting AP mode
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Show connecting message on OLED
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 16, "WiFi - Manager");
    display.display();
    
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", IP);
    Serial.println("DNS server started for captive portal");
    
    // Start WiFi scan immediately so results are ready when portal opens
    WiFi.scanNetworks(true);
    scanInProgress = true;
    lastScanTime = millis();
    Serial.println("Initial WiFi scan started");

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
          WiFi.scanNetworks(true); // Start async scan
          lastScanTime = currentTime;
        }
        
        // Check if scan completed
        int n = WiFi.scanComplete();
        if (n >= 0) {
          // Scan completed, build JSON
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
          // Scan still in progress, return empty or old cache
          if (cachedScanResults.isEmpty()) {
            request->send(200, "application/json", "{\"networks\":[],\"scanning\":true}");
            return;
          }
        }
      }
      
      // Return cached results
      request->send(200, "application/json", cachedScanResults);
    });
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      bool settingsChanged = false;
      for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            settingsChanged = true;
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.println("Password updated (not logged for security)");
            // Save password to NVS
            preferences.begin("config", false);
            preferences.putString("pass", pass);
            preferences.end();
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            settingsChanged = true;
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            settingsChanged = true;
          }
          // HTTP POST dhcp value
          if (p->name() == PARAM_INPUT_5) {
            useDHCP = p->value().c_str();
            Serial.print("DHCP set to: ");
            Serial.println(useDHCP);
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
        useDHCP = "false";
        settingsChanged = true;
        Serial.println("DHCP set to: false");
      }
      
      // Save all network settings to config file
      if (settingsChanged) {
        settings.saveNetwork();
      }
      
      String responseMsg = "Done. ESP will restart and connect to your router";
      if (useDHCP == "on" || useDHCP == "true") {
        responseMsg += " using DHCP.";
      } else {
        responseMsg += " using IP address: " + ip;
      }
      request->send(200, "text/plain", responseMsg);
      delay(3000);
      ESP.restart();
    });
    
    // Serve static files (CSS, images, etc.) - must be last
    server.serveStatic("/", LittleFS, "/");
    
    server.begin();
  }  // End of else (AP mode)
}  // End of setup()

// ============================================================================
// LOOP HELPER FUNCTIONS
// ============================================================================

// Handle display tasks (IP display timeout, etc.)
void handleDisplayTasks() {
  // Non-blocking IP display clear after timeout
  if (ipDisplayShown && !ipDisplayCleared && (millis() - ipDisplayTime > DISPLAY_IP_SHOW_DURATION)) {
    display.clear();
    display.display();
    ipDisplayCleared = true;
  }
}

// Handle network tasks (DNS, OTA, WiFi)
void handleNetworkTasks() {
  // Process DNS requests only in AP mode for captive portal
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  
  // Handle ArduinoOTA updates (only if enabled and not in AP mode)
  if (!isAPMode && Settings::stringToBool(settings.otaEnabled)) {
    ArduinoOTA.handle();
  }
}

// Handle system tasks (scheduled reboots)
void handleSystemTasks() {
  // Handle scheduled reboot after delay
  if (rebootScheduled && (millis() - rebootTime > REBOOT_DELAY)) {
    performReboot();
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  handleDisplayTasks();
  handleSystemTasks();
  handleNetworkTasks();
  
  // Small delay to prevent excessive CPU usage
  delay(LOOP_DELAY);
}

// Function to show reboot message and restart
void performReboot() {
  Serial.println("Showing reboot message on display");
  display.clear();
  display.invertDisplay();  // Yellow background
  display.drawString(0, 26, "Rebooting...");
  display.display();
  Serial.println("Reboot message displayed");
  delay(2000);
  Serial.println("Executing restart...");
  ESP.restart();
}
