# Copilot Instructions for ESP32S3-Base

## Quick Facts
- **Hardware**: Seeed XIAO ESP32-S3 (8MB flash, 320KB RAM)
- **Current Version**: 2026.1.1.05 (Timezone selection + NTP sync)
- **Firmware Size**: ~1.33MB (67% of 1.92MB OTA partition)
- **Status**: Production-ready with stable architecture

## Big Picture Architecture
- Single entrypoint: [src/main.cpp](../src/main.cpp) owns WiFi/AP mode, AsyncWebServer routes, OLED status, and modular task loop (`handleDisplayTasks()`, `handleNetworkTasks()`, `handleSystemTasks()`).
- **Settings are a 3-tier stack**: compile-time defaults in [include/config.h](../include/config.h) → **NVS via `Settings`** in [src/settings.cpp](../src/settings.cpp) → runtime `String&` aliases in [src/main.cpp](../src/main.cpp).
- **Data flow**: UI/API change → `Settings::save()` → NVS → reboot → `Settings::load()` → display updates.
- **Automatic version sync**: `Settings::syncVersions()` detects compile-time version changes and updates NVS on every boot.

## Core Modules & Responsibilities
- **`Settings`** ([include/settings.h](../include/settings.h) / [src/settings.cpp](../src/settings.cpp)): NVS namespaces `config` and `ota`. Methods: `initialize()`, `load()`, `save()`, `syncVersions()` (detects firmware version changes), `clearWiFi()`. Always call `syncVersions()` after compile-time version bump.
- **`GitHubUpdater`** ([include/github_updater.h](../include/github_updater.h) / [src/github_updater.cpp](../src/github_updater.cpp)): version compare, GitHub API fetch, download/install for firmware + LittleFS. All API handlers return JSON strings (no redirects).
- **`WiFiManager`** ([src/wifi_manager.cpp](../src/wifi_manager.cpp)): async WiFi connect with DHCP/static IP fallback; triggers AP mode if connection fails.

## Web UI + Template Binding
- Static files live in [data/](../data/) and are served from LittleFS at `/littlefs`.
- **Template replacement** happens in [src/main.cpp](../src/main.cpp) handlers: Template tokens (`%VAR%` format) are replaced server-side before sending to client.
- [data/update.html](../data/update.html) polls `/api/update/status` every 500ms during updates; auto-redirects on success or error.

## Build & Upload Workflow
```bash
# Standard build (default: esp32s3dev)
pio run                        # Firmware only
pio run -t buildfs            # Filesystem only (LittleFS, 512KB)
pio run -t upload && pio run -t uploadfs  # Both together

# For WROOM ESP32 (legacy, still supported)
pio run -e esp32dev -t upload
```

## Critical Conventions & Gotchas
⚠️ **STRING FLAGS**: Feature flags stored as `"true"/"false"` or `"on"/"off"` strings. Always normalize before compare:
```cpp
bool isEnabled = (useDHCP == "true" || useDHCP == "on");  // ✅ Correct
if (useDHCP == "true") { ... }  // ❌ Risky (might be "on")
```

⚠️ **VERSION PREFIXES**: GitHub assets named `fw-YYYY.M.m.p.bin` and `fs-YYYY.M.m.p.bin` without `v` prefix. Tag names: `YYYY.M.m.p` (no `v`). Prefixes are stripped during version comparison.

⚠️ **DEBUG FLAG GATES**: Disabling debug in settings also disables GPIO Viewer and OTA (security design). See [src/main.cpp](../src/main.cpp) lines ~1080-1095.

⚠️ **LITTLEFS POWER CYCLE**: LittleFS updates require manual power cycle after install (firmware updates auto-reboot, but filesystem doesn't).

⚠️ **NVS DEFAULTS**: First boot triggers `Settings::initialize()` which seeds NVS with compile-time defaults. Only runs once (checked via `isInitialized()`).

## Feature Interaction Matrix
- **Debug OFF** → GPIO Viewer OFF, OTA OFF (for security)
- **Updates ON** → Requires GitHub token in settings (blank token = "Check" shows error)
- **NTP ON** → Syncs on boot; if timeout, falls back to stored date (max 1 update/day to reduce flash wear)
- **AP Mode** → Triggered when WiFi connect times out; DNS server active for captive portal

## Where to Start
- **New setting**: Add to [include/config.h](../include/config.h) `#define`, extend `Settings` class with new field, call `settings.save()` in POST handler.
- **New API endpoint**: Add route in [src/main.cpp](../src/main.cpp) and handler in `GitHubUpdater` if it's update-related.
- **Version bump**: Edit `#define FIRMWARE_VERSION` / `FILESYSTEM_VERSION` in [include/config.h](../include/config.h); `syncVersions()` auto-updates NVS on next boot.
- **Debug workflow**: Check [docs/OTA_Testing_Checklist.md](../docs/OTA_Testing_Checklist.md) and [CHANGELOG.md](../CHANGELOG.md) for recent API changes.
