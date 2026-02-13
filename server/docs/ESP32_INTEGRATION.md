# Telemetry Client Integration Example

## How to Integrate Telemetry in MS11-Master

### Step 1: Add Configuration to config.h

Add these definitions to `include/config.h`:

```cpp
// Telemetry Server Configuration
#define TELEMETRY_ENABLED false  // Set to true to enable telemetry
#define TELEMETRY_SERVER_URL "https://your-server.com/ms11/api/ingest.php"
#define TELEMETRY_API_KEY "your_api_key_here"
#define TELEMETRY_INTERVAL 60000  // Send interval in milliseconds (60 seconds)
#define TELEMETRY_DEVICE_NAME "MS11-Master"  // Optional friendly name
```

### Step 2: Include in main.cpp

Add to the includes section:

```cpp
#include "telemetry_client.h"
```

### Step 3: Initialize in setup()

Add to your `setup()` function:

```cpp
void setup() {
    // ... existing setup code ...
    
    #if TELEMETRY_ENABLED
    // Initialize telemetry client
    TelemetryClient& telemetry = TelemetryClient::getInstance();
    
    // Generate device ID from MAC address
    String deviceId = "MS11-" + WiFi.macAddress();
    deviceId.replace(":", "");
    
    telemetry.begin(TELEMETRY_SERVER_URL, TELEMETRY_API_KEY);
    telemetry.setDeviceInfo(deviceId, TELEMETRY_DEVICE_NAME);
    telemetry.setInterval(TELEMETRY_INTERVAL);
    telemetry.enable(true);
    
    // Send boot event
    telemetry.sendEvent("info", "boot", "Device booted successfully");
    
    Serial.println("[Main] Telemetry client initialized");
    #endif
}
```

### Step 4: Update Telemetry Data

Update telemetry data whenever you read sensors:

```cpp
void handleDisplayTasks() {
    static unsigned long lastSensorRead = 0;
    
    if (millis() - lastSensorRead >= 30000) {
        lastSensorRead = millis();
        
        // Read sensor data (existing code)
        if (AHT10Manager::getInstance().readSensor()) {
            float temp = AHT10Manager::getInstance().getTemperature();
            float humidity = AHT10Manager::getInstance().getHumidity();
            
            // ... existing display code ...
            
            #if TELEMETRY_ENABLED
            // Update telemetry client with latest sensor data
            TelemetryClient& telemetry = TelemetryClient::getInstance();
            telemetry.setTemperature(temp);
            telemetry.setHumidity(humidity);
            telemetry.setMS11Connected(ms11Present);
            #endif
        }
    }
}
```

### Step 5: Call update() in loop()

Add to your main `loop()` function:

```cpp
void loop() {
    // ... existing loop code ...
    
    #if TELEMETRY_ENABLED
    // Update telemetry client (handles automatic sending)
    TelemetryClient::getInstance().update();
    #endif
}
```

### Step 6: Send Events for Important Actions

Send events for important state changes:

```cpp
// When WiFi connects
#if TELEMETRY_ENABLED
TelemetryClient::getInstance().sendEvent("info", "network", "WiFi connected");
#endif

// When MS11 connection is lost
#if TELEMETRY_ENABLED
TelemetryClient::getInstance().sendEvent("warning", "ms11", "MS11-Control connection lost");
#endif

// When MS11 connection is restored
#if TELEMETRY_ENABLED
TelemetryClient::getInstance().sendEvent("info", "ms11", "MS11-Control connection restored");
#endif

// When firmware update starts
#if TELEMETRY_ENABLED
TelemetryClient::getInstance().sendEvent("info", "update", "Firmware update started");
#endif

// When error occurs
#if TELEMETRY_ENABLED
TelemetryClient::getInstance().sendEvent("error", "sensor", "Failed to read AHT10 sensor");
#endif
```

## Complete Example Integration

Here's a complete example showing integration in main.cpp:

