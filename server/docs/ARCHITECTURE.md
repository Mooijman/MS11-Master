# MS11 Telemetry Logging Server - Architecture

## Overview

The MS11 Telemetry Logging Server is a complete solution for collecting, storing, and visualizing telemetry data from multiple MS11 devices. The system consists of three main components:

1. **Server Backend** (PHP + MySQL) - Hosted on shared webserver
2. **Web Dashboard** (HTML/CSS/JavaScript) - Real-time data visualization
3. **ESP32 Client Library** (C++) - Device-side telemetry collection

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         MS11 Devices                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐            │
│  │  ESP32-S3   │  │  ESP32-S3   │  │  ESP32-S3   │            │
│  │  + AHT10    │  │  + AHT10    │  │  + AHT10    │  ...       │
│  │  + MS11     │  │  + MS11     │  │  + MS11     │            │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘            │
│         │                │                │                     │
│         │ HTTPS POST (JSON + API Key)     │                     │
│         └────────────────┴────────────────┘                     │
└─────────────────────────┬───────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Shared Web Server                           │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                    Apache/Nginx                            │ │
│  │  ┌─────────────────────────────────────────────────────┐  │ │
│  │  │              PHP API Layer                          │  │ │
│  │  │  ┌──────────────────┐  ┌────────────────────────┐  │  │ │
│  │  │  │  ingest.php      │  │  query.php             │  │  │ │
│  │  │  │  - Validate key  │  │  - list_devices        │  │  │ │
│  │  │  │  - Parse JSON    │  │  - latest_data         │  │  │ │
│  │  │  │  - Store data    │  │  - telemetry history   │  │  │ │
│  │  │  │  - Log events    │  │  - events              │  │  │ │
│  │  │  └─────────┬────────┘  │  - statistics          │  │  │ │
│  │  │            │            └────────────┬───────────┘  │  │ │
│  │  └────────────┼─────────────────────────┼──────────────┘  │ │
│  └───────────────┼─────────────────────────┼─────────────────┘ │
│                  │                         │                    │
│                  ▼                         ▲                    │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │                    MySQL Database                        │  │
│  │  ┌──────────────┐  ┌─────────────────┐                 │  │
│  │  │   devices    │  │ telemetry_data  │                 │  │
│  │  │  - device_id │  │  - temperature  │                 │  │
│  │  │  - name      │  │  - humidity     │                 │  │
│  │  │  - versions  │  │  - wifi_rssi    │                 │  │
│  │  │  - last_seen │  │  - uptime       │                 │  │
│  │  └──────────────┘  │  - ms11_status  │                 │  │
│  │                    └─────────────────┘                  │  │
│  │  ┌──────────────┐  ┌─────────────────┐                 │  │
│  │  │    events    │  │ statistics_     │                 │  │
│  │  │  - type      │  │   hourly        │                 │  │
│  │  │  - category  │  │  - aggregated   │                 │  │
│  │  │  - message   │  │    metrics      │                 │  │
│  │  └──────────────┘  └─────────────────┘                 │  │
│  └─────────────────────────────────────────────────────────┘  │
└──────────────────────────────┬──────────────────────────────────┘
                               │
                               │ HTTP GET (JSON)
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Web Dashboard                            │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                   index.html + style.css                   │ │
│  │  ┌─────────────────────────────────────────────────────┐  │ │
│  │  │  Summary Statistics                                  │  │ │
│  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐  │  │ │
│  │  │  │ Active  │ │  Avg    │ │  Avg    │ │ Total   │  │  │ │
│  │  │  │ Devices │ │  Temp   │ │Humidity │ │  Data   │  │  │ │
│  │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘  │  │ │
│  │  └─────────────────────────────────────────────────────┘  │ │
│  │  ┌─────────────────────────────────────────────────────┐  │ │
│  │  │  Device Cards (per device)                          │  │ │
│  │  │  ┌────────────────────────────────────────────────┐ │  │ │
│  │  │  │ Device Name              [Online/Offline]      │ │  │ │
│  │  │  │ Last seen: 2 minutes ago                       │ │  │ │
│  │  │  │                                                 │ │  │ │
│  │  │  │ Temperature │ Humidity │ WiFi │ Memory │...    │ │  │ │
│  │  │  │    23.5°C   │   45%    │ -65  │ 240 KB │       │ │  │ │
│  │  │  └────────────────────────────────────────────────┘ │  │ │
│  │  └─────────────────────────────────────────────────────┘  │ │
│  │  Auto-refresh: 30 seconds                                 │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
           ▲
           │ HTTPS (Browser access)
           │
        End User
