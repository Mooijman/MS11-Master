<?php
/**
 * Test script for MS11 Telemetry API
 * 
 * Usage: php test_api.php
 * This script tests the API endpoints without requiring an actual device
 */

// Configuration
$API_BASE_URL = 'http://localhost/server/api';  // Adjust for your environment
$API_KEY = 'example_key_device_1';              // From database.php

// Test data
$testDeviceId = 'MS11-TEST-001';
$testDeviceName = 'Test Device';

echo "========================================\n";
echo "MS11 Telemetry API Test Script\n";
echo "========================================\n\n";

// Test 1: Send telemetry data
echo "Test 1: Sending telemetry data...\n";
$telemetryData = [
    'device_id' => $testDeviceId,
    'device_name' => $testDeviceName,
    'firmware_version' => '2026.2.12.02',
    'filesystem_version' => '2026.2.12.02',
    'ip_address' => '192.168.1.100',
    'mac_address' => 'AA:BB:CC:DD:EE:FF',
    'temperature' => 23.5,
    'humidity' => 45.2,
    'uptime_seconds' => 86400,
    'free_heap' => 245760,
    'wifi_rssi' => -65,
    'ms11_connected' => true,
];

$ch = curl_init($API_BASE_URL . '/ingest.php');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_POST, true);
curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($telemetryData));
curl_setopt($ch, CURLOPT_HTTPHEADER, [
    'Content-Type: application/json',
    'X-API-Key: ' . $API_KEY
]);

$response = curl_exec($ch);
$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

echo "Response Code: $httpCode\n";
echo "Response: $response\n\n";

if ($httpCode == 201) {
    echo "✓ Test 1 PASSED\n\n";
} else {
    echo "✗ Test 1 FAILED\n\n";
}

// Test 2: Send event
echo "Test 2: Sending event...\n";
$eventData = [
    'device_id' => $testDeviceId,
    'device_name' => $testDeviceName,
    'event' => [
        'type' => 'info',
        'category' => 'test',
        'message' => 'This is a test event from the test script'
    ]
];

$ch = curl_init($API_BASE_URL . '/ingest.php');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_POST, true);
curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($eventData));
curl_setopt($ch, CURLOPT_HTTPHEADER, [
    'Content-Type: application/json',
    'X-API-Key: ' . $API_KEY
]);

$response = curl_exec($ch);
$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

echo "Response Code: $httpCode\n";
echo "Response: $response\n\n";

if ($httpCode == 201) {
    echo "✓ Test 2 PASSED\n\n";
} else {
    echo "✗ Test 2 FAILED\n\n";
}

// Test 3: List devices
echo "Test 3: Listing devices...\n";
$ch = curl_init($API_BASE_URL . '/query.php?action=list_devices');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

$response = curl_exec($ch);
$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

echo "Response Code: $httpCode\n";
echo "Response: $response\n\n";

if ($httpCode == 200) {
    $data = json_decode($response, true);
    if (isset($data['devices'])) {
        echo "✓ Test 3 PASSED - Found " . count($data['devices']) . " device(s)\n\n";
    } else {
        echo "✗ Test 3 FAILED - Invalid response format\n\n";
    }
} else {
    echo "✗ Test 3 FAILED\n\n";
}

// Test 4: Get latest telemetry
echo "Test 4: Getting latest telemetry...\n";
$ch = curl_init($API_BASE_URL . '/query.php?action=latest_data');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

$response = curl_exec($ch);
$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

echo "Response Code: $httpCode\n";
echo "Response: $response\n\n";

if ($httpCode == 200) {
    $data = json_decode($response, true);
    if (isset($data['data'])) {
        echo "✓ Test 4 PASSED - Found " . count($data['data']) . " data point(s)\n\n";
    } else {
        echo "✗ Test 4 FAILED - Invalid response format\n\n";
    }
} else {
    echo "✗ Test 4 FAILED\n\n";
}

// Test 5: Get events
echo "Test 5: Getting events...\n";
$ch = curl_init($API_BASE_URL . '/query.php?action=events&limit=10');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

$response = curl_exec($ch);
$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

echo "Response Code: $httpCode\n";
echo "Response: $response\n\n";

if ($httpCode == 200) {
    $data = json_decode($response, true);
    if (isset($data['events'])) {
        echo "✓ Test 5 PASSED - Found " . count($data['events']) . " event(s)\n\n";
    } else {
        echo "✗ Test 5 FAILED - Invalid response format\n\n";
    }
} else {
    echo "✗ Test 5 FAILED\n\n";
}

// Test 6: Test invalid API key
echo "Test 6: Testing API security (invalid key)...\n";
$ch = curl_init($API_BASE_URL . '/ingest.php');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_POST, true);
curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode(['device_id' => 'test']));
curl_setopt($ch, CURLOPT_HTTPHEADER, [
    'Content-Type: application/json',
    'X-API-Key: invalid_key_12345'
]);

$response = curl_exec($ch);
$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

echo "Response Code: $httpCode\n";
echo "Response: $response\n\n";

if ($httpCode == 401) {
    echo "✓ Test 6 PASSED - Unauthorized access properly rejected\n\n";
} else {
    echo "✗ Test 6 FAILED - Security issue: invalid key was accepted\n\n";
}

echo "========================================\n";
echo "Test Complete\n";
echo "========================================\n";
