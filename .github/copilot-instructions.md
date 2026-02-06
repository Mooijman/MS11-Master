# Copilot Instructions for ESP32S3-Base

## Big Picture Architecture
- Single entrypoint: [src/main.cpp](../src/main.cpp) owns WiFi/AP mode, AsyncWebServer routes, OLED status, and task loop (`handleDisplayTasks()`, `handleNetworkTasks()`, `handleSystemTasks()`).
- Settings are a 3-tier stack: compile-time defaults in [include/config.h](../include/config.h) → NVS via `Settings` in [src/settings.cpp](../src/settings.cpp) → runtime `String&` aliases in [src/main.cpp](../src/main.cpp).
- Data flow: UI/API change → `Settings::save()` → NVS → reboot → `Settings::load()`.

## Core Modules & Responsibilities
- `Settings` in [include/settings.h](../include/settings.h) / [src/settings.cpp](../src/settings.cpp): NVS namespaces `config` and `ota`, plus `syncVersions()` to keep NVS in sync with compile-time versions.
- `GitHubUpdater` in [include/github_updater.h](../include/github_updater.h) / [src/github_updater.cpp](../src/github_updater.cpp): version compare, GitHub API fetch, download/install for firmware + LittleFS; API handlers return JSON strings.
- `WiFiManager` in [src/wifi_manager.cpp](../src/wifi_manager.cpp): used by [src/main.cpp](../src/main.cpp) to connect or fall back to AP/captive portal.

## Web UI + Template Binding
- Static UI lives in [data/](../data/) and is served from LittleFS at runtime.
- Template tokens (`TEMPLATE_*` in [include/config.h](../include/config.h)) are replaced in the `/settings` route handler in [src/main.cpp](../src/main.cpp).
- Update UI logic/polling lives in [data/update.html](../data/update.html) and hits `/api/update/*` routes.

## Build / Upload Workflow (PlatformIO)
- Default env is `esp32s3dev` in [platformio.ini](../platformio.ini) with `partitions_xiao_s3.csv` and LittleFS.
- Typical commands (run firmware + filesystem together):
	- `pio run -e esp32s3dev -t upload`
	- `pio run -e esp32s3dev -t uploadfs`

## Project Conventions & Gotchas
- Feature flags are stored as strings (`"true"/"false"` or `"on"/"off"`); normalize before comparing (see DHCP normalization in [DECISIONS.md](../DECISIONS.md)).
- Debug flag gates GPIO Viewer and OTA; turning debug off disables both in [src/main.cpp](../src/main.cpp).
- GitHub updater expects assets named `fw-*.bin` and `fs-*.bin` and tags without a `v` prefix; versions in [include/config.h](../include/config.h) are `fw-2026.1.1.03` / `fs-2026.1.1.03` and are stripped during comparisons.
- LittleFS is mounted at `/littlefs` (see `LITTLEFS_BASE_PATH` in [include/config.h](../include/config.h)); filesystem updates require a manual power cycle after install.
- AP mode uses a captive portal and cached async scan results (`cachedScanResults` in [src/main.cpp](../src/main.cpp)).

## Where to Start
- New feature: add defaults in [include/config.h](../include/config.h), extend `Settings`, then wire UI/routes in [src/main.cpp](../src/main.cpp).
- Update behavior: check [docs/OTA_Testing_Checklist.md](../docs/OTA_Testing_Checklist.md) and [CHANGELOG.md](../CHANGELOG.md) for recent constraints.