```cpp
#include <Arduino.h>
#include "config.h"
#include "telemetry_client.h"

// ... other includes ...

void setup() {
    Serial.begin(921600);
    
    // ... existing WiFi and sensor setup ...
    
    #if TELEMETRY_ENABLED
    // Initialize telemetry
    TelemetryClient& telemetry = TelemetryClient::getInstance();
    String deviceId = "MS11-" + WiFi.macAddress();
    deviceId.replace(":", "");
    
    telemetry.begin(TELEMETRY_SERVER_URL, TELEMETRY_API_KEY);
    telemetry.setDeviceInfo(deviceId, TELEMETRY_DEVICE_NAME);
    telemetry.setInterval(TELEMETRY_INTERVAL);
    telemetry.enable(true);
    
    telemetry.sendEvent("info", "boot", "Device initialized");
    #endif
}

void loop() {
    // Handle web server
    // ... existing code ...
    
    // Handle display tasks
    handleDisplayTasks();
    
    // Handle network tasks
    handleNetworkTasks();
    
    #if TELEMETRY_ENABLED
    // Update telemetry (auto-sends at configured interval)
    TelemetryClient::getInstance().update();
    #endif
    
    delay(10);
}

void handleDisplayTasks() {
    static unsigned long lastSensorRead = 0;
    static float lastTemp = 0;
    static float lastHumidity = 0;
    
    // Read sensors every 30 seconds
    if (millis() - lastSensorRead >= 30000) {
        lastSensorRead = millis();
        
        if (AHT10Manager::getInstance().readSensor()) {
            float temp = AHT10Manager::getInstance().getTemperature();
            float humidity = AHT10Manager::getInstance().getHumidity();
            
            // Update display
            // ... existing code ...
            
            #if TELEMETRY_ENABLED
            // Update telemetry with sensor data
            TelemetryClient& telemetry = TelemetryClient::getInstance();
            telemetry.setTemperature(temp);
            telemetry.setHumidity(humidity);
            
            // Log significant changes
            if (abs(temp - lastTemp) > 5.0) {
                telemetry.sendEvent("warning", "sensor", 
                    "Significant temperature change: " + String(temp, 1) + "°C");
            }
            
            lastTemp = temp;
            lastHumidity = humidity;
            #endif
        }
    }
}

void handleNetworkTasks() {
    static bool wasMS11Connected = false;
    
    // Check MS11 connection
    bool ms11Connected = checkMS11Connection();
    
    #if TELEMETRY_ENABLED
    TelemetryClient& telemetry = TelemetryClient::getInstance();
    telemetry.setMS11Connected(ms11Connected);
    
    // Log connection state changes
    if (ms11Connected != wasMS11Connected) {
        if (ms11Connected) {
            telemetry.sendEvent("info", "ms11", "MS11-Control connected");
        } else {
            telemetry.sendEvent("warning", "ms11", "MS11-Control disconnected");
        }
        wasMS11Connected = ms11Connected;
    }
    #endif
}
```

## Testing

1. Set `TELEMETRY_ENABLED` to `true` in config.h
2. Configure your server URL and API key
3. Build and upload the firmware
4. Monitor Serial output for telemetry logs:
   ```
   [Telemetry] Client initialized
   [Telemetry] Server: https://your-server.com/ms11/api/ingest.php
   [Telemetry] Sending event: boot - Device initialized
   [Telemetry] Event sent successfully
   [Telemetry] Sending data to server (245 bytes)
   [Telemetry] Success: {"success":true,"message":"Data received successfully"}
   ```

5. Check the dashboard at `https://your-server.com/ms11/dashboard/`

## Troubleshooting

### No data appearing on dashboard

- Check Serial monitor for telemetry errors
- Verify TELEMETRY_ENABLED is true
- Check server URL and API key are correct
- Ensure device has internet access
- Check server logs for errors

### "HTTP 401 Unauthorized"

- API key is incorrect or not configured on server
- Check API_KEYS array in server/config/database.php

### "Connection failed"

- Server URL is incorrect or unreachable
- Check firewall/network settings
- Verify HTTPS certificate if using SSL

### High memory usage

- Reduce TELEMETRY_INTERVAL to send less frequently
- Check for memory leaks in HTTPClient usage

## Best Practices

1. **Don't spam the server** - Use reasonable intervals (60+ seconds)
2. **Handle failures gracefully** - The client automatically retries
3. **Log important events only** - Don't log every sensor reading as an event
4. **Use conditional compilation** - Keep telemetry code in `#if TELEMETRY_ENABLED` blocks
5. **Monitor resource usage** - HTTP requests use memory and CPU
6. **Use HTTPS in production** - Encrypt data in transit
7. **Rotate API keys** - Change keys periodically for security

## Performance Impact

- HTTP POST: ~200-500ms per request
- Memory: ~4KB heap usage during transmission
- Network: ~300-500 bytes per telemetry packet
- Recommended interval: 60 seconds or more

## Privacy & Security

- Data is sent over HTTPS (recommended)
- API key authenticates device
- No sensitive data should be in event messages
- Consider data retention policies
- Review server logs periodically
