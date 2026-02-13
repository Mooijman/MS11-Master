<?php
/**
 * Clean old telemetry data
 * 
 * This script removes telemetry data older than the configured retention period
 * Run this script via cron daily: 0 2 * * * php cleanup_old_data.php
 */

require_once __DIR__ . '/../config/database.php';

echo "========================================\n";
echo "MS11 Telemetry - Data Cleanup\n";
echo "========================================\n\n";

$retentionDays = DATA_RETENTION_DAYS;
$dryRun = isset($argv[1]) && $argv[1] === '--dry-run';

echo "Retention period: $retentionDays days\n";
echo "Mode: " . ($dryRun ? "DRY RUN (no changes)" : "LIVE") . "\n\n";

try {
    $pdo = getDbConnection();
    
    // Calculate cutoff date
    $cutoffDate = date('Y-m-d H:i:s', strtotime("-$retentionDays days"));
    echo "Removing data older than: $cutoffDate\n\n";
    
    // Count records to be deleted
    $stmt = $pdo->prepare("
        SELECT COUNT(*) as count FROM telemetry_data 
        WHERE timestamp < :cutoff_date
    ");
    $stmt->execute(['cutoff_date' => $cutoffDate]);
    $telemetryCount = $stmt->fetch()['count'];
    
    $stmt = $pdo->prepare("
        SELECT COUNT(*) as count FROM events 
        WHERE timestamp < :cutoff_date
    ");
    $stmt->execute(['cutoff_date' => $cutoffDate]);
    $eventsCount = $stmt->fetch()['count'];
    
    $stmt = $pdo->prepare("
        SELECT COUNT(*) as count FROM statistics_hourly 
        WHERE hour_timestamp < :cutoff_date
    ");
    $stmt->execute(['cutoff_date' => $cutoffDate]);
    $statsCount = $stmt->fetch()['count'];
    
    echo "Records to be deleted:\n";
    echo "- Telemetry data: $telemetryCount\n";
    echo "- Events: $eventsCount\n";
    echo "- Statistics: $statsCount\n";
    echo "- Total: " . ($telemetryCount + $eventsCount + $statsCount) . "\n\n";
    
    if ($dryRun) {
        echo "DRY RUN - No records deleted\n";
    } else {
        if ($telemetryCount + $eventsCount + $statsCount === 0) {
            echo "No records to delete\n";
        } else {
            echo "Deleting records...\n";
            
            // Delete old telemetry data
            if ($telemetryCount > 0) {
                echo "Deleting telemetry data... ";
                $stmt = $pdo->prepare("DELETE FROM telemetry_data WHERE timestamp < :cutoff_date");
                $stmt->execute(['cutoff_date' => $cutoffDate]);
                echo "Done\n";
            }
            
            // Delete old events
            if ($eventsCount > 0) {
                echo "Deleting events... ";
                $stmt = $pdo->prepare("DELETE FROM events WHERE timestamp < :cutoff_date");
                $stmt->execute(['cutoff_date' => $cutoffDate]);
                echo "Done\n";
            }
            
            // Delete old statistics
            if ($statsCount > 0) {
                echo "Deleting statistics... ";
                $stmt = $pdo->prepare("DELETE FROM statistics_hourly WHERE hour_timestamp < :cutoff_date");
                $stmt->execute(['cutoff_date' => $cutoffDate]);
                echo "Done\n";
            }
            
            echo "\nCleanup complete!\n";
        }
    }
    
    // Show current database size
    echo "\nCurrent database size:\n";
    $stmt = $pdo->query("
        SELECT 
            table_name AS 'Table',
            ROUND(((data_length + index_length) / 1024 / 1024), 2) AS 'Size_MB',
            table_rows AS 'Rows'
        FROM information_schema.TABLES
        WHERE table_schema = '" . DB_NAME . "'
        ORDER BY (data_length + index_length) DESC
    ");
    
    $tables = $stmt->fetchAll();
    foreach ($tables as $table) {
        echo "- {$table['Table']}: {$table['Size_MB']} MB ({$table['Rows']} rows)\n";
    }
    
    echo "\n========================================\n";
    echo "Cleanup script complete\n";
    echo "========================================\n";
    
} catch (PDOException $e) {
    echo "ERROR: " . $e->getMessage() . "\n";
    exit(1);
}
