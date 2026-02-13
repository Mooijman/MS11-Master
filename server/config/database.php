<?php
/**
 * MS11 Telemetry Logging Server - Database Configuration
 * 
 * This file contains database connection settings.
 * Copy this file to database.local.php and configure for your environment.
 * The .local.php file should not be committed to version control.
 */

// Database configuration
define('DB_HOST', 'localhost');
define('DB_NAME', 'ms11_telemetry');
define('DB_USER', 'ms11_user');
define('DB_PASS', 'your_password_here');
define('DB_CHARSET', 'utf8mb4');

// Timezone configuration (should match device timezone)
define('TIMEZONE', 'Europe/Amsterdam');

// API security settings
define('API_KEY_REQUIRED', true);
define('API_KEYS', [
    // Add your API keys here - each device should have a unique key
    'example_key_device_1' => 'Device 1',
    'example_key_device_2' => 'Device 2',
]);

// Data retention settings (in days)
define('DATA_RETENTION_DAYS', 90); // Keep data for 90 days

// Database connection helper
function getDbConnection() {
    static $pdo = null;
    
    if ($pdo === null) {
        try {
            $dsn = "mysql:host=" . DB_HOST . ";dbname=" . DB_NAME . ";charset=" . DB_CHARSET;
            $options = [
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
                PDO::ATTR_EMULATE_PREPARES => false,
            ];
            $pdo = new PDO($dsn, DB_USER, DB_PASS, $options);
        } catch (PDOException $e) {
            error_log("Database connection failed: " . $e->getMessage());
            http_response_code(500);
            echo json_encode(['error' => 'Database connection failed']);
            exit;
        }
    }
    
    return $pdo;
}

// Verify API key
function verifyApiKey($apiKey) {
    if (!API_KEY_REQUIRED) {
        return true;
    }
    
    return isset(API_KEYS[$apiKey]);
}

// Get device name from API key
function getDeviceNameFromApiKey($apiKey) {
    return API_KEYS[$apiKey] ?? 'Unknown Device';
}
