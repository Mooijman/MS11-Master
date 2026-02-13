# MS11 Telemetry Server - Quick Reference

## Quick Links

- **Dashboard**: `https://your-server.com/server/dashboard/`
- **API Docs**: [docs/README.md](README.md)
- **Deployment**: [docs/DEPLOYMENT.md](DEPLOYMENT.md)
- **ESP32 Integration**: [docs/ESP32_INTEGRATION.md](ESP32_INTEGRATION.md)
- **Architecture**: [docs/ARCHITECTURE.md](ARCHITECTURE.md)

## Common Commands

### Database Setup
```bash
# Create database and tables
mysql -u root -p < sql/schema.sql

# Create user
mysql -u root -p -e "CREATE USER 'ms11_user'@'localhost' IDENTIFIED BY 'password'; GRANT SELECT, INSERT, UPDATE, DELETE ON ms11_telemetry.* TO 'ms11_user'@'localhost'; FLUSH PRIVILEGES;"
```

### Configuration
```bash
# Copy and edit config
cp config/database.php config/database.local.php
nano config/database.local.php

# Generate API key
openssl rand -base64 32
```

### Testing
```bash
# Test API
php test_api.php

# Check database
mysql -u ms11_user -p ms11_telemetry -e "SELECT device_id, device_name, last_seen FROM devices;"
```

### Maintenance
```bash
# Aggregate statistics
php maintenance/aggregate_statistics.php

# Clean old data (dry run)
php maintenance/cleanup_old_data.php --dry-run

# Clean old data (live)
php maintenance/cleanup_old_data.php
```

## API Quick Reference

### Ingest Data
```bash
curl -X POST https://your-server.com/server/api/ingest.php \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your_api_key" \
  -d '{
    "device_id": "MS11-001",
    "device_name": "Kitchen",
    "temperature": 23.5,
    "humidity": 45.2,
    "wifi_rssi": -65
  }'
```

### Query Devices
```bash
# List all devices
curl https://your-server.com/server/api/query.php?action=list_devices

# Latest data
curl https://your-server.com/server/api/query.php?action=latest_data

# Device telemetry
curl "https://your-server.com/server/api/query.php?action=telemetry&device_id=MS11-001&limit=100"

# Events
curl "https://your-server.com/server/api/query.php?action=events&limit=50"
```

## ESP32 Quick Start

### 1. Add to config.h
```cpp
#define TELEMETRY_ENABLED true
#define TELEMETRY_SERVER_URL "https://your-server.com/server/api/ingest.php"
#define TELEMETRY_API_KEY "your_api_key"
#define TELEMETRY_INTERVAL 60000
```

### 2. Include header
```cpp
#include "telemetry_client.h"
```

### 3. Initialize in setup()
```cpp
TelemetryClient& telemetry = TelemetryClient::getInstance();
telemetry.begin(TELEMETRY_SERVER_URL, TELEMETRY_API_KEY);
telemetry.setDeviceInfo("MS11-001", "Device Name");
telemetry.enable(true);
```

### 4. Update in loop()
```cpp
telemetry.setTemperature(temp);
telemetry.setHumidity(humidity);
telemetry.update();
```

## Troubleshooting

### Dashboard shows no data
1. Check API endpoint in browser: `/server/api/query.php?action=list_devices`
2. Check browser console (F12) for JavaScript errors
3. Verify database has data: `SELECT * FROM devices;`

### Device not sending data
1. Check Serial monitor for telemetry logs
2. Verify TELEMETRY_ENABLED is true
3. Check server URL and API key
4. Test server reachability: `ping your-server.com`

### Database connection failed
1. Check credentials in `config/database.local.php`
2. Verify MySQL is running: `systemctl status mysql`
3. Test connection: `mysql -u ms11_user -p`

### API returns 401
1. Check API key in device config matches server
2. Verify API_KEYS array in `config/database.php`
3. Check X-API-Key header is sent correctly

## File Locations

### Server Files (upload to hosting)
```
public_html/server/
├── api/ingest.php, query.php
├── config/database.local.php
├── dashboard/index.html, style.css
└── sql/schema.sql
```

### ESP32 Files
```
MS11-Master/
├── include/telemetry_client.h
├── src/telemetry_client.cpp
└── include/config.h (add TELEMETRY_ defines)
```

## Default Settings

- **API Port**: 80/443 (HTTP/HTTPS)
- **Telemetry Interval**: 60 seconds
- **Data Retention**: 90 days
- **Dashboard Refresh**: 30 seconds
- **Device Timeout**: 5 minutes (offline status)

## Response Codes

- `200 OK` - Query successful
- `201 Created` - Data stored successfully
- `400 Bad Request` - Invalid JSON or missing fields
- `401 Unauthorized` - Invalid API key
- `405 Method Not Allowed` - Wrong HTTP method
- `500 Server Error` - Database or server error

## Database Tables

- **devices** - Device registry (17 columns)
- **telemetry_data** - Time-series data (10 columns)
- **events** - Event log (7 columns)
- **statistics_hourly** - Aggregated stats (12 columns)

## Useful SQL Queries

```sql
-- Latest reading per device
SELECT d.device_name, t.* FROM telemetry_data t
JOIN devices d ON t.device_id = d.device_id
WHERE t.id IN (SELECT MAX(id) FROM telemetry_data GROUP BY device_id);

-- Device uptime
SELECT device_id, device_name, 
       TIMESTAMPDIFF(SECOND, first_seen, last_seen) as uptime_seconds
FROM devices;

-- Event summary
SELECT event_type, COUNT(*) as count 
FROM events 
GROUP BY event_type;

-- Average temperature per hour
SELECT DATE_FORMAT(hour_timestamp, '%Y-%m-%d %H:00') as hour,
       AVG(temp_avg) as avg_temp
FROM statistics_hourly
WHERE device_id = 'MS11-001'
GROUP BY DATE_FORMAT(hour_timestamp, '%Y-%m-%d %H:00')
ORDER BY hour DESC
LIMIT 24;
```

## Performance Tips

1. **Enable OPcache** - PHP bytecode caching
2. **Use indexes** - Already configured in schema
3. **Aggregate data** - Run maintenance/aggregate_statistics.php hourly
4. **Clean old data** - Run maintenance/cleanup_old_data.php daily
5. **Monitor logs** - Check for errors regularly

## Security Checklist

- [ ] Change default API keys
- [ ] Use HTTPS in production
- [ ] Protect config directory (.htaccess)
- [ ] Set strong database password
- [ ] Limit database user permissions
- [ ] Regular backups configured
- [ ] File permissions correct (644 for PHP, 600 for config)
- [ ] Error display disabled in production
- [ ] CORS restricted to specific domains

## Support

- GitHub Issues: https://github.com/Mooijman/MS11-Master/issues
- Documentation: See `server/docs/` directory
- Test Script: `php server/test_api.php`
