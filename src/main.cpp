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

// Forward declaration for slave firmware auto-update at startup
void checkAndUpdateSlaveFromFilesystem();

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

// ============================================================================
// SLAVE FIRMWARE AUTO-UPDATE FROM FILESYSTEM
// ============================================================================
// On startup, checks if a hex file in /fw/ is newer than the current
// MS11-control firmware. If so, flashes the slave automatically.
// This runs during setup(), before the web server starts.

void checkAndUpdateSlaveFromFilesystem() {
  Serial.println("=== SLAVE FIRMWARE CHECK (filesystem) ===");
  
  if (!ms11Present) {
    Serial.println("[SlaveUpdate] Skipping: MS11-control not detected");
    return;
  }
  
  if (!md11SlaveUpdater) {
    Serial.println("[SlaveUpdate] ERROR: MD11SlaveUpdate not initialized");
    return;
  }
  
  // Step 1: Find hex file in /fw/ directory on LittleFS
  File fwDir = LittleFS.open("/fw");
  if (!fwDir || !fwDir.isDirectory()) {
    Serial.println("[SlaveUpdate] No /fw directory found on filesystem");
    return;
  }
  
  String hexFilename = "";
  String hexVersion = "";
  File entry = fwDir.openNextFile();
  while (entry) {
    String name = String(entry.name());
    // Match sl-YYYY.M.m.pp.hex pattern
    if (name.startsWith("sl-") && name.endsWith(".hex")) {
      hexFilename = "/fw/" + name;
      // Extract version: "sl-2026.2.12.01.hex" -> "2026.2.12.01"
      hexVersion = name.substring(3, name.length() - 4);
      Serial.println("[SlaveUpdate] Found hex file: " + hexFilename + " (version " + hexVersion + ")");
      break;  // Use first matching file
    }
    entry = fwDir.openNextFile();
  }
  fwDir.close();
  
  if (hexFilename.isEmpty()) {
    Serial.println("[SlaveUpdate] No sl-*.hex file found in /fw/");
    return;
  }
  
  // Step 2: Read current slave version via I2C
  SlaveVersion slaveVersion;
  if (!SlaveController::getInstance().readFullVersion(slaveVersion)) {
    Serial.println("[SlaveUpdate] WARNING: Could not read slave version, skipping update");
    return;
  }
  
  String currentSlaveVer = slaveVersion.toString();
  Serial.println("[SlaveUpdate] Current slave version: " + currentSlaveVer);
  Serial.println("[SlaveUpdate] Filesystem version: " + hexVersion);
  
  // Step 3: Compare versions - only update if filesystem version is strictly newer
  bool isNewer = githubUpdater->compareVersions(hexVersion, currentSlaveVer);
  
  if (!isNewer) {
    Serial.println("[SlaveUpdate] Slave firmware is up to date, no update needed");
    return;
  }
  
  Serial.println("[SlaveUpdate] Firmware update needed! " + currentSlaveVer + " -> " + hexVersion);
  
  // Step 4: Show status on LCD
  if (LCDManager::getInstance().isInitialized()) {
    LCDManager::getInstance().clear();
    delay(50);
    LCDManager::getInstance().printLine(0, "MS11-control");
    delay(50);
    LCDManager::getInstance().printLine(1, "Updating...");
  }
  
  // Show on OLED too
  DisplayManager::getInstance().clear();
  DisplayManager::getInstance().setFont(ArialMT_Plain_10);
  DisplayManager::getInstance().drawString(0, 0, "MS11-control");
  DisplayManager::getInstance().drawString(0, 15, "Updating slave...");
  DisplayManager::getInstance().drawString(0, 30, currentSlaveVer + " -> " + hexVersion);
  DisplayManager::getInstance().updateDisplay();
  
  // Step 5: Read hex file from filesystem
  File hexFile = LittleFS.open(hexFilename, "r");
  if (!hexFile) {
    Serial.println("[SlaveUpdate] ERROR: Could not open " + hexFilename);
    if (LCDManager::getInstance().isInitialized()) {
      LCDManager::getInstance().printLine(1, "File error!");
    }
    delay(2000);
    return;
  }
  
  String hexContent = hexFile.readString();
  hexFile.close();
  
  if (hexContent.length() == 0) {
    Serial.println("[SlaveUpdate] ERROR: Empty hex file");
    if (LCDManager::getInstance().isInitialized()) {
      LCDManager::getInstance().printLine(1, "Empty file!");
    }
    delay(2000);
    return;
  }
  
  Serial.printf("[SlaveUpdate] Hex file loaded: %d bytes\n", hexContent.length());
  
  // Step 6: Enter bootloader mode
  Serial.println("[SlaveUpdate] Entering bootloader mode...");
  
  if (!md11SlaveUpdater->requestBootloaderMode()) {
    Serial.println("[SlaveUpdate] ERROR: Failed to enter bootloader: " + md11SlaveUpdater->getLastError());
    if (LCDManager::getInstance().isInitialized()) {
      LCDManager::getInstance().printLine(1, "Boot mode fail!");
    }
    delay(2000);
    return;
  }
  Serial.println("[SlaveUpdate] Bootloader mode active");
  
  // Step 7: Flash firmware
  Serial.println("[SlaveUpdate] Flashing firmware...");
  if (LCDManager::getInstance().isInitialized()) {
    LCDManager::getInstance().printLine(1, "Flashing...");
  }
  
  DisplayManager::getInstance().clear();
  DisplayManager::getInstance().setFont(ArialMT_Plain_10);
  DisplayManager::getInstance().drawString(0, 0, "MS11-control");
  DisplayManager::getInstance().drawString(0, 15, "Flashing firmware...");
  DisplayManager::getInstance().updateDisplay();
  
  if (!md11SlaveUpdater->uploadHexFile(hexContent)) {
    String error = md11SlaveUpdater->getLastError();
    Serial.println("[SlaveUpdate] ERROR: Upload failed: " + error);
    if (LCDManager::getInstance().isInitialized()) {
      LCDManager::getInstance().printLine(1, "Flash fail!");
    }
    
    // Try to exit bootloader to restore normal operation
    TwoWire* wire = &Wire1;
    wire->beginTransmission(TWIBOOT_I2C_ADDR);
    wire->write(0x01);  // CMD_SWITCH_APPLICATION
    wire->write(0x80);  // BOOTTYPE_APPLICATION
    wire->endTransmission(true);
    delay(2000);
    return;
  }
  
  hexContent = "";  // Free memory
  Serial.println("[SlaveUpdate] Upload complete!");
  
  // Step 8: Exit bootloader
  Serial.println("[SlaveUpdate] Resetting MS11-control...");
  if (LCDManager::getInstance().isInitialized()) {
    LCDManager::getInstance().printLine(1, "Resetting...");
  }
  
  TwoWire* wire = &Wire1;
  wire->beginTransmission(TWIBOOT_I2C_ADDR);
  wire->write(0x01);  // CMD_SWITCH_APPLICATION
  wire->write(0x80);  // BOOTTYPE_APPLICATION
  wire->endTransmission(true);
  delay(2000);
  
  // Step 9: Verify slave is back
  Serial.println("[SlaveUpdate] Verifying...");
  if (LCDManager::getInstance().isInitialized()) {
    LCDManager::getInstance().printLine(1, "Verifying...");
  }
  
  bool slaveBack = false;
  for (int attempt = 0; attempt < 10; attempt++) {
    if (SlaveController::getInstance().ping()) {
      slaveBack = true;
      break;
    }
    delay(500);
    Serial.printf("[SlaveUpdate] Heartbeat attempt %d/10...\n", attempt + 1);
  }
  
  if (!slaveBack) {
    Serial.println("[SlaveUpdate] WARNING: MS11-control not responding after update!");
    if (LCDManager::getInstance().isInitialized()) {
      LCDManager::getInstance().printLine(1, "No response!");
    }
    delay(2000);
    return;
  }
  
  // Read and display new version
  SlaveVersion newVersion;
  if (SlaveController::getInstance().readFullVersion(newVersion)) {
    Serial.println("[SlaveUpdate] New slave version: " + newVersion.toString());
  }
  
  // Send confirmation LED pulse
  SlaveController::getInstance().pulseLed(500);
  
  Serial.println("[SlaveUpdate] Slave auto-update completed successfully!");
  
  // Show success on displays
  if (LCDManager::getInstance().isInitialized()) {
    LCDManager::getInstance().clear();
    LCDManager::getInstance().printLine(0, "MS11-control");
    LCDManager::getInstance().printLine(1, "Updated!");
  }
  
  DisplayManager::getInstance().clear();
  DisplayManager::getInstance().setFont(ArialMT_Plain_10);
  DisplayManager::getInstance().drawString(0, 0, "MS11-control");
  DisplayManager::getInstance().drawString(0, 15, "Update complete!");
  if (newVersion.valid) {
    DisplayManager::getInstance().drawString(0, 30, "v" + newVersion.toString());
  }
  DisplayManager::getInstance().updateDisplay();
  
  delay(2000);  // Show success message briefly
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
  if (!ProbeManager::getInstance().begin()) {
    Serial.println("WARNING: Probe Manager initialization did not detect any temperature sensors");
  }
  
  // Update display managers with version info after delays
  delayWithBlink(2000);

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
  
  // Clear and show MS11 Master text
  DisplayManager::getInstance().clear();
  delayWithBlink(100);
  Serial.println("Displaying MS11 Master text...");
  DisplayManager::getInstance().setFont(ArialMT_Plain_10);
  DisplayManager::getInstance().drawStringCenter(28, "MS11 Master");
  DisplayManager::getInstance().updateDisplay();
  delayWithBlink(2000);
  DisplayManager::getInstance().clear();
  DisplayManager::getInstance().updateDisplay();

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
    
    // Check for MS11-control on I2C bus (detection LED pulse will be triggered in loop)
    ms11DetectionTime = millis();
    ms11DetectionShown = false;
    ms11Present = SlaveController::getInstance().ping();
    Serial.printf("[Main] MS11-control detection: %s\n", ms11Present ? "PRESENT" : "ABSENT");

    // Check if MS11-control firmware needs updating from filesystem hex
    checkAndUpdateSlaveFromFilesystem();

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
    
    float temp = AHT10Manager::getInstance().getTemperature();
    float humidity = AHT10Manager::getInstance().getHumidity();
    String sensorInfo = String(temp, 1) + "°C  " + String(humidity, 0) + "%";
    
    static String lastSensorInfo = "";
    if (sensorInfo != lastSensorInfo) {
      lastSensorInfo = sensorInfo;
      DisplayManager::getInstance().clear();
      DisplayManager::getInstance().setFont(ArialMT_Plain_10);
      DisplayManager::getInstance().drawString(0, 0, sensorInfo);
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
    
    // Show MS11-control detection (show for 2 seconds)
    if (!ms11DetectionShown) {
      if (LCDManager::getInstance().isInitialized()) {
        LCDManager::getInstance().clear();
        LCDManager::getInstance().printLine(0, "MS11-control");
        LCDManager::getInstance().printLine(1, ms11Present ? "Detected" : "Absent");
        ms11DetectionShown = true;
        ms11DetectionTime = millis();  // Reset timer for 2-second display
        
        // Trigger 500ms LED pulse on MS11-control when detected
        if (ms11Present) {
          if (SlaveController::getInstance().pulseLed(500)) {
            ledPulseStartTime = millis();
            ledPulseDurationMs = 500;
            ledPulseActive = true;
          }
        }
      }
    } else if (!lcdStatusShown && (millis() - ms11DetectionTime > 2000)) {
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
      // Heartbeat: verify MS11-control is still alive (no LED pulse to avoid interfering with slave LED modes)
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
