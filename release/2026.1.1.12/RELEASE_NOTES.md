# Release v2026.1.1.12 - I2C Standard Pins & Dual-Bus Scanner

**Release Date**: February 9, 2026  
**Build**: ESP32-S3 | RAM: 15.9% (52.1KB) | Flash: 69.2% (1.36MB)

---

## ⭐ Key Changes

### I2C Bus Migration to Standard Pins
Bus 0 (Slave controller) migrated to **standard I2C pins** for better hardware support:
- **Bus 0**: GPIO5 (SDA) + GPIO6 (SCL) @ 100kHz - Slave controller (0x30)
- **Bus 1**: GPIO8 (SDA) + GPIO9 (SCL) @ 100kHz - Display devices
- Both buses now use conservative 100kHz speed for maximum reliability

**Benefits**:
- ✅ Native Arduino Wire library support
- ✅ Better ESP32-S3 peripheral routing
- ✅ Easier debugging and maintenance
- ✅ GPIO7 freed up for future use

### Enhanced I2C Diagnostics
The I2C scanner page (`/i2c.html`) now shows both buses separately:
- Visual separation of Bus 0 (Slave) and Bus 1 (Display) devices
- Per-bus information: pins, speed, device count
- Device badges showing which bus each device is on
- Register dumps correctly target the appropriate bus

---

## 🔧 Technical Details

### Files Changed
- `i2c_manager.cpp`: Both buses initialized @ 100kHz
- `config.h`: Version bump + bus configuration
- `main.cpp`: Dual-bus scan API endpoints
- `data/i2c.html`: Enhanced UI with bus separation
- All documentation updated with new pinout

### Hardware Configuration
```
Bus 0 (Slave - Critical):
  GPIO5 (D4) = SDA  ⭐ Standard I2C
  GPIO6 (D5) = SCL  ⭐ Standard I2C
  Speed: 100kHz
  Device: ATmega328P @ 0x30

Bus 1 (Display - Non-critical):
  GPIO8 (D9) = SDA
  GPIO9 (D10) = SCL
  Speed: 100kHz
  Devices: OLED (0x3C), LCD (0x27), Seesaw (0x36)
```

---

## 📦 Installation

### Via Web Interface (Recommended)
1. Navigate to `Settings > Update`
2. Click "Check for Updates"
3. System will detect v2026.1.1.12
4. Click "Update Firmware" and "Update Filesystem"
5. Device reboots automatically

### Manual Upload
```bash
# Firmware only
curl -F "update=@fw-2026.1.1.12.bin" http://<ESP_IP>/update

# Filesystem only
curl -F "update=@fs-2026.1.1.12.bin" http://<ESP_IP>/update
```

### Serial Flash (First Install)
```bash
pio run -e esp32s3dev -t upload
pio run -e esp32s3dev -t uploadfs
```

---

## ⚠️ Upgrade Notes

### Wiring Changes Required
If upgrading from v2026.1.1.11 or earlier:
- **Move Slave SDA**: GPIO6 → GPIO5 (D4)
- **Keep Slave SCL**: GPIO6 (D5) - unchanged
- Display bus (GPIO8/9) remains the same
- Pull-up resistors: 4.7kΩ to 3.3V on all I2C pins

### No Code Changes Needed
- Application logic unchanged
- Settings preserved across update
- NVS configuration compatible

---

## 🧪 Tested Configurations
- ✅ XIAO ESP32-S3 (8MB flash)
- ✅ ATmega328P slave @ 0x30 on Bus 0
- ✅ SSD1306 OLED @ 0x3C on Bus 1
- ✅ PCF8574 LCD @ 0x27 on Bus 1
- ✅ Seesaw Rotary @ 0x36 on Bus 1
- ✅ OTA updates (firmware + filesystem)
- ✅ I2C scanner diagnostics

---

## 📝 Known Issues
None reported for this release.

---

## 🔗 Links
- [Full Changelog](../../CHANGELOG.md)
- [I2C Wiring Guide](../../I2C_WIRING_GUIDE.md)
- [Hardware Documentation](../../XIAO_S3_HARDWARE_WIRING.md)
