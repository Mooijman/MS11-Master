#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// FEATURE FLAGS - Module Enable/Disable
// ============================================================================
// Comment out any feature to disable it and reduce firmware size
// Core modules (always enabled): WiFi Manager, Settings, Web Server, Index

// Display & Debugging
#define FEATURE_OLED_DISPLAY        // SSD1306 OLED display support
#define FEATURE_LCD_16X2            // 16x2 LCD with I2C PCF8574 backpack
#define FEATURE_SEESAW_ROTARY       // Seesaw I2C rotary encoder
#define FEATURE_GPIO_VIEWER         // Real-time GPIO monitoring (debug)

// File System & Management
#define FEATURE_FILE_MANAGER        // Web-based LittleFS file manager (debug)

// Diagnostics & Tools
#define FEATURE_I2C_SCANNER         // I2C bus scanner and diagnostics (debug)

// Updates & Maintenance
#define FEATURE_OTA_UPDATES         // ArduinoOTA local network updates
#define FEATURE_GITHUB_UPDATES      // Automatic updates via GitHub releases

// Future modules (not yet implemented)
// #define FEATURE_MQTT_CLIENT      // MQTT messaging
// #define FEATURE_NTP_TIME         // Network time synchronization
// #define FEATURE_MDNS             // mDNS/Bonjour (esp32.local)
// #define FEATURE_WEBSOCKET        // WebSocket real-time communication
// #define FEATURE_AUTH             // Web interface authentication

// ============================================================================
// FIRMWARE VERSION TRACKING
// ============================================================================
// Format: type-year-major.minor.patch
#define FIRMWARE_VERSION "fw-2026.1.1.20"
#define FILESYSTEM_VERSION "fs-2026.1.1.20"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// ============================================================================
// HARDWARE CONFIGURATION - XIAO ESP32-S3 I2C DUAL BUS SETUP (PINS SWAPPED)
// ============================================================================
// BUS 0 (I2C0/Wire - Display Devices):
//   GPIO8 (D9) = SDA (LCD via LiquidCrystal_I2C)
//   GPIO9 (D10) = SCL
//   Speed: 100kHz (conservative, reliable)
//   Devices: LCD 16x2 (0x27), OLED Display (0x3C), Seesaw (0x36)
//   Pull-ups: 4.7kΩ to 3.3V
//   Note: LiquidCrystal_I2C only supports Wire (I2C0), so all display devices on this bus
//
// BUS 1 (I2C1/Wire1 - Critical Slave):
//   GPIO5 (D4) = SDA
//   GPIO6 (D5) = SCL  
//   Speed: 100kHz (conservative, reliable)
//   Device: Slave Controller (ATmega328P @ 0x30)
//   Pull-ups: 4.7kΩ to 3.3V
//
// OLED Display (SSD1306) - XIAO S3
#define OLED_I2C_ADDRESS 0x3C
#define OLED_DISPLAY_BUS 0  // Uses Bus 0 (Wire/I2C0 - GPIO8/9)
#define OLED_SDA_PIN 8      // GPIO8 on XIAO S3 (D9) - Bus 0
#define OLED_SCL_PIN 9      // GPIO9 on XIAO S3 (D10) - Bus 0

// LCD 16x2 Display (PCF8574 I2C backpack) - XIAO S3
#define LCD_I2C_ADDRESS 0x27        // PCF8574 I2C address (alt: 0x3F)
#define LCD_DISPLAY_BUS 0   // Uses Bus 0 (Wire/I2C0) - GPIO8/9 (LiquidCrystal_I2C doesn't support Wire1)
#define LCD_COLS 16
#define LCD_ROWS 2

// Seesaw Rotary Encoder (Adafruit) - XIAO S3
#define SEESAW_I2C_ADDRESS 0x36     // Seesaw default address
#define SEESAW_DISPLAY_BUS 0        // Uses Bus 0 (Wire/I2C0) - GPIO8/9

// Slave Controller (ATmega328P) - XIAO S3
#define SLAVE_I2C_BUS 1     // Uses Bus 1 (Wire1/I2C1 - GPIO5/6)
#define SLAVE_SDA_PIN 5     // GPIO5 on XIAO S3 (D4) - Bus 1 (Wire1 SDA)
#define SLAVE_SCL_PIN 6     // GPIO6 on XIAO S3 (D5) - Bus 1 (Wire1 SCL)

