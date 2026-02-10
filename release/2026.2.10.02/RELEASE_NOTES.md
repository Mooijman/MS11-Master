# Release v2026.2.10.02 - LCD Clock & Code Optimization

## ✨ Features

### LCD Real-Time Clock
- **Date & Time Display**: LCD line 1 shows current date and time (format: `DD-MM-YYYY HH:MM`)
- **Blinking Colon**: Time separator blinks at 2 Hz for visual feedback
- **Full Year Display**: Shows 4-digit year instead of 2-digit
- **Zero-Padded Format**: Consistent date formatting (e.g., `01-02-2026` not ` 1-02-2026`)
- **NTP Integration**: Clock only displays after successful NTP time synchronization
- **Timezone Support**: Uses timezone setting from configuration for correct local time

### Universal Blink Helper
- **Reusable Function**: New `getBlinkState()` utility for all blink functionality
- **Stateless Design**: No global toggle variables needed
- **Flexible Frequency**: Easy to configure any blink rate (e.g., 500ms = 2 Hz)
- **Synchronized Blinking**: All elements using same interval blink in sync

## 🗑️ Removed

### GPIO Viewer Cleanup
- **Firmware**: Removed library includes and initialization code
- **Settings**: Removed from NVS storage and configuration management
- **Web UI**: Removed checkbox, button, and related styling
- **Build**: Removed library dependencies from platformio.ini
- **Documentation**: Updated instructions to reflect removal

## 🔧 Technical Details

- **LCD Format**: `DD-MM-YYYY HH:MM` (exactly 16 characters for 16x2 LCD)
- **Update Rate**: 500ms refresh for smooth colon blinking
- **Memory**: Reduced RAM usage by removing GPIO Viewer overhead
- **Code Quality**: Cleaner architecture with universal blink helper

## 📦 Installation

1. Download both binaries: `fw-2026.2.10.02.bin` and `fs-2026.2.10.02.bin`
2. Flash via web interface (Settings → Software Updates → Check for Updates)
3. Or use PlatformIO: `pio run -e esp32s3dev -t upload` and `pio run -e esp32s3dev -t uploadfs`

## ⚠️ Breaking Changes

- **GPIO Viewer Removed**: If you relied on GPIO Viewer for debugging, it's no longer available
- **Settings Migration**: GPIO Viewer setting automatically removed on upgrade

## 🐛 Bug Fixes

- None in this release (feature additions and cleanup only)

---

**Full Changelog**: https://github.com/Mooijman/MS11-Master/blob/main/CHANGELOG.md
