#ifndef WEB_SERVER_ROUTES_H
#define WEB_SERVER_ROUTES_H

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// ============================================================================
// WEB SERVER ROUTE REGISTRATION
// ============================================================================
// All AsyncWebServer route handlers are registered here, grouped by
// functionality. Called from setup() in main.cpp.

// Register all STA-mode routes (WiFi station connected).
// Includes: page routes, settings API, I2C API, update API, file API.
void registerSTARoutes(AsyncWebServer& server);

// Register AP-mode routes (captive portal for WiFi configuration).
void registerAPRoutes(AsyncWebServer& server, DNSServer& dnsServer);

#endif // WEB_SERVER_ROUTES_H
