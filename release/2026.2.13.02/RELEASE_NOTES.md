# Release v2026.2.13.02 - Startup Improvements & Heartbeat Pulse Restored

## ✨ New Features

### 2ms Heartbeat LED Pulse
- Visual feedback is back! Every 2 seconds, MS11-control LED briefly pulses (2ms) to confirm heartbeat
- Safe implementation using I2CManager mutex protection (no conflicts with bootloader operations)
- Provides immediate visual indication that the slave controller is alive and responding

## 🎯 Improvements

### Optimized Startup Sequence
**MS11-control Detection Moved Earlier:**
- Detection now happens immediately after I2C/SlaveController initialization
- Occurs **before** WiFi setup and **before** Waacs logo display
- Provides faster feedback about slave controller availability
- 500ms LED pulse triggered on successful detection during startup

### Cleaner Display Flow
**OLED Display:**
- MS11-control status now shown on OLED (not LCD) together with "MS11 Master" text
- Clear 4-line layout:
  ```
  MS11 Master
  
  MS11-control
  Detected
  ```
  or `Absent` if not detected
- Displayed for 2 seconds during startup sequence

**LCD Display:**
- Simplified startup flow: `"Starting up..."` → `"WiFi Enabled"` + IP → `"Ready."` + clock
- No longer shows MS11-control detection message on LCD
- Cleaner transition without timing conflicts

## 📋 Startup Sequence Summary

**New initialization order:**
1. I2C Manager + GPIO + Display/LCD/Seesaw + Slave Controller init
2. **MS11-control detection** (ping + 500ms LED pulse if present) ← Moved earlier
3. Waacs logo on OLED (3s)
4. **MS11 Master + MS11-control status on OLED** (2s) ← New combined view
5. Settings + WiFi initialization
6. WiFi status + IP on LCD (3s)
7. Normal operation: LCD shows "Ready." + clock, OLED clear

## 🔄 Upgrade Notes

**From any previous version:**
- Flash this firmware via OTA or USB
- No LittleFS changes required (but included for completeness)
- No settings migration needed
- Heartbeat pulse will be visible immediately after upgrade

**Visual Changes:**
- MS11-control status no longer appears on LCD during startup
- Check OLED screen after Waacs logo for MS11-control detection status
- Heartbeat pulse visible every 2 seconds on MS11-control LED (if connected)

## 📦 Files in This Release

- `fw-2026.2.13.02.bin` - Main firmware (1.3 MB)
- `fs-2026.2.13.02.bin` - LittleFS filesystem (1.9 MB)

## 🐛 Known Issues

None. All features working as expected.

## 🔗 Links

- [Full Change History](../../CHANGELOG.md)
- [Twiboot Integration Guide](../../TWIBOOT_INTEGRATION.md)
- [Project Documentation](../../README.md)
- [Previous Release (v2026.2.13.01)](../2026.2.13.01/RELEASE_NOTES.md)
