# Copilot Instructions for ESP32-Base

## Architecture Overview
**Single entrypoint design**: [src/main.cpp](../src/main.cpp) owns WiFi/AP mode, AsyncWebServer routes, GitHub OTA, OLED status display, and LittleFS web UI. All module communication routes through `main.cpp`'s setup/loop.

**Three-tier settings layer**:
1. Compile-time defaults: [include/config.h](../include/config.h) (`DEFAULT_*` constants)
2. NVS storage (Preferences): `Settings` class loads/saves to namespaces `config` and `ota`
3. Runtime globals: `main.cpp` aliases `settings.*` as `String&` references for backward compatibility

**Data flow**: App state → `Settings::save()` → NVS → Device reboot → `Settings::load()` → runtime globals

## Critical Modules

### Settings Management
- **Class**: `Settings` ([include/settings.h](../include/settings.h), [src/settings.cpp](../src/settings.cpp))
- **Key methods**: `load()`, `save()`, `saveNetwork()`, `saveFeatures()`, `syncVersions()`
- **Storage**: NVS namespaces `config` (network, features) and `ota` (update state)
- **Convention**: Feature flags stored as strings (`"true"/"false"` or `"on"/"off"`); use `stringToBool()` for comparison
- **DO NOT** add new globals to `main.cpp`—use `Settings` instead

### GitHub Updater (Modularized in v1.0.13+)
- **Class**: `GitHubUpdater` ([include/github_updater.h](../include/github_updater.h), [src/github_updater.cpp](../src/github_updater.cpp))
- **Responsibility**: Version comparison, API calls, NVS persistence for update state
- **Entry points**: `checkGitHubRelease()` (check), `downloadAndInstallFirmware/LittleFS()` (install), API handlers
- **API handlers** return JSON strings; `main.cpp` routes them via AsyncWebServer

### Web UI + Templating
- **Files**: [data/](../data/) HTML/CSS served from LittleFS
- **Templating**: Placeholder tokens (`TEMPLATE_*`) in `config.h` replaced at runtime in `/settings` route
- **Example**: `TEMPLATE_FIRMWARE_VERSION` → `"FIRMWARE_VERSION"` token in HTML → replaced by handler lambda

## Versioning & Release Process

**Format** (enforced):
- Firmware: `fw-YYYY-M.m.pp` (e.g., `fw-2026-1.0.14`)
- Filesystem: `fs-YYYY-M.m.pp` (e.g., `fs-2026-1.0.14`)
- GitHub tags: `YYYY-M.m.pp` (NO `v` prefix)

**Auto-sync on boot**: `Settings::syncVersions()` compares compile-time defines against NVS and updates NVS if mismatch (e.g., after GitHub update).

**Release workflow**: Update version in [include/config.h](../include/config.h), build both envs, tag as `YYYY-M.m.pp`, upload binaries named `fw-*.bin` and `fs-*.bin` to GitHub release.

## Build & Deployment

**PlatformIO commands** (always both together):
```bash
pio run -e esp32dev -t upload      # Firmware
pio run -e esp32dev -t uploadfs    # LittleFS (includes current.info)
pio run -e esp32dev                # Build only (esp32s3dev for S3)
```

**Partitions**: Custom table at [partitions_esp32.csv](../partitions_esp32.csv) (dual OTA + LittleFS); S3 variant at [partitions_esp32s3.csv](../partitions_esp32s3.csv).

**OTA behavior**: Firmware auto-reboots after install; LittleFS requires manual power cycle.

## Key Conventions

- **No `v` prefix on tags** (GitHub API returns `tag_name` without `v`)
- **Feature gating**: Debug flag (`debugEnabled`) controls GPIO Viewer and OTA; disable debug to disable both
- **WiFi reset**: Removes NVS keys in namespace `config` via `/reset-wifi` handler, triggers AP mode on next boot
- **Async WiFi scan**: Cached in `cachedScanResults` with `WIFI_SCAN_CACHE_INTERVAL` (10s); async scan via `WiFi.scanNetworks(true)`
- **Loop structure** (v1.0.14+): Refactored into `handleDisplayTasks()`, `handleNetworkTasks()`, `handleSystemTasks()`

## Gotchas & Known Issues

1. **Feature flags mismatch**: Settings store strings; HTML form POST sends `"on"/"off"` but NVS may have `"true"/"false"`. Always normalize in handlers (see DHCP fix in DECISIONS.md).
2. **I2C/SPI interference**: GPIO Viewer calls `digitalRead()` on all pins, including I2C (pins 4, 5). Causes OLED corruption; feature request pending (GPIOVIEWER_FEATURE_REQUEST.md, GITHUB_TEST_REPORT.md).
3. **LittleFS mount path**: `/littlefs` (not `/` root); see `LITTLEFS_BASE_PATH` in config.h.
4. **Manual power cycle for LittleFS**: Update installs but doesn't reboot—document for users.

## Where to Start

**New feature?** Update [include/config.h](../include/config.h) with defaults, add to Settings class, handle in web routes.

**Bug fix?** Check [CHANGELOG.md](../CHANGELOG.md) and [DECISIONS.md](../DECISIONS.md) for context on recent changes (e.g., code cleanup in 1.0.14, refactoring in 1.0.13).

**Complex task?** Review [docs/](../docs/) for analysis: OTA design, testing checklist, known GPIO issues.

## Essential Files

| File | Purpose |
|------|---------|
| [src/main.cpp](../src/main.cpp) (1400 lines) | Web server, WiFi/AP, loop tasks, route handlers |
| [include/config.h](../include/config.h) | Versions, defaults, endpoints, constants, timing |
| [src/settings.cpp](../src/settings.cpp) | NVS load/save, version sync, factory reset |
| [src/github_updater.cpp](../src/github_updater.cpp) | API calls, version compare, download/install logic |
| [data/settings.html](../data/settings.html) | Settings UI with template placeholders |
| [data/update.html](../data/update.html) | Update UI with JavaScript polling |
