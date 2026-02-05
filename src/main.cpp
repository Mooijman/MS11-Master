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
#include "images.h"
#include "config.h"
#include "settings.h"

// Update state enum
enum UpdateState {
  UPDATE_IDLE,
  UPDATE_CHECKING,
  UPDATE_AVAILABLE,
  UPDATE_DOWNLOADING,
  UPDATE_INSTALLING,
  UPDATE_SUCCESS,
  UPDATE_ERROR
};

// Update info structure
struct UpdateInfo {
  UpdateState state;
  String remoteVersion;
  String remoteLittlefsVersion;
  String firmwareUrl;
  String littlefsUrl;
  bool firmwareAvailable;
  bool littlefsAvailable;
  unsigned long lastCheck;
  String lastError;
  int downloadProgress;
  bool filesystemUpdateDone;  // Flag to indicate filesystem update completed (requires manual reboot)
};

// Global instances
UpdateInfo updateInfo;
Preferences preferences;

// Background update check timing
unsigned long lastBackgroundCheck = 0;
int checksToday = 0;
unsigned long dayStartTime = 0;

// GPIO Viewer pointer (only instantiated if enabled)
GPIOViewer* gpio_viewer = nullptr;

// OLED display instance (using config.h constants)
SSD1306 display(OLED_I2C_ADDRESS, OLED_SDA_PIN, OLED_SCL_PIN);

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
String& storedFirmwareVersion = settings.firmwareVersion;
String& storedFilesystemVersion = settings.filesystemVersion;

// Aliases for version tracking (used in some functions)
String& currentFirmwareVersion = settings.firmwareVersion;
String& currentFilesystemVersion = settings.filesystemVersion;

IPAddress localIP;
IPAddress localGateway;
IPAddress localSubnet;

// Timer variables
unsigned long previousMillis = 0;
const long interval = WIFI_CONNECT_TIMEOUT;  // WiFi connection timeout

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

// Compare versions (format: 2026-1.0.01)
// Returns true if remote is newer than current
bool compareVersions(String remoteVer, String currentVer) {
  if (remoteVer.length() == 0 || currentVer.length() == 0) return false;
  
  // Strip prefixes for comparison: fw-, fs-
  String remote = remoteVer;
  String current = currentVer;
  
  // Remove version prefixes (fw- and fs- only, no v prefix expected)
  if (remote.startsWith("fw-")) remote = remote.substring(3);    // fw-2026-1.0.07 → 2026-1.0.07
  if (remote.startsWith("fs-")) remote = remote.substring(3);    // fs-2026-1.0.07 → 2026-1.0.07
  
  if (current.startsWith("fw-")) current = current.substring(3);
  if (current.startsWith("fs-")) current = current.substring(3);
  
  // Format: 2026-1.0.01 (NO v prefix)
  // Simply compare strings (lexicographic comparison works for this format)
  return remote > current;
}

// Save update info to NVS
void saveUpdateInfo() {
  preferences.begin(NVS_NAMESPACE_OTA, false);
  preferences.putString("remoteVer", updateInfo.remoteVersion);
  preferences.putString("remoteLFS", updateInfo.remoteLittlefsVersion);
  preferences.putString("fwUrl", updateInfo.firmwareUrl);
  preferences.putString("fsUrl", updateInfo.littlefsUrl);
  preferences.putULong("lastCheck", updateInfo.lastCheck);
  preferences.putString("lastError", updateInfo.lastError);
  preferences.putBool("fwAvail", updateInfo.firmwareAvailable);
  preferences.putBool("lfsAvail", updateInfo.littlefsAvailable);
  preferences.end();
}

// Load update info from NVS
void loadUpdateInfo() {
  preferences.begin(NVS_NAMESPACE_OTA, true);
  updateInfo.remoteVersion = preferences.getString("remoteVer", "");
  updateInfo.remoteLittlefsVersion = preferences.getString("remoteLFS", "");
  updateInfo.firmwareUrl = preferences.getString("fwUrl", "");
  updateInfo.littlefsUrl = preferences.getString("fsUrl", "");
  updateInfo.lastCheck = preferences.getULong("lastCheck", 0);
  updateInfo.lastError = preferences.getString("lastError", "");
  updateInfo.firmwareAvailable = preferences.getBool("fwAvail", false);
  updateInfo.littlefsAvailable = preferences.getBool("lfsAvail", false);
  preferences.end();
  updateInfo.state = UPDATE_IDLE;
  updateInfo.downloadProgress = 0;
}

