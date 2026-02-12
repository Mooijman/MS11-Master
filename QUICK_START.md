# Quick Reference Guide - MS11-Master Project

**Last Updated**: 12 February 2026 | **Version**: 2026.2.12.02

## 📱 Project at a Glance

MS11-Master: ESP32-S3 IoT controller with WiFi manager, OTA updates, web interface, NTP time sync, timezone support, and LCD 16x2 display with MS11-control monitoring.

- **Board**: XIAO ESP32-S3 (8MB flash, 320KB RAM)
- **Status**: ✅ Production ready
- **Latest Version**: 2026.2.12.02 (LCD display improvements - startup blink, connection monitoring)
- **Firmware Size**: 1.35MB (68.6% of OTA partition)

## 🚀 Quick Start

### Build & Upload
```bash
# Navigate to project
cd "/Users/jeroen/Library/Mobile Documents/com~apple~CloudDocs/PlatformIO/Projects/MS11/MS11-master"

# Build firmware
pio run -e esp32s3dev

# Build filesystem (LittleFS)
pio run -e esp32s3dev -t buildfs

# Upload both (optional: use combined upload)
pio run -e esp32s3dev -t upload
pio run -e esp32s3dev -t uploadfs
```

### First Boot
1. ESP32 starts in **AP mode** (SSID: ESP-WIFI-MANAGER)
2. Captive portal opens automatically
3. Configure WiFi, IP settings, and timezone
4. Device reboots and connects to configured network

## 🎯 Core Architecture

```
main.cpp (Core app)
├── WiFi Manager (station/AP)
├── Web Server (AsyncWebServer)
│   ├── /settings → Template binding
│   ├── /update → OTA interface
│   ├── /files → File browser
│   └── /api/* → JSON endpoints
├── Settings Manager (NVS persistence)
├── GitHub Updater (pull OTA)
├── Time Sync (NTP + fallback)
└── OLED Display (SSD1306)

include/config.h (Centralized config)
include/settings.h (Settings class)
data/settings.html (Web UI)
```

## 🔧 Key Files

| File | Purpose |
|------|---------|
| `include/config.h` | ALL constants, defaults, templates |
| `include/settings.h` | Settings class API |
| `src/settings.cpp` | NVS persistence (wear-limiting) |
| `src/main.cpp` | Core app, web server, sync loop |
| `data/settings.html` | Configuration UI |
| `platformio.ini` | Build config (esp32s3dev) |

## 🔌 Hardware Pinout (XIAO S3)

| Function | GPIO | Label | Notes |
|----------|------|-------|-------|
| **I2C Bus 0 (Slave @ 100kHz)** | | | Standard I2C |
| Slave SDA | GPIO5 | D4 | ⭐ Default I2C SDA |
| Slave SCL | GPIO6 | D5 | ⭐ Default I2C SCL |
| **I2C Bus 1 (Display @ 100kHz)** | | | |
| Display SDA | GPIO8 | D9 | |
| Display SCL | GPIO9 | D10 | |
| **UART (Optional)** | | | Not used |
| UART TX | GPIO43 | D6 | |
| UART RX | GPIO44 | D7 | |
| **Power** | USB-C | — | 5V input |

## 📡 Web Interface Routes

```
http://<device-ip>/          → Status page
http://<device-ip>/settings  → Configuration
http://<device-ip>/update    → OTA updates
http://<device-ip>/files     → File browser (debug only)
http://<device-ip>/i2c       → I2C scanner (debug only)
```

## ⚙️ Settings Tiers

1. **Compile-time**: `#define` in `include/config.h` (defaults)
2. **NVS Storage**: Preferences in namespace `config` (persisted)
3. **Runtime**: String& aliases in `main.cpp` (working copy)

### NVS Keys (Namespace: `config`)
- `ssid`, `pass` → WiFi credentials
- `ip`, `gateway`, `netmask`, `dhcp` → Network config
- `ntp`, `timezone` → Time sync settings
- `debug`, `gpioviewer`, `ota`, `updates` → Feature flags
- `dateY`, `dateM`, `dateD`, `dateSaved` → Fallback date (wear-limited)

## 🕐 Time Sync Details

### NTP Configuration
- **Enabled by default**: Yes (DEFAULT_NTP_ENABLED = true)
- **Servers**: pool.ntp.org, time.nist.gov, time.google.com
- **Timeout**: 5 seconds
- **Sync on**: Boot, settings save, periodic (if needed)

### Timezone Support
- **15+ Timezones**: UTC, CET, EST, PST, JST, etc.
- **Default**: UTC
- **Stored in NVS**: Yes (persists across reboots)
- **UI**: Dropdown in settings (conditional visibility)

### Date Fallback (Wear-Limiting)
```cpp
// Max 1 write per calendar day
saveStoredDateIfNeeded(year, month, day);

// Stored as: dateY, dateM, dateD (integers)
// Tracked by: dateSaved (YYYYMMDD as uint32)
```

## 🔐 Security Defaults

```cpp
Debug:        OFF (false)
GPIO Viewer:  OFF (false)
OTA Upload:   OFF (false)  ← Explicit opt-in required
Updates:      OFF (false)  ← User consent required
DHCP:         ON (true)    ← Most common setup
NTP:          ON (true)    ← Time sync enabled
```

## 📊 Memory Status

```
Firmware:  1,333,968 bytes (67.8% of 1.92MB OTA partition)
RAM:       51,588 bytes (15.7% of 320KB)
LittleFS:  512KB total (116KB web files, 396KB free)
Flash:     4.45MB of 8MB used (55%)
```

## 🔄 OTA Update Flow

### GitHub Release
1. Create release with tag `YYYY.M.m.pp`
2. Attach binaries: `fw-*.bin` and `fs-*.bin`
3. Binary naming: `fw-2026-1-1-05.bin` format