// ============================================================================
// GPIO CONFIGURATION - XIAO ESP32-S3 Digital I/O
// ============================================================================
// XIAO S3 Available pins: D0-D4, D7, D10, D11-D21 (D5,D6,D8,D9 used for I2C)
//
// GPIO Control Pins:
#define GPIO_POWER_SWITCH    4      // D4 - Power switch input (active high)
#define GPIO_CONTROL_BTN1    3      // D3 - Control button 1 (active low)
#define GPIO_CONTROL_BTN2    2      // D2 - Control button 2 (active low)
#define GPIO_STATUS_LED      1      // D1 - Status LED output (PWM capable)

// GPIO Configuration
#define GPIO_DEBOUNCE_MS     20     // Debounce time for buttons (ms)
#define GPIO_LONGPRESS_MS    1000   // Long press detection (ms)

// ============================================================================
// NETWORK CONFIGURATION
// ============================================================================
// Access Point Mode
#define AP_SSID "ESP-WIFI-MANAGER"
#define AP_PASSWORD ""  // Open network
#define AP_IP_ADDRESS IPAddress(192, 168, 4, 1)
#define AP_GATEWAY IPAddress(192, 168, 4, 1)
#define AP_SUBNET IPAddress(255, 255, 255, 0)

// DNS Server
#define DNS_PORT 53

// Web Server
#define WEB_SERVER_PORT 80

// GPIO Viewer (optional debug tool)
#define GPIO_VIEWER_PORT 5555
#define GPIO_VIEWER_SAMPLING_INTERVAL 100  // milliseconds

// ArduinoOTA
#define OTA_HOSTNAME "ESP32-Base"
#define OTA_PORT 3232
#define OTA_PASSWORD ""  // No password by default

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
// WiFi connection timeout
#define WIFI_CONNECT_TIMEOUT 10000  // 10 seconds in milliseconds

// Update check intervals
#define UPDATE_CHECK_INTERVAL (6UL * 60UL * 60UL * 1000UL)  // 6 hours
#define UPDATE_MAX_CHECKS_PER_DAY 4

// WiFi scan caching
#define WIFI_SCAN_CACHE_INTERVAL 10000  // 10 seconds

// Display timing
#define DISPLAY_IP_SHOW_DURATION 3000  // 3 seconds
#define DISPLAY_REBOOT_MESSAGE_DURATION 2000  // 2 seconds

// Reboot delay
#define REBOOT_DELAY 2000  // 2 seconds

// Loop delay
#define LOOP_DELAY 10  // milliseconds

// ============================================================================
// HTTP CLIENT CONFIGURATION
// ============================================================================
#define HTTP_TIMEOUT 15000  // 15 seconds
#define HTTP_USER_AGENT "ESP32-OTA-Client"

// ============================================================================
// DEFAULT SETTINGS
// ============================================================================
// Default GitHub release URL (can be changed via web interface)
#define DEFAULT_UPDATE_URL "https://api.github.com/repos/Mooijman/MS11-Master/releases/latest"

// Default feature states (security-conscious defaults)
#define DEFAULT_DEBUG_ENABLED false
#define DEFAULT_GPIO_VIEWER_ENABLED false
#define DEFAULT_OTA_ENABLED false
#define DEFAULT_UPDATES_ENABLED false
#define DEFAULT_DHCP_ENABLED true
#define DEFAULT_NTP_ENABLED true
#define DEFAULT_TIMEZONE "UTC"

// Default network settings
#define DEFAULT_NETMASK "255.255.255.0"

// ============================================================================
// NVS NAMESPACES
// ============================================================================
#define NVS_NAMESPACE_CONFIG "config"
#define NVS_NAMESPACE_OTA "ota"

// ============================================================================
// FILE PATHS
// ============================================================================
#define LITTLEFS_PARTITION_LABEL "littlefs"
#define LITTLEFS_BASE_PATH "/littlefs"
#define LITTLEFS_MAX_FILES 10

// Web interface files (served from LittleFS)
#define FILE_INDEX "/index.html"
#define FILE_SETTINGS "/settings.html"
#define FILE_UPDATE "/update.html"
#define FILE_FILES "/files.html"
#define FILE_WIFIMANAGER "/wifimanager.html"
#define FILE_CONFIRM "/confirm.html"
#define FILE_STYLE "/style.css"

// ============================================================================
// HTTP PARAMETERS
// ============================================================================
// WiFi Manager form parameters
#define PARAM_SSID "ssid"
#define PARAM_PASSWORD "pass"
#define PARAM_IP "ip"
#define PARAM_GATEWAY "gateway"
#define PARAM_DHCP "dhcp"