// Check for updates via GitHub API
bool checkGitHubRelease() {
  if (!updateUrl || updateUrl.length() == 0) {
    Serial.println("No update URL configured");
    return false;
  }
  
  updateInfo.state = UPDATE_CHECKING;
  updateInfo.lastError = "";
  
  HTTPClient http;
  WiFiClientSecure client;
  
  // For GitHub, we use setInsecure (production should use certificate)
  client.setInsecure();
  
  // GitHub API URL format: https://api.github.com/repos/OWNER/REPO/releases/latest
  String apiUrl = updateUrl;
  if (!apiUrl.startsWith("http")) {
    Serial.println("Invalid update URL");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Invalid URL format";
    return false;
  }
  
  Serial.println("Checking for updates at: " + apiUrl);
  
  http.begin(client, apiUrl);
  http.addHeader("User-Agent", "ESP32-OTA-Client");
  
  // Add GitHub token for private repository access
  if (githubToken && githubToken.length() > 0) {
    http.addHeader("Authorization", "token " + githubToken);
    Serial.println("Using GitHub token for authentication");
  }
  
  http.setTimeout(15000); // 15 second timeout
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    // Parse JSON response
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.println("JSON parsing failed: " + String(error.c_str()));
      updateInfo.state = UPDATE_ERROR;
      updateInfo.lastError = "JSON parse error";
      http.end();
      return false;
    }
    
    // Extract version and URLs
    String remoteVersion = doc["tag_name"].as<String>();
    JsonArray assets = doc["assets"];
    
    updateInfo.remoteVersion = remoteVersion;
    updateInfo.firmwareUrl = "";
    updateInfo.littlefsUrl = "";
    
    // Look for firmware and filesystem assets
    // For private repos, use the API asset URL
    // Asset names: fw-VERSION.bin and fs-VERSION.bin
    for (JsonObject asset : assets) {
      String name = asset["name"].as<String>();
      String downloadUrl = asset["url"].as<String>();  // This is the API URL
      
      if (name.startsWith("fw-") && name.endsWith(".bin")) {
        updateInfo.firmwareUrl = downloadUrl;
        Serial.println("Found firmware: " + downloadUrl);
      } else if (name.startsWith("fs-") && name.endsWith(".bin")) {
        updateInfo.littlefsUrl = downloadUrl;
        Serial.println("Found filesystem: " + downloadUrl);
      }
    }
    
    // Compare versions - check if updates are available based on files present
    // If fw.bin is present and version is newer, firmware update available
    // If fs.bin is present and version is newer, filesystem update available
    updateInfo.firmwareAvailable = !updateInfo.firmwareUrl.isEmpty() && 
                                    compareVersions(remoteVersion, currentFirmwareVersion);
    updateInfo.littlefsAvailable = !updateInfo.littlefsUrl.isEmpty() && 
                                    compareVersions(remoteVersion, currentFilesystemVersion);
    
    if (updateInfo.firmwareAvailable || updateInfo.littlefsAvailable) {
      updateInfo.state = UPDATE_AVAILABLE;
      Serial.println("Update available: " + remoteVersion);
      Serial.println("Current firmware: " + currentFirmwareVersion);
      Serial.println("Current filesystem: " + currentFilesystemVersion);
      if (updateInfo.firmwareAvailable) {
        Serial.println("→ Firmware update available");
      }
      if (updateInfo.littlefsAvailable) {
        Serial.println("→ Filesystem update available");
      }
    } else {
      updateInfo.state = UPDATE_IDLE;
      Serial.println("No update needed. Remote: " + remoteVersion + ", Current: " + currentFirmwareVersion);
    }
    
    updateInfo.lastCheck = millis();
    saveUpdateInfo();
    
    http.end();
    return true;
    
  } else {
    Serial.println("HTTP request failed: " + String(httpCode));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "HTTP " + String(httpCode);
    http.end();
    return false;
  }
}

