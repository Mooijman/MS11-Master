# Copilot Instructions (MS11-Master)

## Big picture
- Single entrypoint: [src/main.cpp](../src/main.cpp) owns WiFi/AP mode, AsyncWebServer routes, OLED status, and the task loop (`handleDisplayTasks()`, `handleNetworkTasks()`, `handleSystemTasks()`).
- Settings are a 3‑tier stack: compile‑time defaults in [include/config.h](../include/config.h) → NVS via `Settings` in [src/settings.cpp](../src/settings.cpp) → runtime `String&` aliases in [src/main.cpp](../src/main.cpp).
- Data flow: UI/API change → `settings.save()` → NVS → reboot → `settings.load()` → UI/display updates.
- GitHub OTA logic lives in [src/github_updater.cpp](../src/github_updater.cpp); its API handlers return JSON only (no redirects). UI behavior is driven by [data/update.html](../data/update.html).
- **Hardware**: XIAO ESP32-S3 (Seeed Studio) - 8MB flash, 320KB RAM, dual I2C buses (Bus 0: GPIO5/6 @ 100kHz for slave, Bus 1: GPIO8/9 @ 100kHz for displays), 921600 baud USB-C.

## Core modules
- `Settings` ([include/settings.h](../include/settings.h), [src/settings.cpp](../src/settings.cpp)): NVS namespaces `config` + `ota`, version sync via `syncVersions()` on boot. Date/time storage with wear-limiting (NTP fallback, boot time tracking). First boot detection via `isInitialized()` checking for `"ota"` key.
- `WiFiManager` ([src/wifi_manager.cpp](../src/wifi_manager.cpp)): STA connect with DHCP/static IP; if connect fails, AP + captive portal starts in `main.cpp`.
- `GitHubUpdater` ([include/github_updater.h](../include/github_updater.h), [src/github_updater.cpp](../src/github_updater.cpp)): version compare + download/install firmware/LittleFS. API handlers return JSON; UI polling at 500ms for smooth updates.
- `MD11SlaveUpdate` ([include/md11_slave_update.h](../include/md11_slave_update.h), [src/md11_slave_update.cpp](../src/md11_slave_update.cpp)): I2C-based bootloader for external ATmega chips; sends Intel HEX via I2C using 4-byte header protocol (CMD_ACCESS_MEMORY + MEMTYPE_FLASH). Exit bootloader: CMD_SWITCH_APPLICATION (0x01) + BOOTTYPE_APPLICATION (0x80).
- `I2CManager`, `DisplayManager`, `LCDManager`, `SeesawRotary`, `SlaveController`, `GPIOManager`: Singletons for hardware abstraction. Never instantiate directly - use `getInstance()`.

## Dual I2C bus architecture
- **Bus 0** (Slave, critical): GPIO5/6 @ 100kHz for ATmega328P slave (0x30). Conservative speed for reliability.
- **Bus 1** (Display, non-critical): GPIO8/9 @ 100kHz for OLED (0x3C), LCD (0x27), Seesaw (0x36). Conservative speed for reliability.
- **Wire instantiation**: `TwoWire Wire0(0)` and `TwoWire Wire1(1)` in i2c_manager.cpp. Never use global `Wire` directly.
- **Manager hierarchy**: I2CManager initializes both buses → DisplayManager/LCDManager/SeesawRotary use Bus 1 → SlaveController uses Bus 0.

## Project-specific conventions
- String flags are stored as `"true"/"false"` or `"on"/"off"`; always normalize before comparisons (see DHCP handling in [src/main.cpp](../src/main.cpp)).
- Versioning has no `v` prefix. Binaries: `fw-YYYY.M.m.p.bin` and `fs-YYYY.M.m.p.bin`. Tags: `YYYY.M.m.p`.
- Debug OFF also disables GPIO Viewer + OTA for security (see settings POST handler in [src/main.cpp](../src/main.cpp)).
- NTP date writes are wear‑limited (see `saveStoredDateIfNeeded()` in [src/settings.cpp](../src/settings.cpp)).
- Boot time saved to NVS only when debug enabled + NTP synced (uses timezone from settings at boot time).
- **I2C Bootloader Protocol**: Application at 0x30, Twiboot bootloader at 0x14 (after activation). Exit bootloader requires two-byte sequence: 0x01 + 0x80 (not single 0x00).

## Web UI patterns
- HTML templates use `%VAR%` tokens replaced in `server.on("/settings", HTTP_GET)` in [src/main.cpp](../src/main.cpp).
- API routes (`/api/update/*`) must return JSON only; frontend redirects are handled in [data/update.html](../data/update.html).
- UI follows card-based layout; see [data/style.css](../data/style.css) for CSS custom properties (`--spacing-*`, `--color-*`).
- Checkbox rows with action buttons use `form-group-checkbox-row` + `form-group-checkbox-left` for right-aligned buttons.

## Build / upload workflow (PlatformIO)
- Default env is ESP32‑S3 (XIAO). Firmware: `pio run -e esp32s3dev` or upload `pio run -e esp32s3dev -t upload`.
- LittleFS: `pio run -e esp32s3dev -t buildfs` / `-t uploadfs`. Firmware + FS should be uploaded together after UI changes.
- Safe FS upload: use `./uploadfs_with_backup.sh <ESP_IP>` to preserve config files during filesystem updates.
- **First boot sequence**: Flash erase → Serial upload firmware → Serial upload filesystem → Device starts in AP mode → Configure WiFi → Settings saved to NVS.
- **Debug levels**: Set in platformio.ini via `-D CORE_DEBUG_LEVEL=` (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE).

## Release workflow
1. Bump `FIRMWARE_VERSION` and `FILESYSTEM_VERSION` in [include/config.h](../include/config.h).
2. Build: `pio run -e esp32s3dev` + `pio run -e esp32s3dev -t buildfs`.
3. Copy binaries: `mkdir -p release/YYYY.M.m.p && cp .pio/build/esp32s3dev/{firmware,littlefs}.bin release/YYYY.M.m.p/` with proper names.
4. Update [CHANGELOG.md](../CHANGELOG.md) with Added/Changed/Fixed sections.
5. Tag (no `v`!): `git tag YYYY.M.m.p && git push origin YYYY.M.m.p`.
6. Create GitHub release via CLI: `gh release create YYYY.M.m.p release/YYYY.M.m.p/*.bin --title "vYYYY.M.m.p - Description" --notes-file release_notes.md`.

## Partitions & NVS
- Custom partition table: [partitions_xiao_s3.csv](../partitions_xiao_s3.csv) (dual OTA + LittleFS on 8MB flash).
- NVS: `config` namespace stores all settings; `ota` namespace stores update URLs and state.
- LittleFS OTA requires `U_LITTLEFS` or `U_SPIFFS` partition subtype (see `downloadAndInstallLittleFS()`).

## Feature flags (planned modularization)
- See [docs/MODULARIZATION_PLAN.md](../docs/MODULARIZATION_PLAN.md) for module structure.
- Comment out `#define FEATURE_*` in [include/config.h](../include/config.h) to disable modules (not fully implemented yet).

## Where to extend
- New setting: add `#define` in [include/config.h](../include/config.h), add member in `Settings`, wire POST handler in [src/main.cpp](../src/main.cpp), then `settings.save()`.
- New API endpoint: add route in [src/main.cpp](../src/main.cpp) (firmware) or handler in `GitHubUpdater` (OTA logic).
- New web page: add HTML in [data/](../data/), register route in `setup()`, update navigation if needed.
