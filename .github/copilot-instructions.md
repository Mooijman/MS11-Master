# Copilot instructions for ESP32-Base

## Architecture & data flow
- Single firmware entrypoint in [src/main.cpp](../src/main.cpp) owns WiFi/AP mode, AsyncWebServer routes, OTA pull updates, OLED status, and LittleFS web UI.
- Device settings are centralized in `Settings` (NVS-backed) in [src/settings.cpp](../src/settings.cpp) + [include/settings.h](../include/settings.h). `main.cpp` aliases `settings.*` as `String&` globals—modify settings via `Settings::load/save/saveNetwork/saveFeatures` instead of adding new globals.
- Constants and feature defaults live in [include/config.h](../include/config.h) (versions, OTA URLs, NVS namespaces, HTML placeholders, timing). Update versions here for releases.
- Web UI is served from LittleFS under [data/](../data/) and templated with placeholder tokens (see `TEMPLATE_*` in [include/config.h](../include/config.h)). `main.cpp` fills placeholders for `/settings` via a template processor.
- OTA pull updates use GitHub Releases API URLs; `main.cpp` downloads assets via the API `url` field (requires `Accept: application/octet-stream`) and optional GitHub token. Update state is persisted in NVS namespace `ota`.

## Key endpoints & behavior
- STA mode routes: `/` (index), `/settings`, `/update`, `/files`, plus JSON APIs under `/api/*` for update status/check/install and LittleFS file ops; see [src/main.cpp](../src/main.cpp).
- AP mode (captive portal): `/` and `onNotFound` serve `wifimanager.html`, WiFi scan is `/scan` with caching; see [src/main.cpp](../src/main.cpp).
- OTA install: firmware updates auto-reboot; LittleFS updates require manual power cycle (flag `filesystemUpdateDone`).

## Developer workflows (PlatformIO)
- Build: `pio run -e esp32dev` (or `esp32s3dev`).
- Upload firmware: `pio run --target upload` (or `-e esp32dev`).
- Upload LittleFS: `pio run --target uploadfs`.
- Always upload both firmware and LittleFS together after changes: run firmware upload then LittleFS upload.
- OTA dev upload: `pio run -t upload --upload-port <ip-or-hostname>`.
- Partitions are custom (LittleFS + dual OTA); see [platformio.ini](../platformio.ini) and [partitions_esp32.csv](../partitions_esp32.csv).

## Project-specific conventions
- Versions follow `fw-YYYY-x.y.zz` and `fs-YYYY-x.y.zz` (see [include/config.h](../include/config.h)); compare logic strips `fw-`/`fs-` only.
- Feature flags are stored as strings (`"true"/"false"` or `"on"/"off"`), not booleans. Use `Settings::stringToBool` when reading.
- Debug features (GPIO Viewer, OTA) are gated by the `debug` flag; turning off debug also disables GPIO Viewer and OTA in `/settings`.
- WiFi credentials resets happen by removing NVS keys in namespace `config` (see `reset-wifi` handler in [src/main.cpp](../src/main.cpp)).

## Where to look first
- Web UI & placeholders: [data/](../data/) + [include/config.h](../include/config.h).
- Settings storage and defaults: [src/settings.cpp](../src/settings.cpp).
- OTA logic and API endpoints: [src/main.cpp](../src/main.cpp).

## Docs & decision records
- Release history and notable changes: [CHANGELOG.md](../CHANGELOG.md).
- Architecture and release/OTA decisions (tagging, naming, version sync rationale): [DECISIONS.md](../DECISIONS.md).
- Open feature backlog and refactor ideas: [TODO.md](../TODO.md).
- GPIO Viewer I2C/SPI interference request and proposed fix: [GPIOVIEWER_FEATURE_REQUEST.md](../GPIOVIEWER_FEATURE_REQUEST.md).
- GPIO Viewer test results for skipping peripheral pins: [GITHUB_TEST_REPORT.md](../GITHUB_TEST_REPORT.md).
- OTA pull update design analysis: [docs/OTA_Pull_Update_Analysis.md](../docs/OTA_Pull_Update_Analysis.md).
- OTA verification checklist: [docs/OTA_Testing_Checklist.md](../docs/OTA_Testing_Checklist.md).
