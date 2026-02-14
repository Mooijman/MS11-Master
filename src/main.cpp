#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <time.h>

// Project modules
#include "config.h"
#include "settings.h"
#include "i2c_manager.h"
#include "display_manager.h"
#include "lcd_manager.h"
#include "seesaw_rotary.h"
#include "aht10_manager.h"
#include "probe_manager.h"
#include "gpio_manager.h"
#include "slave_controller.h"
#include "github_updater.h"
#include "wifi_manager.h"
#include "md11_slave_update.h"
#include "images.h"

// Extracted modules
#include "utils.h"
#include "ntp_manager.h"
#include "web_server_routes.h"

// ============================================================================
// GLOBAL INSTANCES
// ============================================================================

Preferences preferences;

// GitHub updater instance
GitHubUpdater* githubUpdater = nullptr;

// WiFi manager instance
WiFiManager* wifiManager = nullptr;

// MD11 Slave updater instance
MD11SlaveUpdate* md11SlaveUpdater = nullptr;

// Create AsyncWebServer object
AsyncWebServer server(WEB_SERVER_PORT);

// DNS server for captive portal
DNSServer dnsServer;

// ============================================================================
// APPLICATION STATE
// ============================================================================

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
String& otaEnabled = settings.otaEnabled;
String& updatesEnabled = settings.updatesEnabled;
String& updateUrl = settings.updateUrl;
String& ntpEnabled = settings.ntpEnabled;
String& timezone = settings.timezone;

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

// NeoPixel tracking variables
unsigned long lastNeopixelUpdate = 0;
unsigned long blinkStartTime = 0;
uint32_t currentNeopixelColor = 0;  // Current status color
uint32_t blinkColor = 0;
uint32_t blinkResumeColor = 0;
uint8_t blinkPhases = 0;
uint16_t blinkPhaseDurationMs = 0;
bool blinkActive = false;
bool neoPixelInitializedFlag = false;  // Track if NeoPixel was initialized

// Encoder counter variables
int32_t encoderCounter = 90;  // Counter value (90-350)
int32_t lastEncoderPosition = 0;
unsigned long lastCounterUpdate = 0;
bool counterDisplayNeedsUpdate = true;

// OTA update flag (set by GitHubUpdater during firmware/FS updates)
bool otaUpdateInProgress = false;

// LCD time display tracking
unsigned long lastLcdTimeUpdate = 0;

// MS11 detection tracking
unsigned long ms11DetectionTime = 0;
bool ms11DetectionShown = false;
bool ms11Present = false;

// MS11 connection lost blink + restored tracking
bool ms11ConnectionLost = false;
bool lastConnectionLostBlink = true;
bool ms11Restored = false;
unsigned long ms11RestoredTime = 0;

// Heartbeat / reconnect pulse tracking (2-second interval)
unsigned long lastHeartbeatTime = 0;

// LED pulse tracking (non-blocking)
unsigned long ledPulseStartTime = 0;
uint16_t ledPulseDurationMs = 0;
bool ledPulseActive = false;

// Startup blink tracking
bool startupBlinkDone = false;
unsigned long startupBlinkStart = 0;

// Forward declaration
void performReboot();

// Helper: delay with LCD startup blink (replaces blocking delay() during setup)
void delayWithBlink(unsigned long ms) {
  unsigned long start = millis();
  bool lastVisible = true;
  while (millis() - start < ms) {
    if (!startupBlinkDone && LCDManager::getInstance().isInitialized()) {
      bool visible = blinkState(millis(), 600, 400);
      if (visible != lastVisible) {
        lastVisible = visible;
        LCDManager::getInstance().printLine(1, visible ? " Starting up..." : "");
      }
    }
    delay(10);  // Small yield to avoid WDT
  }
}