```

## Data Flow

### 1. Telemetry Collection (ESP32 → Server)

```
ESP32 Device:
  1. TelemetryClient.update() called in loop()
  2. Check if interval elapsed (default 60s)
  3. Build JSON payload with:
     - Device info (ID, name, versions, IP, MAC)
     - Sensor data (temperature, humidity)
     - System metrics (uptime, heap, WiFi RSSI)
     - MS11 status
  4. HTTP POST to /api/ingest.php with API key
  5. Server validates, stores, responds

Response Codes:
  201 Created - Data stored successfully
  401 Unauthorized - Invalid API key
  400 Bad Request - Invalid JSON
  500 Server Error - Database issue
```

### 2. Data Storage (Server → Database)

```sql
-- Device registration/update
INSERT INTO devices ... ON DUPLICATE KEY UPDATE ...

-- Telemetry data storage
INSERT INTO telemetry_data (device_id, timestamp, temperature, ...)

-- Event logging (optional)
INSERT INTO events (device_id, event_type, category, message)

-- Hourly aggregation (cron job)
INSERT INTO statistics_hourly ... ON DUPLICATE KEY UPDATE ...
```

### 3. Data Visualization (Dashboard → API → Database)

```javascript
// Dashboard requests data
GET /api/query.php?action=list_devices
GET /api/query.php?action=latest_data

// Server queries database
SELECT * FROM devices ORDER BY last_seen DESC
SELECT * FROM telemetry_data WHERE id IN (SELECT MAX(id) ...)

// Dashboard updates UI
updateStatistics(data)
updateDeviceCards(data)

// Auto-refresh every 30s
setInterval(loadDashboardData, 30000)
```

## File Structure

```
MS11-Master/
├── server/                           # Telemetry server (deploy to web hosting)
│   ├── api/                          # API endpoints
│   │   ├── ingest.php               # POST - Receive telemetry data
│   │   └── query.php                # GET - Query data (multiple actions)
│   ├── config/                       # Configuration
│   │   ├── database.php             # Database config template
│   │   ├── database.local.php       # Local config (not in git)
│   │   └── .htaccess                # Protect config directory
│   ├── dashboard/                    # Web dashboard
│   │   ├── index.html               # Dashboard UI
│   │   └── style.css                # Styles (from MS11-Master)
│   ├── docs/                         # Documentation
│   │   ├── README.md                # API documentation
│   │   ├── DEPLOYMENT.md            # Deployment guide
│   │   └── ESP32_INTEGRATION.md     # Integration examples
│   ├── maintenance/                  # Maintenance scripts
│   │   ├── aggregate_statistics.php # Hourly aggregation (cron)
│   │   └── cleanup_old_data.php     # Data cleanup (cron)
│   ├── sql/                          # Database schema
│   │   └── schema.sql               # CREATE TABLE statements
│   ├── .htaccess                     # Apache config
│   ├── .gitignore                    # Ignore local config
│   ├── test_api.php                  # API test script
│   └── README.md                     # Server overview
├── include/
│   └── telemetry_client.h           # ESP32 client header
└── src/
    └── telemetry_client.cpp         # ESP32 client implementation
