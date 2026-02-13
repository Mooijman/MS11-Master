<?php
/**
 * Aggregate hourly statistics
 * 
 * This script aggregates telemetry data into hourly statistics for better performance
 * Run this script via cron every hour: 0 * * * * php aggregate_statistics.php
 */

require_once __DIR__ . '/../config/database.php';

echo "========================================\n";
echo "MS11 Telemetry - Hourly Aggregation\n";
echo "========================================\n\n";

try {
    $pdo = getDbConnection();
    
    // Get the last hour that needs aggregation
    $currentHour = date('Y-m-d H:00:00', strtotime('-1 hour'));
    
    echo "Aggregating data for hour: $currentHour\n";
    
    // Get all devices
    $stmt = $pdo->query("SELECT device_id FROM devices WHERE is_active = 1");
    $devices = $stmt->fetchAll(PDO::FETCH_COLUMN);
    
    echo "Found " . count($devices) . " active device(s)\n\n";
    
    $aggregated = 0;
    
    foreach ($devices as $deviceId) {
        echo "Processing device: $deviceId... ";
        
        // Calculate statistics for this device and hour
        $stmt = $pdo->prepare("
            SELECT 
                COUNT(*) as sample_count,
                AVG(temperature) as temp_avg,
                MIN(temperature) as temp_min,
                MAX(temperature) as temp_max,
                AVG(humidity) as humidity_avg,
                MIN(humidity) as humidity_min,
                MAX(humidity) as humidity_max,
                AVG(uptime_seconds) as uptime_avg,
                AVG(free_heap) as free_heap_avg,
                AVG(wifi_rssi) as wifi_rssi_avg
            FROM telemetry_data
            WHERE device_id = :device_id
                AND timestamp >= :hour_start
                AND timestamp < DATE_ADD(:hour_start, INTERVAL 1 HOUR)
                AND temperature IS NOT NULL
        ");
        
        $stmt->execute([
            'device_id' => $deviceId,
            'hour_start' => $currentHour
        ]);
        
        $stats = $stmt->fetch();
        
        if ($stats['sample_count'] > 0) {
            // Insert or update statistics
            $stmt = $pdo->prepare("
                INSERT INTO statistics_hourly (
                    device_id, hour_timestamp, 
                    temp_avg, temp_min, temp_max,
                    humidity_avg, humidity_min, humidity_max,
                    uptime_avg, free_heap_avg, wifi_rssi_avg,
                    sample_count
                ) VALUES (
                    :device_id, :hour_timestamp,
                    :temp_avg, :temp_min, :temp_max,
                    :humidity_avg, :humidity_min, :humidity_max,
                    :uptime_avg, :free_heap_avg, :wifi_rssi_avg,
                    :sample_count
                )
                ON DUPLICATE KEY UPDATE
                    temp_avg = VALUES(temp_avg),
                    temp_min = VALUES(temp_min),
                    temp_max = VALUES(temp_max),
                    humidity_avg = VALUES(humidity_avg),
                    humidity_min = VALUES(humidity_min),
                    humidity_max = VALUES(humidity_max),
                    uptime_avg = VALUES(uptime_avg),
                    free_heap_avg = VALUES(free_heap_avg),
                    wifi_rssi_avg = VALUES(wifi_rssi_avg),
                    sample_count = VALUES(sample_count)
            ");
            
            $stmt->execute([
                'device_id' => $deviceId,
                'hour_timestamp' => $currentHour,
                'temp_avg' => $stats['temp_avg'],
                'temp_min' => $stats['temp_min'],
                'temp_max' => $stats['temp_max'],
                'humidity_avg' => $stats['humidity_avg'],
                'humidity_min' => $stats['humidity_min'],
                'humidity_max' => $stats['humidity_max'],
                'uptime_avg' => $stats['uptime_avg'],
                'free_heap_avg' => $stats['free_heap_avg'],
                'wifi_rssi_avg' => $stats['wifi_rssi_avg'],
                'sample_count' => $stats['sample_count']
            ]);
            
            echo "OK (" . $stats['sample_count'] . " samples)\n";
            $aggregated++;
        } else {
            echo "No data\n";
        }
    }
    
    echo "\n========================================\n";
    echo "Aggregation complete: $aggregated device(s) processed\n";
    echo "========================================\n";
    
} catch (PDOException $e) {
    echo "ERROR: " . $e->getMessage() . "\n";
    exit(1);
}
