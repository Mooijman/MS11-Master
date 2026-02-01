#include <gpio_viewer.h> // Must me the first include in your project
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <SSD1306.h>
#include "images.h"

// GPIO Viewer pointer (only instantiated if enabled)
GPIOViewer* gpio_viewer = nullptr;

// OLED display instance (address 0x3c, SDA pin 5, SCL pin 4)
SSD1306 display(0x3c, 5, 4);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// DNS server for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";
const char* PARAM_INPUT_5 = "dhcp";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;
String useDHCP;
String gpioViewerEnabled;
String otaEnabled;
String updatesEnabled;
String updateUrl;

// File paths to save input values permanently
const char* globalConfigPath = "/global.conf";
const char* passPath = "/pass.txt";

// WiFi scan cache
String cachedScanResults = "";
unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 10000; // 10 seconds
bool scanInProgress = false;

IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 255, 0);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

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
  if (!LittleFS.begin(true)) {
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

// Read first line from file in LittleFS
String readFirstLine(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to LittleFS (atomic)
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  
  // Create temp file path
  String tempPath = String(path) + ".tmp";
  
  // Write to temp file
  File file = fs.open(tempPath.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("- failed to open temp file for writing");
    return;
  }
  
  bool writeSuccess = file.print(message);
  file.flush();
  file.close();
  
  if(!writeSuccess){
    Serial.println("- write failed");
    fs.remove(tempPath.c_str()); // Clean up temp file
    return;
  }
  
  // Remove old file if exists
  if(fs.exists(path)){
    fs.remove(path);
  }
  
  // Rename temp to final
  if(fs.rename(tempPath.c_str(), path)){
    Serial.println("- file written (atomic)");
  } else {
    Serial.println("- atomic rename failed");
    fs.remove(tempPath.c_str()); // Clean up temp file
  }
}

// Read global config from file
void readGlobalConfig() {
  File file = LittleFS.open(globalConfigPath);
  if (!file) {
    Serial.println("Global config file not found");
    return;
  }
  
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.startsWith("ssid=")) {
      ssid = line.substring(5);
    } else if (line.startsWith("ip=")) {
      ip = line.substring(3);
    } else if (line.startsWith("gateway=")) {
      gateway = line.substring(8);
    } else if (line.startsWith("dhcp=")) {
      useDHCP = line.substring(5);
    } else if (line.startsWith("gpioviewer=")) {
      gpioViewerEnabled = line.substring(11);
    } else if (line.startsWith("ota=")) {
      otaEnabled = line.substring(4);
    } else if (line.startsWith("updates=")) {
      updatesEnabled = line.substring(8);
    } else if (line.startsWith("updateUrl=")) {
      updateUrl = line.substring(10);
    }
  }
  file.close();
}

// Write global config to file (atomic)
void writeGlobalConfig() {
  const char* tempPath = "/global.conf.tmp";
  
  // Write to temp file
  File file = LittleFS.open(tempPath, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open temp config for writing");
    return;
  }
  
  file.println("ssid=" + ssid);
  file.println("ip=" + ip);
  file.println("gateway=" + gateway);
  file.println("dhcp=" + useDHCP);
  file.println("gpioviewer=" + gpioViewerEnabled);
  file.println("ota=" + otaEnabled);
  file.println("updates=" + updatesEnabled);
  file.println("updateUrl=" + updateUrl);
  file.flush();
  file.close();
  
  // Remove old file if exists
  if(LittleFS.exists(globalConfigPath)){
    LittleFS.remove(globalConfigPath);
  }
  
  // Rename temp to final
  if(LittleFS.rename(tempPath, globalConfigPath)){
    Serial.println("Global config saved (atomic)");
  } else {
    Serial.println("Failed to rename config file");
    LittleFS.remove(tempPath); // Clean up temp file
  }
}

// Update only GPIO Viewer setting in config file (atomic)
void updateGpioViewerSetting() {
  // Simply use writeGlobalConfig which writes all current global variables
  // This prevents losing other config fields
  writeGlobalConfig();
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

    if (!WiFi.config(localIP, localGateway, subnet)){
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
      LittleFS.remove(globalConfigPath);
      writeFile(LittleFS, passPath, "");
      return false;
    }
    delay(10); // Prevent busy-wait, allow other tasks to run
  }

  Serial.println(WiFi.localIP());
  return true;
}

