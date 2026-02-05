#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

class WiFiManager {
public:
  WiFiManager(Preferences& prefs);
  
  // Initialize WiFi connection
  bool begin(const String& ssid, const String& password, 
            const String& ip, const String& gateway, const String& netmask,
            bool useDHCP, unsigned long timeout = 10000);

private:
  Preferences& preferences;
};

#endif // WIFI_MANAGER_H
