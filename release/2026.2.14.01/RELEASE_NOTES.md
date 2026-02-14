# Release v2026.2.14.01 - Automatic MS11-control Firmware Update

## ✨ New Features

### Automatic MS11-control Firmware Update via Web Interface
A major workflow improvement that makes updating MS11-control firmware as simple as uploading a file!

**How it works:**
1. Upload a `.hex` file to the ESP32 filesystem via the web interface (Files page)
2. Reboot the MS11 Master
3. During startup, the system automatically:
   - Detects the `.hex` file in LittleFS root directory
   - Enters MS11-control bootloader mode (I2C command 0x99/0xB0 to address 0x30)
   - Uploads the firmware via I2C using Intel HEX format
   - Exits bootloader (I2C command 0x01/0x80 to address 0x14)
   - Deletes the `.hex` file
   - Restarts the ESP32 automatically

**Features:**
- **LCD Feedback**: Real-time status display during update
  - "MS11-Control" + "Updating..." (during upload)
  - "Success!" (after completion)
  - Error messages if update fails
- **Safe & Automatic**: No manual I2C commands or bootloader interaction needed
- **Self-Cleaning**: `.hex` file is automatically deleted after successful upload
- **Fallback Protection**: If delete fails, file is renamed to `.hex.done` to prevent re-upload

**Technical Implementation:**
- New `checkAndUpdateMS11Firmware()` function in [src/main.cpp](../../src/main.cpp)
- Scans root directory for `.hex` files using LittleFS
- Uses existing `MD11SlaveUpdate` class for I2C bootloader protocol
- Proper bootloader entry/exit sequence with 6s startup delay
- Full integration with I2CManager for mutex-protected I2C operations

## 🎯 Improvements

### Updated MS11-control Firmware
- **New**: `firmware.hex` - Latest MS11-control firmware (377 lines)
- **Removed**: Old `sl-2026.2.12.03.hex` (180 lines)
- Firmware included in filesystem for reference and automatic updates

## 🔄 Upgrade Notes

**From v2026.2.13.02 or earlier:**
- Upload both `fw-2026.2.14.01.bin` (firmware) and `fs-2026.2.14.01.bin` (filesystem) via GitHub OTA update page
- After reboot, you can now upload `.hex` files for MS11-control updates via Files page

**To update MS11-control firmware:**
1. Navigate to Files page in web interface
2. Upload your `.hex` file (Intel HEX format)
3. Reboot MS11 Master (Settings page or power cycle)
4. Watch LCD for update progress
5. System will automatically restart after successful update

## 📦 Assets
- `fw-2026.2.14.01.bin` - Main firmware (ESP32-S3)
- `fs-2026.2.14.01.bin` - LittleFS filesystem with web interface + new firmware.hex

## 🏷️ Version Info
- Firmware: fw-2026.2.14.01
- Filesystem: fs-2026.2.14.01
- Build Date: Feb 14, 2026
