# v2026.1.1.19 - Display Updates During Software Updates

## 🐛 Fixed Issues

### Software Update Display
- **OLED Display**: Now shows update status during firmware and LittleFS downloads
  - "Updating FW" / "Updating FS" at the start
  - "Please Wait... DO NOT POWER OFF" during download
  - "Update done / Rebooting..." after successful completion
  
- **LCD Display**: Now shows "Updating..." on line 0 during any software update
  - Provides visual feedback to users during update process
  - Non-blocking, works if LCD is initialized

### Root Cause
All `DisplayManager` and `LCDManager` display update calls were commented out in `downloadAndInstallFirmware()` and `downloadAndInstallLittleFS()` functions, leaving the displays blank during updates.

## 📝 Files Changed
- `include/config.h`: Version bump (2026.1.1.19)
- `include/github_updater.h`: Added LCDManager include
- `src/github_updater.cpp`: Uncommented display code and made API-correct method calls
- `CHANGELOG.md`: Documented changes

## 📦 Build Info
- **Firmware**: fw-2026.1.1.19.bin
- **Filesystem**: fs-2026.1.1.19.bin
- **Platform**: ESP32-S3 (Seeed XIAO)
- **Build Date**: 2026-02-10

## 🚀 Installation
Flash both binaries using:
```bash
./uploadfs_with_backup.sh <ESP_IP>
```
Or manually upload firmware + filesystem via PlatformIO.

---

**Released**: 2026-02-10
