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

// start gpio viewer instance:
GPIOViewer gpio_viewer;

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

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";
const char* dhcpPath = "/dhcp.txt";

IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 0, 0);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Track if we're in AP mode
bool isAPMode = false;

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
      writeFile(LittleFS, ssidPath, "");
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
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  ip = readFile(LittleFS, ipPath);
  gateway = readFile(LittleFS, gatewayPath);
  useDHCP = readFile(LittleFS, dhcpPath);
  
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
    delay(2000);
    
    // Initialize GPIO Viewer
    gpio_viewer.setSamplingInterval(100);
    gpio_viewer.setPort(5555);
    gpio_viewer.begin();
    
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html");
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
    display.drawString(0, 16, "Connecting Wifi...");
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
      for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(LittleFS, ssidPath, ssid.c_str());
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
            // Write file to save value
            writeFile(LittleFS, ipPath, ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            writeFile(LittleFS, gatewayPath, gateway.c_str());
          }
          // HTTP POST dhcp value
          if (p->name() == PARAM_INPUT_5) {
            useDHCP = p->value().c_str();
            Serial.print("DHCP set to: ");
            Serial.println(useDHCP);
            // Write file to save value
            writeFile(LittleFS, dhcpPath, useDHCP.c_str());
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
        writeFile(LittleFS, dhcpPath, "false");
        Serial.println("DHCP set to: false");
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
  }
}

void loop() {
  // Process DNS requests only in AP mode for captive portal
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  
  // Handle ArduinoOTA updates
  ArduinoOTA.handle();
  
  // Small delay to prevent 100% CPU usage and reduce heat
  delay(10);
}
