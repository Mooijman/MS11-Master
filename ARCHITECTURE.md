# Architecture Decisions (ESP32-Base)

This document consolidates architecture and technical decisions for the ESP32-Base firmware.

## Single entrypoint + centralized settings
- The firmware has a single entrypoint in [src/main.cpp](src/main.cpp) that owns WiFi/AP mode, web server routes, OTA pull updates, OLED status, and LittleFS web UI.
- Device settings are centralized in the `Settings` module ([include/settings.h](include/settings.h), [src/settings.cpp](src/settings.cpp)) backed by NVS to avoid scattered globals.
- A compatibility layer in `main.cpp` aliases `settings.*` as `String&` to keep legacy handlers working without refactors.

## Config + defaults live in config.h
- Constants, defaults, endpoints, and placeholders are centralized in [include/config.h](include/config.h) for single-source configuration.
- Compile-time version strings in `config.h` are the authoritative version source (fw/fs).

## Versioning + release tagging
- Release tag names are **without** `v` prefix: `YYYY-M.0.NN`.
- Firmware/Filesystem assets use `fw-YYYY-M.0.NN.bin` and `fs-YYYY-M.0.NN.bin` naming to match update logic.
- Version comparison strips only `fw-`/`fs-` prefixes; no `v` stripping expected.

## Automatic version sync
- On boot, NVS-stored firmware/filesystem versions are synchronized with compile-time versions.
- This prevents manual updates in multiple places after OTA, GitHub updates, or serial uploads.

## OTA pull updates via GitHub Releases API
- Firmware and LittleFS updates are pulled from the GitHub Releases API URL (`releases/latest`).
- The update system uses the API asset `url` field and requires `Accept: application/octet-stream` for downloads.
- Update state and URLs are persisted in NVS (`ota` namespace).
- Firmware update auto-reboots; LittleFS update requires manual power cycle.

## Web UI + templating
- Web UI assets are stored in LittleFS under [data/](data/) and served by AsyncWebServer.
- HTML templates use placeholder tokens (`TEMPLATE_*`) defined in [include/config.h](include/config.h) and replaced in the `/settings` route.

## WiFi/AP mode + captive portal
- STA mode: serves `/`, `/settings`, `/update`, `/files` and `/api/*` endpoints.
- AP mode: captive portal serves `wifimanager.html` and handles `/scan` with cached async scans.
- Failed STA connect clears NVS credentials so AP mode starts on next reboot.

## Feature gating (debug)
- Debug flags are stored as strings (`"true"/"false"` or `"on"/"off"`).
- Turning off debug disables GPIO Viewer and ArduinoOTA in settings handling.

## Partitions + storage
- Custom partition tables enable dual OTA + LittleFS (see [partitions_esp32.csv](partitions_esp32.csv), [partitions_esp32s3.csv](partitions_esp32s3.csv)).
- LittleFS is mounted at `/littlefs` with limited file handles (per [include/config.h](include/config.h)).

## Known hardware interaction decision
- GPIO Viewer can interfere with I2C/SPI; tests and feature request are documented (see [GPIOVIEWER_FEATURE_REQUEST.md](GPIOVIEWER_FEATURE_REQUEST.md), [GITHUB_TEST_REPORT.md](GITHUB_TEST_REPORT.md)).

## References
- Change history: [CHANGELOG.md](CHANGELOG.md)
- Rationale and decisions: [DECISIONS.md](DECISIONS.md)
- OTA analysis and checklist: [docs/OTA_Pull_Update_Analysis.md](docs/OTA_Pull_Update_Analysis.md), [docs/OTA_Testing_Checklist.md](docs/OTA_Testing_Checklist.md)