// Check for .hex file in LittleFS root and perform MS11-control update if found
bool checkAndUpdateMS11Firmware() {
  // Scan root directory for .hex files
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    return false;
  }
  
  String hexFilePath = "";
  File file = root.openNextFile();
  
  while (file) {
    String fileName = String(file.name());
    if (fileName.endsWith(".hex") && !fileName.startsWith(".")) {
      hexFilePath = "/" + fileName;
      file.close();
      break;
    }
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  
  if (hexFilePath.isEmpty()) {
    return false;
  }
  
  // Show update message on LCD
  if (LCDManager::getInstance().isInitialized()) {
    LCDManager::getInstance().clear();
    LCDManager::getInstance().printLine(0, "MS11-Control");
    LCDManager::getInstance().printLine(1, "Updating...");
    startupBlinkDone = true;
  }
  
  // Enter bootloader mode
  I2CManager& manager = I2CManager::getInstance();
  if (!manager.writeRegister(0x30, 0x99, 0xB0)) {
    LCDManager::getInstance().printLine(1, "Update failed!");
    delay(3000);
    return false;
  }
  
  // Wait for bootloader to start
  delay(6000);
  
  // Check if bootloader is active
  if (!manager.ping(0x14, I2C_BUS_SLAVE)) {
    LCDManager::getInstance().printLine(1, "Bootloader fail!");
    delay(3000);
    return false;
  }
  
  // Upload hex file
  File hexFile = LittleFS.open(hexFilePath, "r");
  if (!hexFile) {
    LCDManager::getInstance().printLine(1, "File open fail!");
    delay(3000);
    return false;
  }
  
  String hexContent = hexFile.readString();
  hexFile.close();
  
  if (!md11SlaveUpdater) {
    md11SlaveUpdater = new MD11SlaveUpdate();
  }
  
  if (!md11SlaveUpdater->uploadHexFile(hexContent)) {
    LCDManager::getInstance().printLine(1, "Upload failed!");
    delay(3000);
    return false;
  }
  
  // Exit bootloader
  uint8_t exitCmd[2] = {0x01, 0x80};
  manager.write(0x14, exitCmd, 2);
  delay(1000);
  
  // Delete hex file
  delay(100);
  if (!LittleFS.remove(hexFilePath)) {
    // Fallback: rename to mark as processed
    LittleFS.rename(hexFilePath, hexFilePath + ".done");
  }
  
  // Show success message
  if (LCDManager::getInstance().isInitialized()) {
    LCDManager::getInstance().printLine(1, "Success!");
  }
  delay(2000);
  
  delay(500);
  ESP.restart();
  
  return true;  // Never reached due to ESP.restart()
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  // Initialize I2C Manager (Dual Bus: Slave @100kHz, Display @100kHz)
  if (!I2CManager::getInstance().begin()) {
    Serial.println("CRITICAL: I2C Manager initialization failed!");
    while (true) { delay(100); }  // Halt
  }

  // Initialize GPIO Manager (Power Switch, Buttons, Status LED)
  if (!GPIOManager::getInstance().begin()) {
    Serial.println("WARNING: GPIO Manager initialization failed");
  }

  // Initialize Display Manager (Bus 1: GPIO8/9, OLED @ 0x3C)
  if (!DisplayManager::getInstance().begin()) {
    Serial.println("WARNING: OLED Display initialization failed - continuing anyway");
  }
  
  // Initialize LCD Manager (Bus 1: GPIO8/9, LCD @ 0x27)
  if (!LCDManager::getInstance().begin()) {
    Serial.println("WARNING: LCD 16x2 initialization failed - continuing anyway");
  } else {
    // Show startup message on LCD (line 1 will blink in loop)
    LCDManager::getInstance().clear();
    LCDManager::getInstance().printLine(0, "*MagicSmoker 11*");
    LCDManager::getInstance().printLine(1, " Starting up...");
    startupBlinkStart = millis();
    startupBlinkDone = false;
  }
  
  // Initialize Seesaw Rotary Encoder (Bus 1: GPIO8/9, Seesaw @ 0x36)
  if (!SeesawRotary::getInstance().begin()) {
    Serial.println("WARNING: Seesaw Rotary Encoder initialization failed - continuing anyway");
  } else {
    // Initialize NeoPixel immediately and set startup color
    if (SeesawRotary::getInstance().neoPixelBegin()) {
      neoPixelInitializedFlag = true;
      currentNeopixelColor = 0xFFFF00;  // Yellow at power-on
      SeesawRotary::getInstance().setNeoPixelColor(currentNeopixelColor);
    } else {
      Serial.println("[NeoPixel] WARNING: init failed in setup");
    }
  }

  // Initialize AHT10 Temperature & Humidity Sensor (Bus 1: GPIO8/9, AHT10 @ 0x38)
  if (!AHT10Manager::getInstance().begin()) {
    Serial.println("WARNING: AHT10 sensor initialization failed - continuing anyway");
  }
  
  // Initialize Slave Controller (Bus 0: GPIO6/7)
  if (!SlaveController::getInstance().begin()) {
    Serial.println("WARNING: Slave controller not responding - check I2C wiring");
  }

  // Initialize Probe Manager (aggregated temperature from multiple sources)
  ProbeManager::getInstance().begin();
  
  // Initialize LittleFS early (needed for firmware update check)
  initLittleFS();
  
  // Detect MS11-control early (before WiFi, before Waacs logo)
  delayWithBlink(500);
  ms11Present = SlaveController::getInstance().ping();
  Serial.printf("[Main] MS11-control detection: %s\n", ms11Present ? "PRESENT" : "ABSENT");
  
  // Check for firmware update hex file and perform update if found
  if (ms11Present) {
    checkAndUpdateMS11Firmware();  // Will reboot if update performed
    
    // Trigger 500ms LED pulse on MS11-control (normal startup)
    if (SlaveController::getInstance().pulseLed(500)) {
      ledPulseStartTime = millis();
      ledPulseDurationMs = 500;
      ledPulseActive = true;
    }
  }

  // Show Waacs logo on display
  Serial.println("Displaying Waacs logo...");
  DisplayManager::getInstance().clear();
  delayWithBlink(100);
  // Waacs logo: 105 pixels wide, 21 pixels high (centered on 128x64 display)
  // x=11 centers the logo: (128-105)/2 = 11.5 ≈ 11
  DisplayManager::getInstance().drawXbm(11, 16, 105, 21, Waacs_Logo_bits);
  Serial.println("drawXbm called for Waacs logo (105x21 @ 11,16)");
  DisplayManager::getInstance().updateDisplay();
  Serial.println("Waacs logo displayed");
  delayWithBlink(3000);
  
  // Clear and show MS11 Master text + MS11-control status
  DisplayManager::getInstance().clear();
  delayWithBlink(100);
  Serial.println("Displaying MS11 Master + MS11-control status...");
  DisplayManager::getInstance().setFont(ArialMT_Plain_10);
  DisplayManager::getInstance().drawString(0, 0, "MS11 Master");
  // Line 1 blank
  DisplayManager::getInstance().drawString(0, 28, "MS11-control");
  DisplayManager::getInstance().drawString(0, 42, ms11Present ? "Detected" : "Absent");
  DisplayManager::getInstance().updateDisplay();
  startupBlinkDone = true;  // Stop startup blink
  delayWithBlink(2000);
  DisplayManager::getInstance().clear();
  DisplayManager::getInstance().updateDisplay();

  // Initialize NVS with defaults on first boot (LittleFS already initialized earlier)
  settings.initialize();

  // Load values from NVS
  settings.load();
  
  // Sync versions with compile-time defines
  settings.syncVersions();
  
  // Print loaded settings
  settings.print();
  
  // Initialize GitHub updater
  githubUpdater = new GitHubUpdater(preferences);
  githubUpdater->loadUpdateInfo();
  
  // Initialize MD11 Slave updater
  md11SlaveUpdater = new MD11SlaveUpdate();
  
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

  // Show blue while WiFi is initializing
  if (neoPixelInitializedFlag) {
    currentNeopixelColor = 0x0000FF;
    SeesawRotary::getInstance().setNeoPixelColor(currentNeopixelColor);
  }
  
  if(wifiManager->begin(ssid, pass, ip, gateway, netmask, isDHCP, WIFI_CONNECT_TIMEOUT)) {
    // WiFi connected successfully
    Serial.println("WiFi connected!");

    if (neoPixelInitializedFlag) {
      currentNeopixelColor = 0x00FF00;
      SeesawRotary::getInstance().setNeoPixelColor(currentNeopixelColor);
    }
    
    // Show WiFi logo on OLED
    DisplayManager::getInstance().clear();
    delay(100);  // Ensure clear is complete
    DisplayManager::getInstance().drawXbm(34, 14, 60, 36, WiFi_Logo_bits);
    DisplayManager::getInstance().updateDisplay();
    delay(100);  // Let render complete
    delay(1000);
    
    // Show IP address + versions on OLED
    DisplayManager::getInstance().clear();
    DisplayManager::getInstance().setFont(ArialMT_Plain_10);
    DisplayManager::getInstance().drawString(0, 0, "IP: " + WiFi.localIP().toString());
    // Line 2 intentionally left blank
    DisplayManager::getInstance().drawString(0, 28, "fw-" + currentFirmwareVersion);
    DisplayManager::getInstance().drawString(0, 42, "fs-" + currentFilesystemVersion);
    DisplayManager::getInstance().drawString(0, 54, "sl-" + SlaveController::getInstance().getFullVersionString());
    DisplayManager::getInstance().updateDisplay();
    
    // Show WiFi enabled and IP address on LCD (16x2 display)
    LCDManager::getInstance().clear();
    LCDManager::getInstance().printLine(0, "WiFi Enabled");
    String ipStr = WiFi.localIP().toString();
    String ipDisplay = ipStr.length() > 16 ? ipStr.substring(0, 16) : ipStr;
    LCDManager::getInstance().printLine(1, ipDisplay);
    startupBlinkDone = true;  // Stop startup blink
    
    // Start timer to clear display after 3 seconds
    ipDisplayTime = millis();
    ipDisplayShown = true;
    ipDisplayCleared = false;
    
    // MS11-control already detected in early setup, set flags for LCD display
    ms11DetectionTime = millis();
    ms11DetectionShown = true;  // Already shown during early startup

    // Sync time if enabled
    syncTimeIfEnabled(true);  // true = boot sync, saves boot time
    
    // Register all STA-mode web routes
    registerSTARoutes(server);
    
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
    DisplayManager::getInstance().clear();
    DisplayManager::getInstance().setTextAlignment(TEXT_ALIGN_LEFT);
    DisplayManager::getInstance().drawString(0, 16, "WiFi - Manager");
    DisplayManager::getInstance().updateDisplay();
    
    // Show AP mode on LCD
    LCDManager::getInstance().clear();
    LCDManager::getInstance().printLine(0, "WiFi manager");
    LCDManager::getInstance().printLine(1, "ESP-WIFI-MANAGER");
    startupBlinkDone = true;  // Stop startup blink in AP mode
    
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

    // Register AP-mode web routes (captive portal)
    registerAPRoutes(server, dnsServer);
    
    server.begin();
  }  // End of else (AP mode)
}  // End of setup()