// Legacy parameter names (for backwards compatibility)
#define PARAM_INPUT_1 PARAM_SSID
#define PARAM_INPUT_2 PARAM_PASSWORD
#define PARAM_INPUT_3 PARAM_IP
#define PARAM_INPUT_4 PARAM_GATEWAY
#define PARAM_INPUT_5 PARAM_DHCP

// Settings form parameters
#define PARAM_DEBUG "debug"
#define PARAM_GPIO_VIEWER "gpioviewer"
#define PARAM_OTA "ota"
#define PARAM_UPDATES "updates"
#define PARAM_UPDATE_URL "updateUrl"
#define PARAM_GITHUB_TOKEN "githubToken"
#define PARAM_NTP "ntp"
#define PARAM_TIMEZONE "timezone"

// Update parameters
#define PARAM_UPDATE_TYPE "type"

// ============================================================================
// API ENDPOINTS
// ============================================================================
#define API_UPDATE_STATUS "/api/update/status"
#define API_UPDATE_CHECK "/api/update/check"
#define API_UPDATE_INSTALL "/api/update/install"
#define API_WIFI_SCAN "/api/scan"
#define API_WIFI_CLEAR "/api/clear"
#define API_REBOOT "/api/reboot"

// ============================================================================
// DEBUG CONFIGURATION
// ============================================================================
// Serial baud rate
#define SERIAL_BAUD_RATE 115200

// Debug levels are set in platformio.ini:
// -D CORE_DEBUG_LEVEL=0 (NONE)
// -D CORE_DEBUG_LEVEL=1 (ERROR)
// -D CORE_DEBUG_LEVEL=2 (WARN)
// -D CORE_DEBUG_LEVEL=3 (INFO)
// -D CORE_DEBUG_LEVEL=4 (DEBUG)
// -D CORE_DEBUG_LEVEL=5 (VERBOSE)

// ============================================================================
// BUFFER SIZES
// ============================================================================
#define JSON_BUFFER_SIZE 4096
#define CONFIG_LINE_BUFFER_SIZE 128

// ============================================================================
// UPDATE CONFIGURATION
// ============================================================================
// GitHub asset naming convention
#define FIRMWARE_ASSET_ESP32 "firmware-esp32.bin"
#define FIRMWARE_ASSET_ESP32S3 "firmware-esp32s3.bin"
#define LITTLEFS_ASSET_ESP32 "littlefs-esp32.bin"
#define LITTLEFS_ASSET_ESP32S3 "littlefs-esp32s3.bin"

// Update download buffer
#define UPDATE_DOWNLOAD_BUFFER_SIZE 1024

// ============================================================================
// PLACEHOLDERS & TEMPLATES
// ============================================================================
// Template placeholders in HTML files
#define TEMPLATE_FIRMWARE_VERSION "%FIRMWARE_VERSION%"
#define TEMPLATE_FILESYSTEM_VERSION "%FILESYSTEM_VERSION%"
#define TEMPLATE_IP_ADDRESS "%IP_ADDRESS%"
#define TEMPLATE_GATEWAY "%GATEWAY%"
#define TEMPLATE_NETMASK "%NETMASK%"
#define TEMPLATE_SSID "%SSID%"
#define TEMPLATE_DHCP_CHECKED "%DHCP_CHECKED%"
#define TEMPLATE_DEBUG_CHECKED "%DEBUG_CHECKED%"
#define TEMPLATE_GPIO_VIEWER_CHECKED "%GPIOVIEWER_CHECKED%"
#define TEMPLATE_OTA_CHECKED "%OTA_CHECKED%"
#define TEMPLATE_UPDATES_CHECKED "%UPDATES_CHECKED%"
#define TEMPLATE_NTP_CHECKED "%NTP_CHECKED%"
#define TEMPLATE_TIMEZONE "%TIMEZONE%"
#define TEMPLATE_UPDATE_URL "%UPDATE_URL%"
#define TEMPLATE_GITHUB_TOKEN "%GITHUB_TOKEN%"

// ============================================================================
// NTP TIME SYNC
// ============================================================================
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"
#define NTP_SERVER_3 "time.google.com"
#define NTP_GMT_OFFSET_SEC 0
#define NTP_DAYLIGHT_OFFSET_SEC 0
#define NTP_SYNC_TIMEOUT 5000
#define NTP_VALID_TIME 1700000000UL

#endif // CONFIG_H
