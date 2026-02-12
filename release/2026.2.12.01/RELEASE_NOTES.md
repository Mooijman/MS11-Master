# Release v2026.2.12.01 - LCD Display States & LED Pulse Communication

## 🚀 New Features

### LCD Display States
Complete startup and operational display sequences for 16x2 LCD:
- **Startup**: "MagicSmoker 11" → "Starting up..." (blinking 600ms/400ms)
- **WiFi Enabled**: Shows IP address for 3 seconds
- **WiFi Manager**: Shows "ESP-WIFI-MANAGER" SSID in AP mode
- **MS11-control Detection**: "Detected" or "Absent" status (2 seconds)
- **Ready State**: "Ready." with NTP time display (DD-MM-YYYY HH:MM)
- **Time Display**: Blinking colon synchronized to system clock

### LED Pulse Communication
Enhanced MS11-control slave signaling via I2C:
- **Detection Pulse**: 500ms LED pulse when MS11-control found at startup
- **Heartbeat Pulse**: 2ms LED pulse every 4 seconds during operation
- Non-blocking pulse state machine for smooth multitasking
- Debug serial output for all LED operations

## 🔧 Improvements

### Non-blocking Architecture
- Removed all `delay()` calls from display updates
- Universal `blinkState()` function for asymmetric blink patterns
- State-based LCD update tracking with timestamps
- OLED sensor display delayed until startup sequence complete

### LED Control Protocol
- Proper I2C register usage (0x10 SLAVE_REG_LED_ONOFF)
- State machine handles pulse timing in main loop
- Reliable communication with ATmega328P slave

## 🐛 Bug Fixes
- Time display colon now properly synchronized using modulo arithmetic
- MS11 detection display correctly shows status with state tracking
- WiFi manager displays correct AP SSID instead of configured SSID
- Startup blink properly stopped when entering WiFi manager mode

## 📦 Installation

**OTA Update:**
Use the built-in GitHub OTA updater in the web interface.

**Serial Upload:**
```bash
# Firmware only
esptool.py write_flash 0x10000 fw-2026.2.12.01.bin

# Filesystem only
esptool.py write_flash 0x310000 fs-2026.2.12.01.bin
```

**PlatformIO:**
```bash
pio run -e esp32s3dev -t upload
pio run -e esp32s3dev -t uploadfs
```

## 🔗 Files
- `fw-2026.2.12.01.bin` - Firmware (1.3MB)
- `fs-2026.2.12.01.bin` - Filesystem (512KB)

## 📝 Full Changelog
See [CHANGELOG.md](../../CHANGELOG.md) for complete version history.