```

## Database Schema

### devices
Primary table for device registry
- `device_id` (PK): Unique device identifier
- `device_name`: Friendly name
- `firmware_version`, `filesystem_version`: Version tracking
- `ip_address`, `mac_address`: Network info
- `first_seen`, `last_seen`: Timestamps
- `is_active`: Boolean flag

### telemetry_data
Time-series data storage
- `id` (PK, auto-increment): Unique record ID
- `device_id` (FK): References devices
- `timestamp`: When data was recorded
- `temperature`, `humidity`: Sensor readings
- `uptime_seconds`, `free_heap`, `wifi_rssi`: System metrics
- `ms11_connected`: MS11 control status
- `extra_data`: JSON for extensibility

Indexes: `(device_id, timestamp)`, `timestamp`

### events
Important device events
- `id` (PK): Unique event ID
- `device_id` (FK): References devices
- `timestamp`: When event occurred
- `event_type`: info, warning, error, critical
- `event_category`: boot, network, sensor, update, etc.
- `message`: Event description
- `details`: JSON for additional data

### statistics_hourly
Pre-aggregated statistics for performance
- `device_id`, `hour_timestamp`: Composite key
- `temp_avg`, `temp_min`, `temp_max`: Temperature stats
- `humidity_avg`, `humidity_min`, `humidity_max`: Humidity stats
- `uptime_avg`, `free_heap_avg`, `wifi_rssi_avg`: System stats
- `sample_count`: Number of samples in hour

## Security Features

1. **API Key Authentication**: Each device requires unique API key
2. **Prepared Statements**: All SQL queries use parameter binding
3. **Input Validation**: JSON parsing with error handling
4. **HTTPS Support**: Encrypted data transmission (recommended)
5. **Config Protection**: .htaccess prevents access to config files
6. **XSS Prevention**: HTML escaping in dashboard
7. **CORS Control**: Configurable cross-origin access

## Performance Optimization

1. **Indexed Queries**: Database indexes on frequently queried columns
2. **Hourly Aggregation**: Pre-computed statistics reduce query load
3. **Connection Pooling**: Singleton PDO connection
4. **Minimal Payload**: Only necessary data transmitted
5. **Caching**: Static assets cached in browser
6. **Compression**: gzip compression enabled

## Maintenance Tasks

### Automated (Cron Jobs)

```bash
# Aggregate hourly statistics (every hour)
0 * * * * php /path/to/server/maintenance/aggregate_statistics.php

# Clean old data (daily at 2 AM)
0 2 * * * php /path/to/server/maintenance/cleanup_old_data.php

# Backup database (daily at 3 AM)
0 3 * * * mysqldump -u user -p'pass' ms11_telemetry | gzip > /backups/ms11_$(date +\%Y\%m\%d).sql.gz
```

### Manual

- Review error logs: `tail -f /var/log/apache2/error.log`
- Check database size: SQL query in cleanup script
- Monitor API usage: Check Apache access logs
- Update API keys: Edit `config/database.local.php`

## Integration Example

```cpp
// In config.h
#define TELEMETRY_ENABLED true
#define TELEMETRY_SERVER_URL "https://your-server.com/server/api/ingest.php"
#define TELEMETRY_API_KEY "your_device_api_key"
#define TELEMETRY_INTERVAL 60000  // 60 seconds

// In main.cpp
#include "telemetry_client.h"

void setup() {
    TelemetryClient& telemetry = TelemetryClient::getInstance();
    telemetry.begin(TELEMETRY_SERVER_URL, TELEMETRY_API_KEY);
    telemetry.setDeviceInfo("MS11-001", "Kitchen MS11");
    telemetry.enable(true);
}

void loop() {
    // Update sensor readings
    telemetry.setTemperature(23.5);
    telemetry.setHumidity(45.2);
    telemetry.setMS11Connected(true);
    
    // Auto-send at configured interval
    telemetry.update();
}
```

## Future Enhancements

- [ ] Real-time charts (Chart.js or similar)
- [ ] Email/SMS alerts for critical events
- [ ] Data export (CSV, JSON download)
- [ ] User authentication for dashboard
- [ ] Device groups and filtering
- [ ] Custom dashboard widgets
- [ ] Mobile app (responsive design done)
- [ ] WebSocket for real-time updates
- [ ] Multi-tenant support
- [ ] API rate limiting
- [ ] Prometheus/Grafana integration

## Technology Stack

**Backend:**
- PHP 7.4+ (PDO MySQL extension)
- MySQL 5.7+ / MariaDB 10.2+
- Apache/Nginx web server

**Frontend:**
- HTML5 + CSS3 (CSS custom properties)
- Vanilla JavaScript (ES6+)
- No framework dependencies

**ESP32:**
- Arduino framework
- HTTPClient library
- ArduinoJson library

**Styling:**
- Matches MS11-Master design system
- Responsive grid layout
- Gradient stat cards
- Auto-refreshing data