// Download and install firmware
bool downloadAndInstallFirmware(String url) {
  if (url.length() == 0) {
    Serial.println("No firmware URL available");
    return false;
  }
  
  // Display update message on OLED - FW specific
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Updating FW");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 30, "Please Wait...");
  display.drawString(0, 45, "DO NOT POWER OFF");
  display.display();
  
  updateInfo.state = UPDATE_DOWNLOADING;
  updateInfo.downloadProgress = 0;
  
  HTTPClient http;
  WiFiClientSecure client;
  
  client.setInsecure();
  
  Serial.println("Downloading firmware from: " + url);
  Serial.println("Token available: " + String(githubToken.length() > 0 ? "YES" : "NO"));
  
  http.begin(client, url);
  http.setTimeout(60000); // 60 second timeout for download
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // Follow redirects
  
  // Add authorization and accept headers for GitHub API
  if (githubToken.length() > 0) {
    http.addHeader("Authorization", "token " + githubToken);
  }
  http.addHeader("Accept", "application/octet-stream");
  
  int httpCode = http.GET();
  Serial.println("HTTP response code: " + String(httpCode));
  
  if (httpCode != 200 && httpCode != 302) {
    Serial.println("Download failed: " + String(httpCode));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Download failed HTTP " + String(httpCode);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("Invalid content length");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Invalid content length";
    http.end();
    return false;
  }
  
  Serial.printf("Firmware size: %d bytes\n", contentLength);
  
  updateInfo.state = UPDATE_INSTALLING;
  
  // Start OTA update
  if (!Update.begin(contentLength, U_FLASH)) {
    Serial.println("Update.begin failed: " + String(Update.errorString()));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = Update.errorString();
    http.end();
    return false;
  }
  
  // Get stream and write
  WiFiClient * stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buff[1024];
  
  while (http.connected() && (written < contentLength)) {
    size_t available = stream->available();
    
    if (available) {
      int c = stream->readBytes(buff, min(available, sizeof(buff)));
      
      if (c > 0) {
        if (Update.write(buff, c) != c) {
          Serial.println("Write failed");
          Update.abort();
          updateInfo.state = UPDATE_ERROR;
          updateInfo.lastError = "Write failed";
          http.end();
          return false;
        }
        written += c;
        updateInfo.downloadProgress = (written * 100) / contentLength;
        
        // Reset watchdog timers
        esp_task_wdt_reset();
        yield();
        
        if (written % 10240 == 0) { // Log every 10KB
          Serial.printf("Progress: %d%%\n", updateInfo.downloadProgress);
        }
      }
    }
    esp_task_wdt_reset();
    delay(1);
  }
  
  http.end();
  
  if (written != contentLength) {
    Serial.println("Written bytes mismatch");
    Update.abort();
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Size mismatch";
    return false;
  }
  
  if (!Update.end()) {
    Serial.println("Update.end failed: " + String(Update.errorString()));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = Update.errorString();
    return false;
  }
  
  if (!Update.isFinished()) {
    Serial.println("Update not finished");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Update incomplete";
    return false;
  }
  
  Serial.println("Firmware update successful!");
  updateInfo.state = UPDATE_SUCCESS;
  updateInfo.downloadProgress = 100;
  
  // Display success message on OLED
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 15, "Update done");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 40, "Powercycle now");
  display.display();
  
  // Update version in current.info (strip fw- prefix if present)
  String newVersion = updateInfo.remoteVersion;
  if (newVersion.startsWith("fw-")) {
    newVersion = newVersion.substring(3);
  }
  currentFirmwareVersion = newVersion;
  settings.updateVersions();
  
  return true;
}

