/**
 * MS11 Telemetry Client Implementation
 */

#include "telemetry_client.h"
#include <WiFi.h>

TelemetryClient::TelemetryClient() 
    : _interval(60000),  // Default 60 seconds
      _enabled(false),
      _temperature(0),
      _humidity(0),
      _ms11Connected(false),
      _hasTemperature(false),
      _hasHumidity(false),
      _hasMS11Status(false),
      _lastSendTime(0),
      _lastAttemptTime(0),
      _lastSendSuccess(false) {
}

void TelemetryClient::begin(const String& serverUrl, const String& apiKey) {
    _serverUrl = serverUrl;
    _apiKey = apiKey;
    
    if (_serverUrl.length() > 0 && _apiKey.length() > 0) {
        _enabled = true;
        Serial.println("[Telemetry] Client initialized");
        Serial.printf("[Telemetry] Server: %s\n", _serverUrl.c_str());
    } else {
        _enabled = false;
        Serial.println("[Telemetry] Client disabled - no server URL or API key");
    }
}

void TelemetryClient::setDeviceInfo(const String& deviceId, const String& deviceName) {
    _deviceId = deviceId;
    _deviceName = deviceName;
}

void TelemetryClient::setInterval(unsigned long intervalMs) {
    _interval = intervalMs;
}

void TelemetryClient::enable(bool enabled) {
    _enabled = enabled;
    if (enabled) {
        Serial.println("[Telemetry] Client enabled");
    } else {
        Serial.println("[Telemetry] Client disabled");
    }
}

void TelemetryClient::setTemperature(float temp) {
    _temperature = temp;
    _hasTemperature = true;
}

void TelemetryClient::setHumidity(float humidity) {
    _humidity = humidity;
    _hasHumidity = true;
}

void TelemetryClient::setMS11Connected(bool connected) {
    _ms11Connected = connected;
    _hasMS11Status = true;
}

bool TelemetryClient::sendTelemetry() {
    if (!_enabled) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        _lastError = "WiFi not connected";
        return false;
    }

    // Don't spam the server - enforce minimum interval
    unsigned long now = millis();
    if (now - _lastAttemptTime < 5000) { // Min 5 seconds between attempts
        return false;
    }
    _lastAttemptTime = now;

    HTTPClient http;
    http.begin(_serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", _apiKey);
    http.setTimeout(10000); // 10 second timeout

    // Build JSON payload
    StaticJsonDocument<512> doc;
    
    // Device information
    doc["device_id"] = _deviceId;
    doc["device_name"] = _deviceName;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["filesystem_version"] = FILESYSTEM_VERSION;
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mac_address"] = WiFi.macAddress();
    
    // Telemetry data
    if (_hasTemperature) {
        doc["temperature"] = _temperature;
    }
    if (_hasHumidity) {
        doc["humidity"] = _humidity;
    }
    if (_hasMS11Status) {
        doc["ms11_connected"] = _ms11Connected;
    }
    
    // System metrics
    doc["uptime_seconds"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();

    // Serialize JSON
    String payload;
    serializeJson(doc, payload);

    Serial.printf("[Telemetry] Sending data to server (%d bytes)\n", payload.length());

    // Send POST request
    int httpCode = http.POST(payload);

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String response = http.getString();
        Serial.printf("[Telemetry] Success: %s\n", response.c_str());
        
        _lastSendTime = now;
        _lastSendSuccess = true;
        _lastError = "";
        
        http.end();
        return true;
    } else {
        _lastSendSuccess = false;
        if (httpCode > 0) {
            _lastError = "HTTP " + String(httpCode) + ": " + http.getString();
            Serial.printf("[Telemetry] Error: %s\n", _lastError.c_str());
        } else {
            _lastError = "Connection failed: " + http.errorToString(httpCode);
            Serial.printf("[Telemetry] Connection error: %s\n", _lastError.c_str());
        }
        
        http.end();
        return false;
    }
}

void TelemetryClient::sendEvent(const String& type, const String& category, const String& message) {
    if (!_enabled || WiFi.status() != WL_CONNECTED) {
        return;
    }

    HTTPClient http;
    http.begin(_serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", _apiKey);
    http.setTimeout(10000);

    // Build JSON payload with event
    StaticJsonDocument<512> doc;
    doc["device_id"] = _deviceId;
    doc["device_name"] = _deviceName;
    
    JsonObject event = doc.createNestedObject("event");
    event["type"] = type;
    event["category"] = category;
    event["message"] = message;

    String payload;
    serializeJson(doc, payload);

    Serial.printf("[Telemetry] Sending event: %s - %s\n", category.c_str(), message.c_str());

    int httpCode = http.POST(payload);
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        Serial.println("[Telemetry] Event sent successfully");
    } else {
        Serial.printf("[Telemetry] Event failed: HTTP %d\n", httpCode);
    }

    http.end();
}

void TelemetryClient::update() {
    if (!_enabled) {
        return;
    }

    unsigned long now = millis();
    
    // Check if it's time to send telemetry
    if (now - _lastSendTime >= _interval) {
        sendTelemetry();
    }
}
