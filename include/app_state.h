#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// Forward declarations
class GitHubUpdater;
class MD11SlaveUpdate;
class WiFiManager;

// ============================================================================
// SHARED APPLICATION STATE
// ============================================================================
// Extern declarations for global state defined in main.cpp.
// Include this header in any module that needs access to shared state.

// Global server and DNS
extern AsyncWebServer server;
extern DNSServer dnsServer;

// Global instances
extern Preferences preferences;
extern GitHubUpdater* githubUpdater;
extern WiFiManager* wifiManager;
extern MD11SlaveUpdate* md11SlaveUpdater;

// Application mode flags
extern bool isAPMode;

// Scheduled reboot state
extern bool rebootScheduled;
extern unsigned long rebootTime;

// OTA update flag (disables display updates during firmware/FS writes)
extern bool otaUpdateInProgress;

// WiFi scan cache (used by captive portal)
extern String cachedScanResults;
extern unsigned long lastScanTime;
extern bool scanInProgress;

#endif // APP_STATE_H
