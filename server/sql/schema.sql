-- MS11 Telemetry Logging Server - Database Schema
-- Version: 1.0
-- Created: 2026-02-13

-- Create database (if not exists)
CREATE DATABASE IF NOT EXISTS ms11_telemetry CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE ms11_telemetry;

-- Devices table - stores information about each MS11 device
CREATE TABLE IF NOT EXISTS devices (
    device_id VARCHAR(64) PRIMARY KEY,
    device_name VARCHAR(255) NOT NULL,
    firmware_version VARCHAR(32),
    filesystem_version VARCHAR(32),
    ip_address VARCHAR(45),
    mac_address VARCHAR(17),
    first_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE,
    INDEX idx_last_seen (last_seen),
    INDEX idx_is_active (is_active)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Telemetry data table - stores sensor readings and system metrics
CREATE TABLE IF NOT EXISTS telemetry_data (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    device_id VARCHAR(64) NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    -- Environmental sensors
    temperature DECIMAL(5,2) NULL COMMENT 'Temperature in Celsius',
    humidity DECIMAL(5,2) NULL COMMENT 'Humidity in percentage',
    
    -- System metrics
    uptime_seconds INT UNSIGNED NULL COMMENT 'Device uptime in seconds',
    free_heap INT UNSIGNED NULL COMMENT 'Free heap memory in bytes',
    wifi_rssi TINYINT NULL COMMENT 'WiFi signal strength in dBm',
    
    -- MS11 control status
    ms11_connected BOOLEAN NULL COMMENT 'MS11 control connection status',
    
    -- Additional data (JSON for extensibility)
    extra_data JSON NULL COMMENT 'Additional sensor data or custom fields',
    
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE,
    INDEX idx_device_timestamp (device_id, timestamp),
    INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Events table - stores important events and alerts
CREATE TABLE IF NOT EXISTS events (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    device_id VARCHAR(64) NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    event_type ENUM('info', 'warning', 'error', 'critical') NOT NULL DEFAULT 'info',
    event_category VARCHAR(64) NOT NULL COMMENT 'Category: boot, network, sensor, update, etc.',
    message TEXT NOT NULL,
    details JSON NULL,
    
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE,
    INDEX idx_device_timestamp (device_id, timestamp),
    INDEX idx_event_type (event_type),
    INDEX idx_event_category (event_category)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Aggregated statistics table (for performance optimization)
CREATE TABLE IF NOT EXISTS statistics_hourly (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    device_id VARCHAR(64) NOT NULL,
    hour_timestamp TIMESTAMP NOT NULL,
    
    -- Temperature statistics
    temp_avg DECIMAL(5,2) NULL,
    temp_min DECIMAL(5,2) NULL,
    temp_max DECIMAL(5,2) NULL,
    
    -- Humidity statistics
    humidity_avg DECIMAL(5,2) NULL,
    humidity_min DECIMAL(5,2) NULL,
    humidity_max DECIMAL(5,2) NULL,
    
    -- System statistics
    uptime_avg INT UNSIGNED NULL,
    free_heap_avg INT UNSIGNED NULL,
    wifi_rssi_avg TINYINT NULL,
    
    -- Sample count
    sample_count INT UNSIGNED NOT NULL DEFAULT 0,
    
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE,
    UNIQUE KEY unique_device_hour (device_id, hour_timestamp),
    INDEX idx_hour_timestamp (hour_timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Create a user for the application (run manually with appropriate password)
-- CREATE USER 'ms11_user'@'localhost' IDENTIFIED BY 'your_secure_password';
-- GRANT SELECT, INSERT, UPDATE, DELETE ON ms11_telemetry.* TO 'ms11_user'@'localhost';
-- FLUSH PRIVILEGES;