void setup() {
  Serial.begin(115200);

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

  // Load values saved in LittleFS
  readGlobalConfig();
  pass = readFirstLine(LittleFS, passPath);
  
  // Set default GPIO Viewer state to off if not set
  if (gpioViewerEnabled == "") {
    gpioViewerEnabled = "off";
    writeGlobalConfig();
  }
  
  // Set default OTA state to on if not set
  if (otaEnabled == "") {
    otaEnabled = "on";
    writeGlobalConfig();
  }
  
  Serial.println("Loaded credentials:");
  Serial.println("SSID: " + ssid);
  Serial.println("Password: " + pass);
  Serial.println("DHCP: " + useDHCP);
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
      gpio_viewer->setSamplingInterval(100);
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
      
      request->send(LittleFS, "/settings.html", "text/html", false, [gpioEnabled, updatesEnabledBool](const String& var) -> String {
        if (var == "GPIOVIEWER_CHECKED") {
          return gpioEnabled ? "checked" : "";
        }
        if (var == "GPIOVIEWER_BUTTON") {
          if (gpioEnabled) {
            String ip = WiFi.localIP().toString();
            return "<a href=\"http://" + ip + ":5555\" target=\"_blank\" class=\"btn-small\">Open</a>";
          }
          return "";
        }
        if (var == "UPDATES_CHECKED") {
          return updatesEnabledBool ? "checked" : "";
        }
        if (var == "UPDATE_URL") {
          return updateUrl;
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
      
      // Read current GPIO Viewer setting
      String currentGpioViewerSetting = gpioViewerEnabled;
      
      // Check gpioviewer checkbox
      if (request->hasParam("gpioviewer", true)) {
        // Checkbox is checked - enable GPIO Viewer
        if (currentGpioViewerSetting != "on" && currentGpioViewerSetting != "true") {
          gpioViewerEnabled = "on";
          updateGpioViewerSetting();
          Serial.println("GPIO Viewer enabled - rebooting...");
          message = "<img src='hex100.png' alt='' class='icon-inline'>GPIO Viewer enabled - please wait";
          shouldReboot = true;
        }
      } else {
        // Checkbox is not checked - disable GPIO Viewer
        if (currentGpioViewerSetting == "on" || currentGpioViewerSetting == "true") {
          gpioViewerEnabled = "off";
          updateGpioViewerSetting();
          Serial.println("GPIO Viewer disabled - powercycle required");
          message = "<img src='hex100.png' alt='' class='icon-inline'>Powercycle ESP32 now!";
          gpioDisabled = true;
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
        // Get the update URL
        if (request->hasParam("updateurl", true)) {
          String newUpdateUrl = request->getParam("updateurl", true)->value();
          if (newUpdateUrl != updateUrl) {
            updateUrl = newUpdateUrl;
            configChanged = true;
            Serial.println("Update URL changed to: " + updateUrl);
          }
        }
      } else {
        // Checkbox is not checked - disable updates
        if (updatesEnabled == "on" || updatesEnabled == "true") {
          updatesEnabled = "off";
          configChanged = true;
          Serial.println("Software updates disabled");
        }
      }
      
      // Save config changes if needed
      if (configChanged) {
        writeGlobalConfig();
      }
      
      // Check if reboot checkbox was checked
      if (request->hasParam("reboot", true)) {
        Serial.println("Reboot requested");
        message = "<img src='hex100.png' alt='' class='icon-inline'>Rebooting - please wait";
        shouldReboot = true;
      }
      
      // Send confirmation page
      if (shouldReboot || gpioDisabled) {
        request->send(LittleFS, "/confirmation.html", "text/html", false, [message, shouldReboot, gpioDisabled](const String& var) -> String {
          if (var == "MESSAGE") {
            return message;
          }
          if (var == "REFRESH_META") {
            return shouldReboot ? "<meta http-equiv='refresh' content='20;url=/settings'>" : "";
          }
          if (var == "RELOAD_BUTTON") {
            return gpioDisabled ? "<div class='text-right'><input type='button' value='Reload' onclick='window.location.reload();' class='btn-reload'></div>" : "";
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
    
    // File manager page
    server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/files.html", "text/html");
    });
    
    // Firmware update page
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/update.html", "text/html");
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
      if (cachedScanResults.isEmpty() || (currentTime - lastScanTime > SCAN_INTERVAL)) {
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
            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
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
        writeGlobalConfig();
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

void loop() {
  // Non-blocking IP display clear after 3 seconds
  if (ipDisplayShown && !ipDisplayCleared && (millis() - ipDisplayTime > 3000)) {
    display.clear();
    display.display();
    ipDisplayCleared = true;
  }
  
  // Handle scheduled reboot after 2 seconds delay
  if (rebootScheduled && (millis() - rebootTime > 2000)) {
    performReboot();
  }
  
  // Process DNS requests only in AP mode for captive portal
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  
  // Handle ArduinoOTA updates (only if enabled and not in AP mode)
  if (!isAPMode && (otaEnabled == "on" || otaEnabled == "true")) {
    ArduinoOTA.handle();
  }
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