// Download and install LittleFS
bool downloadAndInstallLittleFS(String url) {
  if (url.length() == 0) {
    Serial.println("No LittleFS URL available");
    return false;
  }
  
  // Display update message on OLED - FS specific
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Updating FS");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 30, "Please Wait...");
  display.drawString(0, 45, "DO NOT POWER OFF");
  display.display();
  
  updateInfo.state = UPDATE_DOWNLOADING;
  updateInfo.downloadProgress = 0;
  
  HTTPClient http;
  WiFiClientSecure client;
  
  client.setInsecure();
  
  Serial.println("=== DOWNLOAD DEBUG ===");
  Serial.println("LittleFS URL: " + url);
  Serial.println("Token (first 10 chars): " + (githubToken.length() > 0 ? githubToken.substring(0, min(10, (int)githubToken.length())) : "EMPTY"));
  Serial.println("=====================");
  
  http.begin(client, url);
  http.setTimeout(60000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // Follow redirects
  
  // Add authorization and accept headers for GitHub API
  if (githubToken.length() > 0) {
    http.addHeader("Authorization", "token " + githubToken);
    Serial.println("Added Authorization header");
  }
  http.addHeader("Accept", "application/octet-stream");
  Serial.println("Added Accept header");
  
  int httpCode = http.GET();
  Serial.println("HTTP response code: " + String(httpCode));
  
  if (httpCode != 200 && httpCode != 302) {
    Serial.println("Download failed: " + String(httpCode));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS download failed HTTP " + String(httpCode);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("Invalid content length");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Invalid LFS content length";
    http.end();
    return false;
  }
  
  Serial.printf("LittleFS size: %d bytes\n", contentLength);
  
  updateInfo.state = UPDATE_INSTALLING;
  
  // Start LittleFS update (use U_SPIFFS type for filesystem partition)
  if (!Update.begin(contentLength, U_SPIFFS)) {
    Serial.println("LittleFS Update.begin failed: " + String(Update.errorString()));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS " + String(Update.errorString());
    http.end();
    return false;
  }
  
  // Get stream and write
  WiFiClient * stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buff[1024];
  
  while (http.connected() && (written < contentLength)) {
    size_t available = stream->available();
    
    if (available) {
      int c = stream->readBytes(buff, min(available, sizeof(buff)));
      
      if (c > 0) {
        if (Update.write(buff, c) != c) {
          Serial.println("LittleFS Write failed");
          Update.abort();
          updateInfo.state = UPDATE_ERROR;
          updateInfo.lastError = "LFS write failed";
          http.end();
          return false;
        }
        written += c;
        updateInfo.downloadProgress = (written * 100) / contentLength;
        
        // Reset watchdog timers
        esp_task_wdt_reset();
        yield();
        
        if (written % 10240 == 0) {
          Serial.printf("LittleFS Progress: %d%%\n", updateInfo.downloadProgress);
        }
      }
    }
    esp_task_wdt_reset();
    delay(1);
  }
  
  http.end();
  
  if (written != contentLength) {
    Serial.println("LittleFS Written bytes mismatch");
    Update.abort();
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS size mismatch";
    return false;
  }
  
  if (!Update.end()) {
    Serial.println("LittleFS Update.end failed: " + String(Update.errorString()));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS " + String(Update.errorString());
    return false;
  }
  
  if (!Update.isFinished()) {
    Serial.println("LittleFS Update not finished");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS update incomplete";
    return false;
  }
  
  Serial.println("LittleFS update successful!");
  updateInfo.state = UPDATE_SUCCESS;
  updateInfo.downloadProgress = 100;
  
  // Display success message on OLED
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 15, "Update done");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 40, "Powercycle now");
  display.display();
  
  // Update version in current.info (strip fw- prefix if present)
  String newVersion = updateInfo.remoteVersion;
  if (newVersion.startsWith("fw-")) {
    newVersion = newVersion.substring(3);
  }
  currentFilesystemVersion = newVersion;
  settings.updateVersions();
  
  return true;
}