// ============================================================================
// LOOP HELPER FUNCTIONS
// ============================================================================

// Track if LCD has been updated with final status
bool lcdStatusShown = false;

// Track last startup blink state to avoid unnecessary LCD writes
bool lastStartupBlinkVisible = true;

// Handle display tasks (IP display timeout, sensor readings, etc.)
void handleDisplayTasks() {
  // Skip display updates during OTA firmware/filesystem updates
  if (otaUpdateInProgress) {
    return;
  }
  
  // Show temperature and humidity on OLED continuously (throttled)
  static unsigned long lastDisplayUpdate = 0;
  unsigned long now = millis();
  
  // Read AHT10 sensor periodically (every 30 seconds to save power)
  static unsigned long lastSensorRead = 0;
  if ((now - lastSensorRead >= 30000) && AHT10Manager::getInstance().isInitialized()) {
    AHT10Manager::getInstance().readSensor();
    lastSensorRead = now;
  }

  // Update display every 1 second with temperature and humidity (only after startup complete)
  // Only redraw OLED when sensor data actually changed to avoid unnecessary I2C traffic
  if (ipDisplayCleared && (now - lastDisplayUpdate >= 1000) && DisplayManager::getInstance().isInitialized()) {
    lastDisplayUpdate = now;
    
    // Read all temperature probes (AHT10 master and MS11-slave)
    if (ProbeManager::getInstance().isInitialized()) {
      ProbeManager::getInstance().readAllProbes();
    }
    
    // Get MST (master) temperature and humidity from AHT10
    float tempMST = AHT10Manager::getInstance().getTemperature();
    float humidity = AHT10Manager::getInstance().getHumidity();
    String lineM = "MST: " + String(tempMST, 1) + "°C " + String(humidity, 0) + "%";
    
    // Get SLV (slave) temperature from MS11-control
    String lineSLV = "";
    ProbeData* slaveProbe = ProbeManager::getInstance().getProbeByType(ProbeType::MS11_CONTROL_TEMP);
    if (slaveProbe && slaveProbe->healthy) {
      float tempSLV = slaveProbe->temperature;
      lineSLV = "SLV: " + String(tempSLV, 1) + "°C";
    }
    
    // Combine both lines
    String displayInfo = lineM;
    if (!lineSLV.isEmpty()) {
      displayInfo = lineM + "\n" + lineSLV;
    }
    
    static String lastDisplayInfo = "";
    if (displayInfo != lastDisplayInfo) {
      lastDisplayInfo = displayInfo;
      DisplayManager::getInstance().clear();
      DisplayManager::getInstance().setFont(ArialMT_Plain_10);
      DisplayManager::getInstance().drawString(0, 0, lineM);
      if (!lineSLV.isEmpty()) {
        DisplayManager::getInstance().drawString(0, 12, lineSLV);
      }
      DisplayManager::getInstance().updateDisplay();
    }
  }
  
  // NOTE: LED pulse check moved to top of loop() for accurate timing
  
  // ---- Startup blink: "Starting up..." blinks on line 1 (600ms on / 400ms off) ----
  if (!startupBlinkDone && LCDManager::getInstance().isInitialized()) {
    bool visible = blinkState(now, 600, 400);
    if (visible != lastStartupBlinkVisible) {
      lastStartupBlinkVisible = visible;
      LCDManager::getInstance().printLine(1, visible ? " Starting up..." : "");
    }
  }
  
  // Non-blocking IP display clear after timeout
  if (ipDisplayShown && !ipDisplayCleared && (millis() - ipDisplayTime > DISPLAY_IP_SHOW_DURATION)) {
    DisplayManager::getInstance().clear();
    DisplayManager::getInstance().updateDisplay();
    
    // MS11-control detection already shown during early startup, transition directly to clock
    if (!lcdStatusShown) {
      // After MS11 detection message, show ready status
      if (LCDManager::getInstance().isInitialized()) {
        LCDManager::getInstance().clear();
        lcdStatusShown = true;
      }
    }
    
    if (lcdStatusShown) {
      ipDisplayCleared = true;
    }
  }

  // ---- Ready display with blinking period ----
  if (lcdStatusShown && ipDisplayCleared && !ms11ConnectionLost && !ms11Restored && LCDManager::getInstance().isInitialized()) {
    // Blinking period after Ready: visible first 600ms of each second
    bool periodVisible = blinkState(now, 600, 400);
    static bool lastPeriodVisible = true;
    static bool readyLineInitialized = false;
    if (!readyLineInitialized || periodVisible != lastPeriodVisible) {
      readyLineInitialized = true;
      lastPeriodVisible = periodVisible;
      LCDManager::getInstance().printLine(0, periodVisible ? "Ready." : "Ready ");
    }
  }
  
  // ---- Heartbeat / reconnect: ping MS11-control every 2 seconds ----
  // Skip heartbeat during bootloader operations to avoid I2C interference
  if (ipDisplayCleared && (now - lastHeartbeatTime >= 2000)) {
    lastHeartbeatTime = now;
    if (ms11Present) {
      // Heartbeat: verify MS11-control is still alive with 2ms LED pulse
      if (!SlaveController::getInstance().ping()) {
        // Lost contact — start blinking "Connection lost!"
        Serial.println("[Main] Lost contact with MS11-control!");
        ms11Present = false;
        ms11ConnectionLost = true;
        ms11Restored = false;
        lastConnectionLostBlink = true;
        if (LCDManager::getInstance().isInitialized()) {
          LCDManager::getInstance().clear();
          LCDManager::getInstance().printLine(0, "MS11-Control");
          LCDManager::getInstance().printLine(1, "Connection lost!");
        }
      } else {
        // MS11-control present: send 2ms heartbeat pulse (safe with I2CManager mutex)
        if (SlaveController::getInstance().pulseLed(2)) {
          ledPulseStartTime = millis();
          ledPulseDurationMs = 2;
          ledPulseActive = true;
        }
      }
    } else {
      // Reconnect: try to re-establish contact with MS11-control
      if (SlaveController::getInstance().ping()) {
        Serial.println("[Main] MS11-control reconnected!");
        ms11Present = true;
        ms11ConnectionLost = false;
        ms11Restored = true;
        ms11RestoredTime = millis();
        if (LCDManager::getInstance().isInitialized()) {
          LCDManager::getInstance().clear();
          LCDManager::getInstance().printLine(0, "MS11-Control");
          LCDManager::getInstance().printLine(1, "Restored");
        }
        // Send detection pulse on reconnect
        if (SlaveController::getInstance().pulseLed(500)) {
          ledPulseStartTime = millis();
          ledPulseDurationMs = 500;
          ledPulseActive = true;
        }
      }
    }
  }
  
  // ---- Blink "Connection lost!" on LCD when MS11-control is absent ----
  if (ms11ConnectionLost && LCDManager::getInstance().isInitialized()) {
    bool visible = blinkState(now, 600, 400);
    if (visible != lastConnectionLostBlink) {
      lastConnectionLostBlink = visible;
      LCDManager::getInstance().printLine(1, visible ? "Connection lost!" : "");
    }
  }
  
  // ---- Non-blocking return to normal display after "Restored" (3 seconds) ----
  if (ms11Restored && (now - ms11RestoredTime >= 3000)) {
    ms11Restored = false;
    if (LCDManager::getInstance().isInitialized()) {
      LCDManager::getInstance().clear();
      // Blinking period after Ready: visible first 600ms of each second
      bool periodVisible = blinkState(now, 600, 400);
      LCDManager::getInstance().printLine(0, periodVisible ? "Ready." : "Ready ");
      LCDManager::getInstance().printLine(1, "");
    }
  }
  
  // ---- LCD time display: colon always visible, blinking period in Ready instead ----
  // Only show clock when MS11-control is connected (ms11Present) and not during "Restored" message
  if (ipDisplayCleared && lcdStatusShown && ms11Present && !ms11Restored && LCDManager::getInstance().isInitialized() && Settings::stringToBool(ntpEnabled)) {
    time_t rawTime = time(nullptr);
    if (rawTime >= NTP_VALID_TIME) {
      int timezoneOffsetHours = parseTimezoneOffset(timezone);
      
      time_t localTime = rawTime + (timezoneOffsetHours * 3600);
      struct tm timeinfo;
      gmtime_r(&localTime, &timeinfo);
      
      // Colon always visible (period in Ready blinks instead)
      char timeStr[17];
      snprintf(timeStr, sizeof(timeStr), "%02d-%02d-%04d %02d:%02d",
               timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
               timeinfo.tm_hour, timeinfo.tm_min);
      
      // Only update LCD when string actually changed
      static char lastTimeStr[17] = "";
      if (strcmp(timeStr, lastTimeStr) != 0) {
        strncpy(lastTimeStr, timeStr, sizeof(lastTimeStr));
        LCDManager::getInstance().printLine(1, String(timeStr));
      }
    }
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

// Handle NeoPixel status indicator and button feedback
void handleNeopixelTasks() {
  if (!SeesawRotary::getInstance().isInitialized()) {
    return;  // Seesaw not ready
  }

  unsigned long now = millis();
  static unsigned long lastInitAttempt = 0;

  auto setColorIfChanged = [&](uint32_t color) {
    if (color != currentNeopixelColor) {
      currentNeopixelColor = color;
      SeesawRotary::getInstance().setNeoPixelColor(color);
    }
  };

  auto startBlink = [&](uint32_t color, uint32_t resumeColor, uint8_t phases, uint16_t phaseMs) {
    blinkActive = true;
    blinkColor = color;
    blinkResumeColor = resumeColor;
    blinkPhases = phases;
    blinkPhaseDurationMs = phaseMs;
    blinkStartTime = now;
  };

  // Try to initialize NeoPixel once after boot, then retry occasionally if needed
  if (!neoPixelInitializedFlag) {
    if (now < 2000) {
      return;  // Let system settle before first init attempt
    }

    if (now - lastInitAttempt >= 3000) {
      lastInitAttempt = now;
      Serial.println("[NeoPixel] Attempting init...");
      if (SeesawRotary::getInstance().neoPixelBegin()) {
        neoPixelInitializedFlag = true;
        currentNeopixelColor = 0xFFFF00;  // Yellow at startup
        SeesawRotary::getInstance().setNeoPixelColor(currentNeopixelColor);
        Serial.println("[NeoPixel] ✓ Init OK");
      } else {
        Serial.println("[NeoPixel] Init failed");
      }
    }
    return;
  }

  // Determine base status color
  uint32_t statusColor = 0xFFFF00;  // Yellow immediately at startup
  wl_status_t wifiStatus = WiFi.status();
  if (!isAPMode && wifiStatus == WL_CONNECTED) {
    statusColor = 0x00FF00;  // Green: WiFi connected
  } else if (isAPMode) {
    statusColor = 0x0000FF;  // Blue: AP mode
  } else if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL) {
    statusColor = 0xFF0000;  // Red: WiFi error
  } else {
    statusColor = 0x0000FF;  // Blue: connecting
  }

  // Button press: 3x white blink, ignore early boot noise
  // Throttle button reads to every 50ms (I2C read is ~2-5ms)
  static unsigned long lastButtonCheck = 0;
  if (!blinkActive && now > 5000 && (now - lastButtonCheck >= 50)) {
    lastButtonCheck = now;
    if (SeesawRotary::getInstance().getButtonPress()) {
      startBlink(0xFFFFFF, statusColor, 6, 100);
    }
  }

  // Handle blink animation
  if (blinkActive) {
    unsigned long elapsed = now - blinkStartTime;
    uint8_t phase = elapsed / blinkPhaseDurationMs;
    if (phase < blinkPhases) {
      if ((phase % 2) == 0) {
        SeesawRotary::getInstance().setNeoPixelColor(blinkColor);
      } else {
        SeesawRotary::getInstance().neoPixelOff();
      }
      return;
    }
    blinkActive = false;
    currentNeopixelColor = blinkResumeColor;
    SeesawRotary::getInstance().setNeoPixelColor(blinkResumeColor);
    lastNeopixelUpdate = now;  // Reset throttle to prevent immediate overwrite
    return;  // Exit immediately after restoration
  }

  // AP mode: blue blink (throttled to prevent excessive I2C writes)
  if (isAPMode) {
    bool apOn = ((now / 500) % 2) == 0;
    static bool lastApOn = true;
    if (apOn != lastApOn) {
      lastApOn = apOn;
      if (apOn) {
        setColorIfChanged(0x0000FF);
      } else {
        setColorIfChanged(0x000000);
      }
    }
    return;
  }

  // Solid status color
  if (now - lastNeopixelUpdate >= 500) {
    lastNeopixelUpdate = now;
    setColorIfChanged(statusColor);
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // ---- LED pulse check FIRST for accurate pulse timing ----
  if (ledPulseActive) {
    unsigned long elapsed = millis() - ledPulseStartTime;
    if (elapsed >= ledPulseDurationMs) {
      SlaveController::getInstance().setLed(false);
      ledPulseActive = false;
    }
  }

  // Update GPIO states (buttons, switch, LED animations)
  GPIOManager::getInstance().update();
  
  handleDisplayTasks();
  handleSystemTasks();
  handleNetworkTasks();
  handleNeopixelTasks();  // Update NeoPixel status indicator
  
  // Small yield to prevent excessive CPU usage
  delay(LOOP_DELAY);
}

// ============================================================================
// REBOOT
// ============================================================================

// Function to show reboot message and restart
void performReboot() {
  Serial.println("Showing reboot message on display");
  DisplayManager::getInstance().clear();
  DisplayManager::getInstance().invert(true);  // Invert colors
  DisplayManager::getInstance().drawString(0, 26, "Rebooting...");
  DisplayManager::getInstance().updateDisplay();
  Serial.println("Reboot message displayed");
  delay(2000);
  Serial.println("Executing restart...");
  ESP.restart();
}
