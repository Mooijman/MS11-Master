<?php
/**
 * MS11 Telemetry Logging Server - Data Ingestion API
 * 
 * Endpoint: POST /api/ingest.php
 * Receives telemetry data from MS11 devices
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, X-API-Key');

// Handle preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit;
}

// Only allow POST requests
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    echo json_encode(['error' => 'Method not allowed']);
    exit;
}

// Load configuration
require_once __DIR__ . '/../config/database.php';

// Verify API key
$apiKey = $_SERVER['HTTP_X_API_KEY'] ?? '';
if (!verifyApiKey($apiKey)) {
    http_response_code(401);
    echo json_encode(['error' => 'Unauthorized - Invalid API key']);
    exit;
}

// Parse JSON payload
$input = file_get_contents('php://input');
$data = json_decode($input, true);

if (json_last_error() !== JSON_ERROR_NONE) {
    http_response_code(400);
    echo json_encode(['error' => 'Invalid JSON']);
    exit;
}

// Validate required fields
if (!isset($data['device_id'])) {
    http_response_code(400);
    echo json_encode(['error' => 'Missing required field: device_id']);
    exit;
}

try {
    $pdo = getDbConnection();
    $pdo->beginTransaction();
    
    // Update or insert device record
    $stmt = $pdo->prepare("
        INSERT INTO devices (device_id, device_name, firmware_version, filesystem_version, ip_address, mac_address, last_seen)
        VALUES (:device_id, :device_name, :firmware_version, :filesystem_version, :ip_address, :mac_address, NOW())
        ON DUPLICATE KEY UPDATE
            device_name = COALESCE(:device_name, device_name),
            firmware_version = COALESCE(:firmware_version, firmware_version),
            filesystem_version = COALESCE(:filesystem_version, filesystem_version),
            ip_address = COALESCE(:ip_address, ip_address),
            mac_address = COALESCE(:mac_address, mac_address),
            last_seen = NOW()
    ");
    
    $stmt->execute([
        'device_id' => $data['device_id'],
        'device_name' => $data['device_name'] ?? getDeviceNameFromApiKey($apiKey),
        'firmware_version' => $data['firmware_version'] ?? null,
        'filesystem_version' => $data['filesystem_version'] ?? null,
        'ip_address' => $data['ip_address'] ?? $_SERVER['REMOTE_ADDR'],
        'mac_address' => $data['mac_address'] ?? null,
    ]);
    
    // Insert telemetry data
    $stmt = $pdo->prepare("
        INSERT INTO telemetry_data (
            device_id, timestamp, temperature, humidity, uptime_seconds, 
            free_heap, wifi_rssi, ms11_connected, extra_data
        ) VALUES (
            :device_id, :timestamp, :temperature, :humidity, :uptime_seconds,
            :free_heap, :wifi_rssi, :ms11_connected, :extra_data
        )
    ");
    
    // Parse timestamp or use current time
    $timestamp = isset($data['timestamp']) ? date('Y-m-d H:i:s', strtotime($data['timestamp'])) : null;
    
    // Prepare extra_data JSON for any additional fields
    $extraData = [];
    $knownFields = ['device_id', 'device_name', 'firmware_version', 'filesystem_version', 
                    'ip_address', 'mac_address', 'timestamp', 'temperature', 'humidity', 
                    'uptime_seconds', 'free_heap', 'wifi_rssi', 'ms11_connected'];
    
    foreach ($data as $key => $value) {
        if (!in_array($key, $knownFields)) {
            $extraData[$key] = $value;
        }
    }
    
    $stmt->execute([
        'device_id' => $data['device_id'],
        'timestamp' => $timestamp,
        'temperature' => $data['temperature'] ?? null,
        'humidity' => $data['humidity'] ?? null,
        'uptime_seconds' => $data['uptime_seconds'] ?? null,
        'free_heap' => $data['free_heap'] ?? null,
        'wifi_rssi' => $data['wifi_rssi'] ?? null,
        'ms11_connected' => isset($data['ms11_connected']) ? (bool)$data['ms11_connected'] : null,
        'extra_data' => !empty($extraData) ? json_encode($extraData) : null,
    ]);
    
    // Log event if provided
    if (isset($data['event'])) {
        $stmt = $pdo->prepare("
            INSERT INTO events (device_id, event_type, event_category, message, details)
            VALUES (:device_id, :event_type, :event_category, :message, :details)
        ");
        
        $stmt->execute([
            'device_id' => $data['device_id'],
            'event_type' => $data['event']['type'] ?? 'info',
            'event_category' => $data['event']['category'] ?? 'general',
            'message' => $data['event']['message'] ?? '',
            'details' => isset($data['event']['details']) ? json_encode($data['event']['details']) : null,
        ]);
    }
    
    $pdo->commit();
    
    http_response_code(201);
    echo json_encode([
        'success' => true,
        'message' => 'Data received successfully',
        'device_id' => $data['device_id'],
    ]);
    
} catch (PDOException $e) {
    if ($pdo->inTransaction()) {
        $pdo->rollBack();
    }
    error_log("Database error: " . $e->getMessage());
    http_response_code(500);
    echo json_encode(['error' => 'Database error occurred']);
}
