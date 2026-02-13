# MS11 Telemetry Logging Server

PHP-based telemetry logging server for MS11 devices with MySQL database backend and web dashboard.

## Features

- ✅ RESTful API for data ingestion and querying
- ✅ MySQL database with optimized schema
- ✅ Web dashboard matching MS11-Master UI style
- ✅ Multi-device support with API key authentication
- ✅ Real-time updates with auto-refresh
- ✅ Event logging for important device events
- ✅ Hourly statistics aggregation for performance
- ✅ ESP32 client library for easy integration

## Quick Start

See [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md) for detailed deployment instructions.

### Server Setup

1. Upload `server` directory to your web hosting
2. Create MySQL database using `sql/schema.sql`
3. Configure database credentials in `config/database.local.php`
4. Generate API keys for your devices
5. Access dashboard at `https://your-domain.com/server/dashboard/`

### ESP32 Integration

Add to your ESP32 code:

```cpp
#include "telemetry_client.h"

void setup() {
    TelemetryClient& telemetry = TelemetryClient::getInstance();
    telemetry.begin("https://your-server.com/api/ingest.php", "your_api_key");
    telemetry.setDeviceInfo("MS11-001", "Kitchen MS11");
    telemetry.enable(true);
}

void loop() {
    telemetry.setTemperature(23.5);
    telemetry.setHumidity(45.2);
    telemetry.update();  // Sends data at configured interval
}
```

See [docs/ESP32_INTEGRATION.md](docs/ESP32_INTEGRATION.md) for complete integration guide.

## API Endpoints

### Data Ingestion
```
POST /api/ingest.php
Headers: X-API-Key: your_device_api_key
Body: JSON telemetry data
```

### Data Query
```
GET /api/query.php?action=list_devices
GET /api/query.php?action=latest_data
GET /api/query.php?action=telemetry&device_id=MS11-001
GET /api/query.php?action=events
GET /api/query.php?action=statistics&device_id=MS11-001
```

## Dashboard

![MS11 Dashboard](../data/hex100.png)

Real-time dashboard shows:
- Active device count and status
- Latest sensor readings (temperature, humidity)
- System metrics (uptime, WiFi signal, memory)
- MS11-Control connection status
- Auto-refresh every 30 seconds

## Database Schema

- **devices** - Device registry with firmware versions
- **telemetry_data** - Time-series sensor and system data
- **events** - Important device events and alerts
- **statistics_hourly** - Pre-aggregated statistics for performance

## Security

- API key authentication
- Prepared statements (SQL injection protection)
- Input validation and sanitization
- HTTPS recommended for production
- Configurable data retention

## Documentation

- [README.md](docs/README.md) - Complete API and usage documentation
- [DEPLOYMENT.md](docs/DEPLOYMENT.md) - Deployment guide for shared hosting
- [ESP32_INTEGRATION.md](docs/ESP32_INTEGRATION.md) - Integration guide for ESP32 devices

## Requirements

- PHP 7.4+ with PDO MySQL extension
- MySQL 5.7+ or MariaDB 10.2+
- Web server (Apache, Nginx)
- HTTPS (recommended)

## License

Part of the MS11-Master project.
