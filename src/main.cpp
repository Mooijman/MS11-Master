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

// File paths to save input values permanently
const char* globalConfigPath = "/global.conf";
const char* passPath = "/pass.txt";

IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 0, 0);

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

// Read File from LittleFS
String readFile(fs::FS &fs, const char * path){
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

// Write file to LittleFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
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
    }
  }
  file.close();
}

// Write global config to file
void writeGlobalConfig() {
  File file = LittleFS.open(globalConfigPath, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open global config for writing");
    return;
  }
  
  file.println("ssid=" + ssid);
  file.println("ip=" + ip);
  file.println("gateway=" + gateway);
  file.println("dhcp=" + useDHCP);
  file.println("gpioviewer=" + gpioViewerEnabled);
  file.close();
  Serial.println("Global config saved");
}

// Update only GPIO Viewer setting in config file
void updateGpioViewerSetting() {
  // Read current config
  String currentSsid, currentIp, currentGateway, currentDhcp;
  File file = LittleFS.open(globalConfigPath);
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.startsWith("ssid=")) {
        currentSsid = line.substring(5);
      } else if (line.startsWith("ip=")) {
        currentIp = line.substring(3);
      } else if (line.startsWith("gateway=")) {
        currentGateway = line.substring(8);
      } else if (line.startsWith("dhcp=")) {
        currentDhcp = line.substring(5);
      }
    }
    file.close();
  }
  
  // Write back with updated GPIO viewer setting
  file = LittleFS.open(globalConfigPath, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open global config for writing");
    return;
  }
  
  file.println("ssid=" + currentSsid);
  file.println("ip=" + currentIp);
  file.println("gateway=" + currentGateway);
  file.println("dhcp=" + currentDhcp);
  file.println("gpioviewer=" + gpioViewerEnabled);
  file.close();
  Serial.println("GPIO Viewer setting updated");
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
  pass = readFile(LittleFS, passPath);
  
  // Set default GPIO Viewer state to off if not set
  if (gpioViewerEnabled == "") {
    gpioViewerEnabled = "off";
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
      Serial.println("Starting GPIO Viewer...");
      gpio_viewer = new GPIOViewer();
      gpio_viewer->setSamplingInterval(100);
      gpio_viewer->setPort(5555);
      gpio_viewer->begin();
    } else {
      Serial.println("GPIO Viewer is disabled");
    }
    
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html");
    });
    
    // Route for settings page with template processor
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
      String gpioViewerStatus = gpioViewerEnabled;
      bool gpioEnabled = (gpioViewerStatus == "true" || gpioViewerStatus == "on");
      
      request->send(LittleFS, "/settings.html", "text/html", false, [gpioEnabled](const String& var) -> String {
        if (var == "GPIOVIEWER_CHECKED") {
          return gpioEnabled ? "checked" : "";
        }
        if (var == "GPIOVIEWER_BUTTON") {
          if (gpioEnabled) {
            String ip = WiFi.localIP().toString();
            return "<a href=\"http://" + ip + ":5555\" target=\"_blank\" id=\"gpioViewerBtn\" style=\"background-color: #4A4A4A; color: white; padding: 3px 8px; text-decoration: none; border-radius: 4px; font-size: 12px; white-space: nowrap; transition: background-color 0.3s; min-width: 60px; display: inline-block; text-align: center;\">Open</a>";
          }
          return "";
        }
        return String();
      });
    });
    
    // Handle settings POST
    server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
      bool shouldReboot = false;
      bool gpioDisabled = false;
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
          message = "<img src='hex100.png' alt='' style='height: 0.8em; vertical-align: -0.05em; margin-right: 6px;'>GPIO Viewer enabled - please wait";
          shouldReboot = true;
        }
      } else {
        // Checkbox is not checked - disable GPIO Viewer
        if (currentGpioViewerSetting == "on" || currentGpioViewerSetting == "true") {
          gpioViewerEnabled = "off";
          updateGpioViewerSetting();
          Serial.println("GPIO Viewer disabled - powercycle required");
          message = "Powercycle ESP32 now!";
          gpioDisabled = true;
        }
      }
      
      // Check if reboot checkbox was checked
      if (request->hasParam("reboot", true)) {
        Serial.println("Reboot requested");
        message = "<img src='hex100.png' alt='' style='height: 0.8em; vertical-align: -0.05em; margin-right: 6px;'>Rebooting - please wait";
        shouldReboot = true;
      }
      
      // Send confirmation page
      if (shouldReboot || gpioDisabled) {
        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Please Wait</title><meta name='viewport' content='width=device-width, initial-scale=1'>";
        if (shouldReboot) {
          html += "<meta http-equiv='refresh' content='20;url=/settings'>";
        }
        html += "<link rel='icon' href='data:,'><link rel='stylesheet' type='text/css' href='style.css'></head><body>";
        html += "<div class='topnav'><img src='waacs-logo.png' alt='Waacs Design & Consultancy' style='height: 24px; margin: 50px auto 10px auto; display: block;'></div>";
        html += "<div class='content'><div class='card-grid'><div class='card'>";
        html += "<h1 style='color: #555;'>" + message + "</h1>";
        if (gpioDisabled) {
          html += "<div style='text-align: right;'><input type='button' value='Reload' onclick='window.location.reload();' style='border: none; color: #FEFCFB; background-color: #000000; padding: 12px 24px; text-align: center; text-decoration: none; display: inline-block; font-size: 14px; width: auto; min-width: 120px; margin: 15px 0 5px 0; border-radius: 4px; cursor: pointer;'></div>";
        }
        html += "</div></div></div></body></html>";
        request->send(200, "text/html", html);
        
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
    
    server.serveStatic("/", LittleFS, "/");
    
    // Start ArduinoOTA
    ArduinoOTA.setHostname("ESP32-Base");
    ArduinoOTA.begin();
    Serial.println("ArduinoOTA started");
    
    server.begin();
    Serial.println("Web server started");
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    isAPMode = true;
    
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

    // Web Server Root URL - Captive Portal
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });

    // Catch-all for captive portal - redirect everything to root
    server.onNotFound([](AsyncWebServerRequest *request){
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });
    
    // WiFi scan endpoint
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
      String json = "{\"networks\":[";
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; ++i) {
        if (i) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\"";
        json += ",\"rssi\":" + String(WiFi.RSSI(i));
        json += ",\"encryption\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
        json += "}";
      }
      json += "]}";
      request->send(200, "application/json", json);
      WiFi.scanDelete();
    });
    
    server.serveStatic("/", LittleFS, "/");
    
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
            Serial.print("Password set to: ");
            Serial.println(pass);
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
  
  // Handle ArduinoOTA updates
  ArduinoOTA.handle();
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