### Device Update
1. Check GitHub releases (if enabled)
2. Compare versions
3. Download firmware/filesystem
4. Install via Update library
5. Reboot to new partition
6. Bootloader validates OTA data

### Local OTA
```bash
# ArduinoOTA enabled on port 3232 (if debug ON)
# Hostname: ESP32-Base
pio run -e esp32s3dev -t upload

# Filesystem upload via OTA
pio run -e esp32s3dev -t uploadfs --upload-port <ip-adres>
```

## 🐛 Debugging

### Enable Debug Mode
1. Go to `/settings`
2. Check "Enable Debug Features"
3. Device reboots
4. Access `/i2c` for I2C scanner
5. GPIO Viewer available on port 5555

### Serial Monitor
```bash
# Monitor at 115200 baud
pio device monitor -b 115200

# Clear NVS and reboot
# Manually erase in code: settings.reset();
```

### Common Issues

| Issue | Solution |
|-------|----------|
| Device won't connect to WiFi | Check SSID/password in `/settings` or reset via `/reset-wifi` |
| OLED not showing | Verify GPIO6/7 (I2C) connections, check `include/config.h` pins |
| Update fails | Ensure GitHub token in settings if private repo |
| NTP not syncing | Check internet connection, verify NTP servers reachable |
| Flash full | Monitor firmware size in build output (~67% current) |

## 📚 Documentation Reference

| Document | Content |
|----------|---------|
| `README.md` | Overview, features, dependencies |
| `.github/copilot-instructions.md` | AI agent guidance (start here!) |
| `DECISIONS.md` | Technical decisions, workflows |
| `SESSION_SUMMARY.md` | Latest session work (Feb 6, 2026) |
| `CHANGELOG.md` | Release history, version notes |
| `PROJECT_CLOSEOUT.md` | Deployment readiness checklist |
| `XIAO_S3_SETUP.md` | Hardware-specific setup |
| `OTA_Testing_Checklist.md` | Testing procedures |

## 🔑 Key Patterns

### Settings Access
```cpp
// In main.cpp - global references
String& ssid = settings.ssid;
String& debugEnabled = settings.debugEnabled;

// In POST handlers
if (request->hasParam("timezone", true)) {
  String newTz = request->getParam("timezone", true)->value();
  if (newTz != timezone) {
    timezone = newTz;
    configChanged = true;
  }
}
```

### NVS Persistence
```cpp
// Save settings
settings.save();  // Full save
settings.saveNetwork();  // Network only
settings.saveFeatures();  // Features only

// Load on boot
settings.load();
settings.syncVersions();  // Sync compile-time versions
```

### Template Binding (Web UI)
```cpp
// In /settings route handler
request->send(LittleFS, "/settings.html", "text/html", false, 
  [](const String& var) -> String {
    if (var == "NTP_CHECKED") {
      return Settings::stringToBool(ntpEnabled) ? "checked" : "";
    }
    if (var == "TIMEZONE") {
      return timezone;
    }
    return String();
  }
);
```

### Time Sync
```cpp
// Boot sync
syncTimeIfEnabled();

// NTP + Fallback
void syncTimeIfEnabled() {
  if (!Settings::stringToBool(ntpEnabled)) return;
  
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
  time_t now = 0;
  while (now < NTP_VALID_TIME && millis() < timeout) {
    time(&now);
    delay(100);
  }
  
  if (now >= NTP_VALID_TIME) {
    // Success - optionally save date for fallback
    settings.saveStoredDateIfNeeded(...);
  } else {
    // NTP failed - use stored date
    StoredDate stored = settings.getStoredDate();
    // Restore via mktime() + settimeofday()
  }
}
```

## 🚦 Build & Release Workflow

### Version Bumping
```bash
# Edit include/config.h
FIRMWARE_VERSION "fw-2026.1.1.08"
FILESYSTEM_VERSION "fs-2026.1.1.08"

# Build
pio run -e esp32s3dev
pio run -e esp32s3dev -t buildfs

# Copy binaries
mkdir -p release/2026.1.1.08
cp .pio/build/esp32s3dev/firmware.bin release/2026.1.1.08/fw-2026.1.1.08.bin
cp .pio/build/esp32s3dev/littlefs.bin release/2026.1.1.08/fs-2026.1.1.08.bin

# Git
git add -A
git commit -m "v2026.1.1.08: Feature description"
git tag 2026.1.1.08
git push && git push origin 2026.1.1.08

# Release via GitHub CLI
gh release create 2026.1.1.08 \
  release/2026.1.1.08/fw-2026.1.1.08.bin \
  release/2026.1.1.08/fs-2026.1.1.08.bin \
  --title "v2026.1.1.08 - Title" --notes-file release_notes.md
```

## ✅ Pre-Deployment Checklist

- [ ] Test on actual XIAO ESP32-S3 hardware
- [ ] Verify NTP sync across timezones
- [ ] Test WiFi AP mode and captive portal
- [ ] Confirm GitHub release download works
- [ ] Test LittleFS persistence after power cycle
- [ ] Monitor NVS wear leveling (extended test)
- [ ] Validate all web UI features

## 🤝 Next Steps

1. **Review**: Read `.github/copilot-instructions.md` (5 min)
2. **Hardware**: Flash latest firmware to XIAO S3
3. **Testing**: Follow `OTA_Testing_Checklist.md`
4. **Documentation**: Update as you work
5. **Enhancement**: Choose from improvement opportunities in PROJECT_CLOSEOUT.md

---

**Version**: 2026.1.1.08 | **Last Updated**: Feb 6, 2026  
**Status**: ✅ Ready for Deployment (pending testing on hardware)
