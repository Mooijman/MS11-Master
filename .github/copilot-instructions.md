# Copilot Instructions (ESP32S3-Base)

## Big picture
- Single entrypoint: [src/main.cpp](../src/main.cpp) owns WiFi/AP mode, AsyncWebServer routes, OLED status, and the task loop (`handleDisplayTasks()`, `handleNetworkTasks()`, `handleSystemTasks()`).
- Settings are a 3‑tier stack: compile‑time defaults in [include/config.h](../include/config.h) → NVS via `Settings` in [src/settings.cpp](../src/settings.cpp) → runtime `String&` aliases in [src/main.cpp](../src/main.cpp).
- Data flow: UI/API change → `settings.save()` → NVS → reboot → `settings.load()` → UI/display updates.
- GitHub OTA logic lives in [src/github_updater.cpp](../src/github_updater.cpp); its API handlers return JSON only (no redirects). UI behavior is driven by [data/update.html](../data/update.html).

## Core modules
- `Settings` ([include/settings.h](../include/settings.h), [src/settings.cpp](../src/settings.cpp)): NVS namespaces `config` + `ota`, version sync via `syncVersions()` on boot.
- `WiFiManager` ([src/wifi_manager.cpp](../src/wifi_manager.cpp)): STA connect with DHCP/static IP; if connect fails, AP + captive portal starts in `main.cpp`.
- `GitHubUpdater` ([include/github_updater.h](../include/github_updater.h), [src/github_updater.cpp](../src/github_updater.cpp)): version compare + download/install firmware/LittleFS.

## Project-specific conventions
- String flags are stored as `"true"/"false"` or `"on"/"off"`; always normalize before comparisons (see DHCP handling in [src/main.cpp](../src/main.cpp)).
- Versioning has no `v` prefix. Binaries: `fw-YYYY.M.m.p.bin` and `fs-YYYY.M.m.p.bin`. Tags: `YYYY.M.m.p`.
- Debug OFF also disables GPIO Viewer + OTA for security (see settings POST handler in [src/main.cpp](../src/main.cpp)).
- NTP date writes are wear‑limited (see `saveStoredDateIfNeeded()` in [src/settings.cpp](../src/settings.cpp)).

## Web UI patterns
- HTML templates use `%VAR%` tokens replaced in `server.on("/settings", HTTP_GET)` in [src/main.cpp](../src/main.cpp).
- API routes (`/api/update/*`) must return JSON only; frontend redirects are handled in [data/update.html](../data/update.html).

## Build / upload workflow (PlatformIO)
- Default env is ESP32‑S3. Firmware: `pio run -e esp32s3dev` or upload `pio run -e esp32s3dev -t upload`.
- LittleFS: `pio run -e esp32s3dev -t buildfs` / `-t uploadfs`. Firmware + FS should be uploaded together after UI changes.

## Release & partitions
- Update `FIRMWARE_VERSION` and `FILESYSTEM_VERSION` in [include/config.h](../include/config.h); `Settings::syncVersions()` updates NVS on boot.
- LittleFS OTA depends on partition subtype; see [partitions_xiao_s3.csv](../partitions_xiao_s3.csv).

## Where to extend
- New setting: add `#define` in [include/config.h](../include/config.h), add member in `Settings`, wire POST handler in [src/main.cpp](../src/main.cpp), then `settings.save()`.
- New update API: add route in [src/main.cpp](../src/main.cpp) and handler in `GitHubUpdater`.
