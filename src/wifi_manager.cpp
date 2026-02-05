#include "wifi_manager.h"

WiFiManager::WiFiManager(Preferences& prefs) : preferences(prefs) {
}

bool WiFiManager::begin(const String& ssid, const String& password,
                       const String& ip, const String& gateway, const String& netmask,
                       bool useDHCP, unsigned long timeout) {
  if (ssid.length() == 0) {
    Serial.println("Undefined SSID.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  
  if (!useDHCP) {
    // Static IP configuration
    if (ip.length() == 0) {
      Serial.println("Static IP selected but no IP address defined.");
      return false;
    }
    
    IPAddress localIP, localGateway, localSubnet;
    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    localSubnet.fromString(netmask.c_str());

    if (!WiFi.config(localIP, localGateway, localSubnet)) {
      Serial.println("STA Failed to configure");
      return false;
    }
    Serial.println("Using Static IP");
  } else {
    Serial.println("Using DHCP");
  }
  
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long startMillis = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startMillis >= timeout) {
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
