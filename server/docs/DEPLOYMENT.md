# MS11 Telemetry Server - Deployment Guide

## Quick Start for Shared Hosting

### Step 1: Upload Files

Upload the `server` directory to your shared hosting account. The recommended structure is:

```
public_html/
└── ms11/
    ├── api/
    │   ├── ingest.php
    │   └── query.php
    ├── config/
    │   └── database.local.php  (create this from database.php)
    └── dashboard/
        ├── index.html
        └── style.css
```

### Step 2: Database Setup

1. Login to your hosting control panel (cPanel, Plesk, etc.)
2. Open MySQL Database or phpMyAdmin
3. Create a new database named `ms11_telemetry`
4. Create a new database user with a strong password
5. Grant ALL privileges on `ms11_telemetry` to the user
6. Import the schema: Go to phpMyAdmin → Import → Choose `sql/schema.sql`

### Step 3: Configure Database Connection

1. Copy `config/database.php` to `config/database.local.php`
2. Edit `config/database.local.php`:
```php
define('DB_HOST', 'localhost');  // Or your DB host from hosting panel
define('DB_NAME', 'ms11_telemetry');
define('DB_USER', 'your_db_username');
define('DB_PASS', 'your_db_password');
```

### Step 4: Generate API Keys

1. Generate secure random keys (use an online generator or this command):
```bash
openssl rand -base64 32
```

2. Add them to `config/database.local.php`:
```php
define('API_KEYS', [
    'AbCdEf1234567890qwerty' => 'Kitchen Device',
    'XyZ9876543210poiuyt' => 'Bedroom Device',
]);
```

### Step 5: Test the Installation

1. Open your browser and navigate to:
   - Dashboard: `https://yourdomain.com/ms11/dashboard/`
   - API Test: `https://yourdomain.com/ms11/api/query.php?action=list_devices`

2. You should see:
   - Dashboard: Empty dashboard (no devices yet)
   - API: `{"success":true,"devices":[],"count":0}`

### Step 6: Configure Your MS11 Device

Add these settings to your MS11 device configuration (see ESP32 client code):

```cpp
#define TELEMETRY_SERVER_URL "https://yourdomain.com/ms11/api/ingest.php"
#define TELEMETRY_API_KEY "AbCdEf1234567890qwerty"
#define TELEMETRY_INTERVAL 60000  // 60 seconds
```

## Security Best Practices

### File Permissions

Set appropriate permissions on your server:
```bash
chmod 644 server/api/*.php
chmod 644 server/dashboard/*.html
chmod 600 server/config/database.local.php  # Only readable by owner
```

### .htaccess Protection

Create a `.htaccess` file in `config/` directory:
```apache
# Deny access to configuration files
<Files "*.php">
    Order Allow,Deny
    Deny from all
</Files>
```

### Exclude from Git

Add to your `.gitignore`:
```
server/config/database.local.php
```

## Common Shared Hosting Configurations

### cPanel

1. File Manager → Upload files to `public_html/ms11/`
2. MySQL Databases → Create database and user
3. phpMyAdmin → Import schema.sql
4. Edit `config/database.local.php` with credentials

### Plesk

1. Files → Upload to `httpdocs/ms11/`
2. Databases → Add MySQL database
3. Import schema via phpMyAdmin
4. Configure database.local.php

### DirectAdmin

1. File Manager → Upload to `public_html/ms11/`
2. MySQL Management → Create database
3. Import schema
4. Update config file

## Troubleshooting

### "Database connection failed"

- Check database credentials in `config/database.local.php`
- Verify database exists in hosting panel
- Check if database user has correct permissions
- Some hosts use `127.0.0.1` instead of `localhost`

### "500 Internal Server Error"

- Check PHP error logs in hosting panel
- Verify PHP version is 7.4 or higher
- Check file permissions
- Look for syntax errors in config files

### "404 Not Found" for API

- Check file paths are correct
- Verify `.htaccess` allows PHP execution
- Check URL rewriting settings

### Dashboard shows connection error

- Open browser DevTools (F12) → Console
- Check if API URLs are correct
- Verify CORS headers are set
- Test API endpoint directly in browser

## Performance Optimization

### Enable OPcache

Add to `.htaccess` or ask your host to enable:
```apache
php_flag opcache.enable On
php_value opcache.memory_consumption 128
php_value opcache.max_accelerated_files 4000
```

### Database Optimization

Add indexes (already in schema.sql):
```sql
-- Verify indexes exist
SHOW INDEX FROM telemetry_data;
SHOW INDEX FROM devices;
```

### Caching Headers

Add to `.htaccess` in dashboard directory:
```apache
# Cache static assets
<IfModule mod_expires.c>
    ExpiresActive On
    ExpiresByType text/css "access plus 1 month"
    ExpiresByType text/javascript "access plus 1 month"
    ExpiresByType image/png "access plus 1 year"
</IfModule>
```

## Monitoring

### Check Database Size

```sql
SELECT 
    table_name AS 'Table',
    ROUND(((data_length + index_length) / 1024 / 1024), 2) AS 'Size (MB)'
FROM information_schema.TABLES
WHERE table_schema = 'ms11_telemetry';
```

### View Recent Activity

```sql
-- Latest data points
SELECT device_id, timestamp, temperature, humidity 
FROM telemetry_data 
ORDER BY timestamp DESC 
LIMIT 10;

-- Device activity
SELECT device_id, device_name, last_seen 
FROM devices 
ORDER BY last_seen DESC;
```

## Backup

### Database Backup (via cron job or manual)

```bash
# Manual backup
mysqldump -u ms11_user -p ms11_telemetry > backup_$(date +%Y%m%d).sql

# Automated daily backup (add to cron)
0 3 * * * mysqldump -u ms11_user -p'yourpassword' ms11_telemetry | gzip > /backups/ms11_$(date +\%Y\%m\%d).sql.gz
```

### Restore from Backup

```bash
mysql -u ms11_user -p ms11_telemetry < backup_20260213.sql
```

## Upgrading

1. Backup database and files
2. Upload new PHP files
3. Run any new migration scripts
4. Test API endpoints
5. Check dashboard functionality

## Support

For more information, see the main README.md file or create an issue on GitHub.
