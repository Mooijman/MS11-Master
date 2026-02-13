<?php
/**
 * MS11 Telemetry Logging Server - Query API
 * 
 * Endpoint: GET /api/query.php
 * Retrieves telemetry data from the database
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

// Handle preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit;
}

// Only allow GET requests
if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    http_response_code(405);
    echo json_encode(['error' => 'Method not allowed']);
    exit;
}

// Load configuration
require_once __DIR__ . '/../config/database.php';

// Parse query parameters
$action = $_GET['action'] ?? 'list_devices';
$deviceId = $_GET['device_id'] ?? null;
$limit = min((int)($_GET['limit'] ?? 100), 1000); // Max 1000 records
$offset = (int)($_GET['offset'] ?? 0);
$startTime = $_GET['start'] ?? null;
$endTime = $_GET['end'] ?? null;

try {
    $pdo = getDbConnection();
    
    switch ($action) {
        case 'list_devices':
            // List all registered devices
            $stmt = $pdo->query("
                SELECT 
                    device_id, device_name, firmware_version, filesystem_version,
                    ip_address, first_seen, last_seen, is_active
                FROM devices
                ORDER BY last_seen DESC
            ");
            $devices = $stmt->fetchAll();
            
            echo json_encode([
                'success' => true,
                'devices' => $devices,
                'count' => count($devices),
            ]);
            break;
            
        case 'latest_data':
            // Get latest telemetry data for all devices or a specific device
            $sql = "
                SELECT 
                    t.device_id, d.device_name, t.timestamp, t.temperature, t.humidity,
                    t.uptime_seconds, t.free_heap, t.wifi_rssi, t.ms11_connected
                FROM telemetry_data t
                JOIN devices d ON t.device_id = d.device_id
                WHERE t.id IN (
                    SELECT MAX(id) FROM telemetry_data
            ";
            
            if ($deviceId) {
                $sql .= " WHERE device_id = :device_id";
            }
            
            $sql .= " GROUP BY device_id)";
            $sql .= " ORDER BY t.timestamp DESC";
            
            $stmt = $pdo->prepare($sql);
            if ($deviceId) {
                $stmt->execute(['device_id' => $deviceId]);
            } else {
                $stmt->execute();
            }
            
            $data = $stmt->fetchAll();
            
            echo json_encode([
                'success' => true,
                'data' => $data,
                'count' => count($data),
            ]);
            break;
            
        case 'telemetry':
            // Get telemetry data with time range filtering
            if (!$deviceId) {
                http_response_code(400);
                echo json_encode(['error' => 'device_id is required for telemetry action']);
                exit;
            }
            
            $sql = "
                SELECT 
                    timestamp, temperature, humidity, uptime_seconds, 
                    free_heap, wifi_rssi, ms11_connected, extra_data
                FROM telemetry_data
                WHERE device_id = :device_id
            ";
            
            $params = ['device_id' => $deviceId];
            
            if ($startTime) {
                $sql .= " AND timestamp >= :start_time";
                $params['start_time'] = date('Y-m-d H:i:s', strtotime($startTime));
            }
            
            if ($endTime) {
                $sql .= " AND timestamp <= :end_time";
                $params['end_time'] = date('Y-m-d H:i:s', strtotime($endTime));
            }
            
            $sql .= " ORDER BY timestamp DESC LIMIT :limit OFFSET :offset";
            
            $stmt = $pdo->prepare($sql);
            
            // Bind parameters
            foreach ($params as $key => $value) {
                $stmt->bindValue(':' . $key, $value);
            }
            $stmt->bindValue(':limit', $limit, PDO::PARAM_INT);
            $stmt->bindValue(':offset', $offset, PDO::PARAM_INT);
            
            $stmt->execute();
            $data = $stmt->fetchAll();
            
            echo json_encode([
                'success' => true,
                'device_id' => $deviceId,
                'data' => $data,
                'count' => count($data),
                'limit' => $limit,
                'offset' => $offset,
            ]);
            break;
            
        case 'events':
            // Get events for a device or all devices
            $sql = "
                SELECT 
                    e.device_id, d.device_name, e.timestamp, e.event_type,
                    e.event_category, e.message, e.details
                FROM events e
                JOIN devices d ON e.device_id = d.device_id
            ";
            
            $params = [];
            
            if ($deviceId) {
                $sql .= " WHERE e.device_id = :device_id";
                $params['device_id'] = $deviceId;
            }
            
            $sql .= " ORDER BY e.timestamp DESC LIMIT :limit OFFSET :offset";
            
            $stmt = $pdo->prepare($sql);
            
            foreach ($params as $key => $value) {
                $stmt->bindValue(':' . $key, $value);
            }
            $stmt->bindValue(':limit', $limit, PDO::PARAM_INT);
            $stmt->bindValue(':offset', $offset, PDO::PARAM_INT);
            
            $stmt->execute();
            $events = $stmt->fetchAll();
            
            echo json_encode([
                'success' => true,
                'events' => $events,
                'count' => count($events),
            ]);
            break;
            
        case 'statistics':
            // Get aggregated statistics
            if (!$deviceId) {
                http_response_code(400);
                echo json_encode(['error' => 'device_id is required for statistics action']);
                exit;
            }
            
            $sql = "
                SELECT 
                    hour_timestamp, temp_avg, temp_min, temp_max,
                    humidity_avg, humidity_min, humidity_max,
                    uptime_avg, free_heap_avg, wifi_rssi_avg, sample_count
                FROM statistics_hourly
                WHERE device_id = :device_id
            ";
            
            $params = ['device_id' => $deviceId];
            
            if ($startTime) {
                $sql .= " AND hour_timestamp >= :start_time";
                $params['start_time'] = date('Y-m-d H:00:00', strtotime($startTime));
            }
            
            if ($endTime) {
                $sql .= " AND hour_timestamp <= :end_time";
                $params['end_time'] = date('Y-m-d H:00:00', strtotime($endTime));
            }
            
            $sql .= " ORDER BY hour_timestamp DESC LIMIT :limit";
            
            $stmt = $pdo->prepare($sql);
            
            foreach ($params as $key => $value) {
                $stmt->bindValue(':' . $key, $value);
            }
            $stmt->bindValue(':limit', $limit, PDO::PARAM_INT);
            
            $stmt->execute();
            $stats = $stmt->fetchAll();
            
            echo json_encode([
                'success' => true,
                'device_id' => $deviceId,
                'statistics' => $stats,
                'count' => count($stats),
            ]);
            break;
            
        default:
            http_response_code(400);
            echo json_encode(['error' => 'Invalid action specified']);
    }
    
} catch (PDOException $e) {
    error_log("Database error: " . $e->getMessage());
    http_response_code(500);
    echo json_encode(['error' => 'Database error occurred']);
}
