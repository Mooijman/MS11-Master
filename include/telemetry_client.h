/**
 * MS11 Telemetry Client
 * 
 * Sends device telemetry data to remote logging server
 */

#ifndef TELEMETRY_CLIENT_H
#define TELEMETRY_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class TelemetryClient {
public:
    // Singleton pattern
    static TelemetryClient& getInstance() {
        static TelemetryClient instance;
        return instance;
    }

    // Configuration
    void begin(const String& serverUrl, const String& apiKey);
    void setDeviceInfo(const String& deviceId, const String& deviceName);
    void setInterval(unsigned long intervalMs);
    void enable(bool enabled);

    // Telemetry data setters
    void setTemperature(float temp);
    void setHumidity(float humidity);
    void setMS11Connected(bool connected);

    // Send telemetry data
    bool sendTelemetry();
    void sendEvent(const String& type, const String& category, const String& message);

    // Update loop - call this in main loop
    void update();

    // Status
    bool isEnabled() const { return _enabled; }
    unsigned long getLastSendTime() const { return _lastSendTime; }
    bool getLastSendSuccess() const { return _lastSendSuccess; }
    String getLastError() const { return _lastError; }

private:
    TelemetryClient();
    TelemetryClient(const TelemetryClient&) = delete;
    TelemetryClient& operator=(const TelemetryClient&) = delete;

    // Configuration
    String _serverUrl;
    String _apiKey;
    String _deviceId;
    String _deviceName;
    unsigned long _interval;
    bool _enabled;

    // Telemetry data
    float _temperature;
    float _humidity;
    bool _ms11Connected;
    bool _hasTemperature;
    bool _hasHumidity;
    bool _hasMS11Status;

    // Timing
    unsigned long _lastSendTime;
    unsigned long _lastAttemptTime;

    // Status
    bool _lastSendSuccess;
    String _lastError;
};

#endif // TELEMETRY_CLIENT_H
