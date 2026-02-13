# MS11 Telemetry Logging Server

## Overview

This is a PHP-based telemetry logging server for MS11 devices with MySQL database backend. It collects, stores, and displays telemetry data from multiple MS11 devices through a web dashboard.

## Features

- **RESTful API** for data ingestion and querying
- **MySQL database** for persistent storage
- **Web dashboard** matching MS11-Master UI style
- **Real-time data updates** with auto-refresh
- **Multi-device support** with individual tracking
- **API key authentication** for secure data submission
- **Aggregated statistics** for performance optimization
- **Event logging** for important device events

## Directory Structure

```
server/
├── api/                    # API endpoints
│   ├── ingest.php         # Data ingestion endpoint (POST)
│   └── query.php          # Data query endpoint (GET)
├── config/                 # Configuration files
│   └── database.php       # Database configuration
├── dashboard/              # Web dashboard
│   ├── index.html         # Dashboard HTML
│   └── style.css          # Styles (MS11-Master compatible)
├── sql/                    # Database schema
│   └── schema.sql         # Database creation script
└── docs/                   # Documentation
    └── README.md          # This file
```

## Installation

### Requirements

- PHP 7.4 or higher with PDO MySQL extension
- MySQL 5.7 or higher (or MariaDB 10.2+)
- Web server (Apache, Nginx, etc.) with PHP support
- HTTPS recommended for production

### Database Setup

1. Create the database and tables:
```bash
mysql -u root -p < sql/schema.sql
```

2. Create a database user (or edit the schema.sql and run the commented commands):
```sql
CREATE USER 'ms11_user'@'localhost' IDENTIFIED BY 'your_secure_password';
GRANT SELECT, INSERT, UPDATE, DELETE ON ms11_telemetry.* TO 'ms11_user'@'localhost';
FLUSH PRIVILEGES;
```

### Configuration

1. Copy the database configuration:
```bash
cp config/database.php config/database.local.php
```

2. Edit `config/database.local.php` with your database credentials:
```php
define('DB_HOST', 'localhost');
define('DB_NAME', 'ms11_telemetry');
define('DB_USER', 'ms11_user');
define('DB_PASS', 'your_secure_password');
```

3. Generate API keys for your devices:
```php
define('API_KEYS', [
    'device1_secret_key_here' => 'Kitchen MS11',
    'device2_secret_key_here' => 'Bedroom MS11',
]);
```

### Web Server Setup

#### Apache

Create a `.htaccess` file in the server directory (if not using VirtualHost):
```apache
# Enable CORS (adjust for production)
Header set Access-Control-Allow-Origin "*"
Header set Access-Control-Allow-Methods "GET, POST, OPTIONS"
Header set Access-Control-Allow-Headers "Content-Type, X-API-Key"

# PHP settings
php_value upload_max_filesize 10M
php_value post_max_size 10M
```

#### Nginx

Add to your server block:
```nginx
location /server/api/ {
    try_files $uri $uri/ /server/api/$1.php?$args;
    
    # CORS headers
    add_header 'Access-Control-Allow-Origin' '*';
    add_header 'Access-Control-Allow-Methods' 'GET, POST, OPTIONS';
    add_header 'Access-Control-Allow-Headers' 'Content-Type, X-API-Key';
}
```

## API Usage

### Data Ingestion

**Endpoint:** `POST /api/ingest.php`

**Headers:**
```
Content-Type: application/json
X-API-Key: your_device_api_key
```

**Example payload:**
```json
{
  "device_id": "MS11-001",
  "device_name": "Kitchen MS11",
  "firmware_version": "2026.2.12.02",
  "filesystem_version": "2026.2.12.02",
  "ip_address": "192.168.1.100",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "temperature": 23.5,
  "humidity": 45.2,
  "uptime_seconds": 86400,
  "free_heap": 245760,
  "wifi_rssi": -65,
  "ms11_connected": true,
  "event": {
    "type": "info",
    "category": "boot",
    "message": "Device booted successfully"
  }
}
```

**Response:**
```json
{
  "success": true,
  "message": "Data received successfully",
  "device_id": "MS11-001"
}
```

### Data Query

**Endpoint:** `GET /api/query.php`

**Available actions:**

1. **List all devices:**
```
GET /api/query.php?action=list_devices
```

2. **Get latest telemetry data:**
```
GET /api/query.php?action=latest_data
GET /api/query.php?action=latest_data&device_id=MS11-001
```

3. **Get telemetry history:**
```
GET /api/query.php?action=telemetry&device_id=MS11-001&limit=100
GET /api/query.php?action=telemetry&device_id=MS11-001&start=2026-02-01&end=2026-02-13
```

4. **Get events:**
```
GET /api/query.php?action=events&device_id=MS11-001&limit=50
```

5. **Get aggregated statistics:**
```
GET /api/query.php?action=statistics&device_id=MS11-001&start=2026-02-01
```

## Dashboard

Access the dashboard at: `http://your-server/server/dashboard/`

Features:
- Real-time device status (online/offline)
- Latest telemetry readings for all devices
- Summary statistics (active devices, average temperature/humidity)
- Auto-refresh every 30 seconds
- MS11-Master consistent styling

## Security Considerations

1. **API Keys:** Always use unique, randomly generated API keys for each device
2. **HTTPS:** Use HTTPS in production to encrypt data in transit
3. **Database:** Use strong database passwords and limit permissions
4. **File Permissions:** Ensure `database.local.php` is not publicly readable
5. **Input Validation:** The API validates and sanitizes all input data
6. **SQL Injection:** All queries use prepared statements with parameter binding

## Maintenance

### Data Retention

Configure data retention in `config/database.php`:
```php
define('DATA_RETENTION_DAYS', 90); // Keep data for 90 days
```

Add a cron job to clean old data:
```bash
0 2 * * * mysql -u ms11_user -p'password' ms11_telemetry -e "DELETE FROM telemetry_data WHERE timestamp < DATE_SUB(NOW(), INTERVAL 90 DAY)"
```

### Statistics Aggregation

For better performance with large datasets, create a cron job to aggregate hourly statistics:
```bash
0 * * * * php /path/to/server/maintenance/aggregate_statistics.php
```

## Troubleshooting

### Cannot connect to database
- Check database credentials in `config/database.local.php`
- Verify MySQL service is running: `systemctl status mysql`
- Check database user permissions

### API returns 401 Unauthorized
- Verify the `X-API-Key` header is set correctly
- Check API keys in `config/database.php`

### Dashboard shows no data
- Check browser console for JavaScript errors
- Verify API endpoints are accessible
- Check PHP error logs: `tail -f /var/log/apache2/error.log`

## Future Enhancements

- [ ] Real-time charts (temperature/humidity over time)
- [ ] Email/SMS alerts for critical events
- [ ] Data export (CSV, JSON)
- [ ] User authentication for dashboard access
- [ ] Device groups and filtering
- [ ] Custom dashboard widgets
- [ ] Historical data visualization
- [ ] API rate limiting

## License

This telemetry server is part of the MS11-Master project.

## Support

For issues or questions, please create an issue in the GitHub repository.
