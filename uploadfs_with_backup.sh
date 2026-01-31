#!/bin/bash

# Script to upload filesystem while preserving configuration files
# Usage: ./uploadfs_with_backup.sh [ESP32_IP]

ESP32_IP="${1:-172.17.1.159}"
BACKUP_DIR="./.fs_backup"

echo "=== LittleFS Upload with Config Backup ==="
echo "ESP32 IP: $ESP32_IP"

# Create backup directory
mkdir -p "$BACKUP_DIR"

# Backup configuration files
echo ""
echo "Step 1/3: Backing up configuration files..."
curl -s "http://$ESP32_IP/api/file?path=/global.conf" -o "$BACKUP_DIR/global.conf" 2>/dev/null
if [ -f "$BACKUP_DIR/global.conf" ]; then
  echo "  ✓ global.conf backed up"
else
  echo "  ⚠ global.conf not found (this is OK for first upload)"
fi

curl -s "http://$ESP32_IP/api/file?path=/pass.txt" -o "$BACKUP_DIR/pass.txt" 2>/dev/null
if [ -f "$BACKUP_DIR/pass.txt" ]; then
  echo "  ✓ pass.txt backed up"
else
  echo "  ⚠ pass.txt not found (this is OK for first upload)"
fi

# Upload filesystem
echo ""
echo "Step 2/3: Uploading filesystem..."
pio run --target uploadfs

if [ $? -ne 0 ]; then
  echo "❌ Filesystem upload failed!"
  exit 1
fi

# Wait for ESP32 to restart
echo ""
echo "Waiting for ESP32 to restart..."
sleep 5

# Restore configuration files
echo ""
echo "Step 3/3: Restoring configuration files..."

if [ -f "$BACKUP_DIR/global.conf" ]; then
  curl -s -X POST -F "file=@$BACKUP_DIR/global.conf" "http://$ESP32_IP/api/upload" >/dev/null 2>&1
  if [ $? -eq 0 ]; then
    echo "  ✓ global.conf restored"
  else
    echo "  ❌ Failed to restore global.conf"
  fi
fi

if [ -f "$BACKUP_DIR/pass.txt" ]; then
  curl -s -X POST -F "file=@$BACKUP_DIR/pass.txt" "http://$ESP32_IP/api/upload" >/dev/null 2>&1
  if [ $? -eq 0 ]; then
    echo "  ✓ pass.txt restored"
  else
    echo "  ❌ Failed to restore pass.txt"
  fi
fi

echo ""
echo "=== Done! ==="
echo "Configuration files preserved, ESP32 should reconnect automatically."
echo ""