// Initialize WiFi
bool initWiFi() {
  if(ssid==""){
    Serial.println("Undefined SSID.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  
  // Check if DHCP or static IP
  bool isDHCP = (useDHCP == "true" || useDHCP == "on");
  
  if (!isDHCP) {
    // Static IP configuration
    if(ip==""){
      Serial.println("Static IP selected but no IP address defined.");
      return false;
    }
    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    localSubnet.fromString(netmask.c_str());

    if (!WiFi.config(localIP, localGateway, localSubnet)){
      Serial.println("STA Failed to configure");
      return false;
    }
    Serial.println("Using Static IP");
  } else {
    Serial.println("Using DHCP");
  }
  
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      // Clear saved credentials so AP mode starts on next reboot
      preferences.begin("config", false);
      preferences.remove("ssid");
      preferences.remove("pass");
      preferences.end();
      return false;
    }
    delay(10); // Prevent busy-wait, allow other tasks to run
  }

  Serial.println(WiFi.localIP());
  return true;
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  // Initialize OLED display
  display.init();
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
  
  // Initialize OTA update system
  loadUpdateInfo();
  
  Serial.println("OTA Update System Initialized");
  Serial.println("Firmware Version: " + currentFirmwareVersion);
  Serial.println("Filesystem Version: " + currentFilesystemVersion);
  
  if (useDHCP != "true" && useDHCP != "on") {
    Serial.println("IP: " + ip);
    Serial.println("Gateway: " + gateway);
  }

  if(initWiFi()) {
    // WiFi connected successfully
    Serial.println("WiFi connected!");
    
    // Show WiFi logo on OLED
    display.clear();
    display.drawXbm(34, 14, 60, 36, WiFi_Logo_bits);
    display.display();
    delay(1000);
    
    // Show IP address
    display.clear();
    display.drawString(0, 16, "IP: " + WiFi.localIP().toString());
    display.display();
    
    // Start timer to clear display after 3 seconds
    ipDisplayTime = millis();
    ipDisplayShown = true;
    ipDisplayCleared = false;
    
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
      
      request->send(LittleFS, "/settings.html", "text/html", false, [gpioEnabled, updatesEnabledBool, otaEnabledBool, dhcpEnabledBool, debugEnabledBool](const String& var) -> String {
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
          return storedFirmwareVersion;
        }
        if (var == "FS_VERSION") {
          return storedFilesystemVersion;
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
        if (newFwVersion != storedFirmwareVersion) {
          storedFirmwareVersion = newFwVersion;
          configChanged = true;
          Serial.println("FW version changed to: " + storedFirmwareVersion);
        }
      }
      
      // FS Version
      if (request->hasParam("fs_version", true)) {
        String newFsVersion = request->getParam("fs_version", true)->value();
        newFsVersion.trim();
        if (newFsVersion != storedFilesystemVersion) {
          storedFilesystemVersion = newFsVersion;
          configChanged = true;
          Serial.println("FS version changed to: " + storedFilesystemVersion);
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
    
    // API: Get update status
    server.on("/api/update/status", HTTP_GET, [](AsyncWebServerRequest *request) {
      // Reload current versions (sync with compile-time versions)
      settings.syncVersions();
      
      JsonDocument doc;
      
      doc["currentFirmwareVersion"] = currentFirmwareVersion;
      doc["currentFilesystemVersion"] = currentFilesystemVersion;
      doc["updatesEnabled"] = (updatesEnabled == "on" || updatesEnabled == "true");
      doc["debugEnabled"] = (debugEnabled == "on" || debugEnabled == "true");
      doc["hasGithubToken"] = (githubToken.length() > 0);
      doc["remoteVersion"] = updateInfo.remoteVersion;
      doc["state"] = updateInfo.state;
      doc["firmwareAvailable"] = updateInfo.firmwareAvailable;
      doc["littlefsAvailable"] = updateInfo.littlefsAvailable;
      doc["availableFirmwareVersion"] = updateInfo.remoteVersion;  // Same as remoteVersion for both
      doc["availableFilesystemVersion"] = updateInfo.remoteVersion;
      doc["lastCheck"] = updateInfo.lastCheck;
      doc["lastError"] = updateInfo.lastError;
      doc["downloadProgress"] = updateInfo.downloadProgress;
      doc["filesystemUpdateDone"] = updateInfo.filesystemUpdateDone;
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
    });
    
    // API: Check for updates
    server.on("/api/update/check", HTTP_POST, [](AsyncWebServerRequest *request) {
      Serial.println("=== UPDATE CHECK REQUESTED ===");
      bool success = checkGitHubRelease();
      
      DynamicJsonDocument doc(256);
      doc["success"] = success;
      doc["message"] = success ? "Check completed" : "Check failed";
      doc["updateAvailable"] = updateInfo.firmwareAvailable || updateInfo.littlefsAvailable;
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
    });
    
    // API: Install update
    server.on("/api/update/install", HTTP_POST, [](AsyncWebServerRequest *request) {
      Serial.println("=== UPDATE INSTALL REQUESTED ===");
      String type = "both";
      
      if (request->hasParam("type", true)) {
        type = request->getParam("type", true)->value();
      }
      
      Serial.println("Install type: " + type);
      Serial.println("Firmware available: " + String(updateInfo.firmwareAvailable));
      Serial.println("LittleFS available: " + String(updateInfo.littlefsAvailable));
      
      bool success = false;
      String message = "";
      bool isFilesystemUpdate = false;
      
      if (type == "firmware" && updateInfo.firmwareAvailable) {
        success = downloadAndInstallFirmware(updateInfo.firmwareUrl);
        message = success ? "Firmware installed" : "Firmware install failed";
      } else if (type == "littlefs" && updateInfo.littlefsAvailable) {
        success = downloadAndInstallLittleFS(updateInfo.littlefsUrl);
        message = success ? "LittleFS installed" : "LittleFS install failed";
        isFilesystemUpdate = true;
      } else if (type == "both") {
        if (updateInfo.firmwareAvailable) {
          success = downloadAndInstallFirmware(updateInfo.firmwareUrl);
          if (!success) {
            message = "Firmware install failed";
          }
        }
        if (success && updateInfo.littlefsAvailable) {
          success = downloadAndInstallLittleFS(updateInfo.littlefsUrl);
          if (!success) {
            message = "LittleFS install failed";
          } else {
            message = "Both installed";
          }
          isFilesystemUpdate = true;
        } else if (success) {
          message = "Firmware installed";
        }
      }
      
      // Set filesystem update flag
      if (success && isFilesystemUpdate) {
        updateInfo.filesystemUpdateDone = true;
      }
      
      DynamicJsonDocument doc(256);
      doc["success"] = success;
      doc["message"] = message;
      doc["rebootRequired"] = success && !isFilesystemUpdate;  // Only auto-reboot for firmware updates
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
      
      // Only restart if firmware was updated (not filesystem)
      if (success && !isFilesystemUpdate) {
        delay(2000);
        ESP.restart();
      }
      // If filesystem was updated, system halts here - user must power cycle
    });
    
    // API: Reinstall current version (debug only)
    server.on("/api/update/reinstall", HTTP_POST, [](AsyncWebServerRequest *request) {
      Serial.println("=== REINSTALL REQUESTED ===");
      
      // Only allow if debug is enabled
      if (debugEnabled != "on" && debugEnabled != "true") {
        request->send(403, "application/json", "{\"success\":false,\"message\":\"Debug mode required\"}");
        return;
      }
      
      String type = "both";
      if (request->hasParam("type", true)) {
        type = request->getParam("type", true)->value();
      }
      
      Serial.println("Reinstall type: " + type);
      
      // Use the current GitHub URLs if we have them, otherwise construct from stored version
      String fwUrl = updateInfo.firmwareUrl;
      String fsUrl = updateInfo.littlefsUrl;
      
      bool success = false;
      String message = "";
      bool isFilesystemUpdate = false;
      
      if (type == "firmware" && !fwUrl.isEmpty()) {
        success = downloadAndInstallFirmware(fwUrl);
        message = success ? "Firmware reinstalled" : "Firmware reinstall failed";
      } else if (type == "littlefs" && !fsUrl.isEmpty()) {
        success = downloadAndInstallLittleFS(fsUrl);
        message = success ? "LittleFS reinstalled" : "LittleFS reinstall failed";
        isFilesystemUpdate = true;
      } else if (type == "both") {
        if (!fwUrl.isEmpty()) {
          success = downloadAndInstallFirmware(fwUrl);
          if (!success) {
            message = "Firmware reinstall failed";
          }
        }
        if (success && !fsUrl.isEmpty()) {
          success = downloadAndInstallLittleFS(fsUrl);
          if (!success) {
            message = "LittleFS reinstall failed";
          } else {
            message = "Both reinstalled";
          }
          isFilesystemUpdate = true;
        } else if (success) {
          message = "Firmware reinstalled";
        }
      } else {
        message = "No update URLs available. Check for updates first.";
      }
      
      // Set filesystem update flag
      if (success && isFilesystemUpdate) {
        updateInfo.filesystemUpdateDone = true;
      }
      
      DynamicJsonDocument doc(256);
      doc["success"] = success;
      doc["message"] = message;
      doc["rebootRequired"] = success && !isFilesystemUpdate;
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
      
      // Only restart if firmware was updated (not filesystem)
      if (success && !isFilesystemUpdate) {
        delay(2000);
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
  display.display();
  display.drawString(0, 26, "Rebooting...");
  display.display();
  Serial.println("Reboot message displayed");
  delay(2000);
  Serial.println("Executing restart...");
  ESP.restart();
}
