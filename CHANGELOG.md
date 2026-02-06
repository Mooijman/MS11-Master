# Changelog

All notable changes to this project will be documented in this file.

## [2026.1.1.03] - 2026-02-06

### Fixed
- **LittleFS OTA update**: Use `spiffs` subtype for LittleFS partition so Update can find it

### Technical Details
- Firmware: 1,324,252 bytes (67.4% of 1.92MB OTA partition)
- Filesystem: 512KB

## [2026.1.1.02] - 2026-02-06

### Fixed
- **LittleFS OTA update**: Use LittleFS partition type to prevent "LFS Partition Could Not be Found"

### Technical Details
- Firmware: 1,324,252 bytes (67.4% of 1.92MB OTA partition)
- Filesystem: 512KB

## [2026.1.1.01] - 2026-02-06

### Changed
- **LittleFS partition**: Increased from 128KB to 512KB for enhanced data storage and logging capability
  - Web files: 116KB
  - Available for data/logging: 396KB
- **Flash usage**: Optimized to 4.45MB of 8MB total (55% utilized)
- **Updated release binaries** with larger LittleFS partition

### Technical Details
- Build time: 16.01s (clean build)
- Firmware: 1,308,108 bytes (66.5% of 1.92MB OTA partition)
- Filesystem: 512KB (previously 128KB)
- Flash layout: 1.92MB × 2 OTA + 512KB LittleFS
- Backwards compatible with previous XIAO S3 releases

## [2026.1.1.00] - 2026-02-06

### Added
- **XIAO ESP32-S3 Optimization**: Complete hardware optimization for Seeed Studio XIAO ESP32-S3
  - Board configuration: `seeed_xiao_esp32s3` with 921600 baud upload speed
  - Optimized partition table: `partitions_xiao_s3.csv` (1.92MB × 2 OTA + 512KB LittleFS)
  - GPIO pin update: I2C on GPIO6 (SDA/D5) and GPIO7 (SCL/D6)
  - Documentation: XIAO_S3_SETUP.md with complete implementation guide

### Changed
- **platformio.ini**: Board changed to `seeed_xiao_esp32s3`, upload speed to 921600
- **README.md**: Updated hardware specifications for XIAO S3 with pinout table
- **MIGRATION.md**: Added comprehensive XIAO S3 optimization section with DevKitC-1 comparison
- **Version format**: Changed to dotted format `2026.1.1.00` for better semantic versioning
- **LittleFS partition**: Increased from 128KB to 512KB for data storage and logging

### Technical Details
- Build time: 16.01s (clean build)
- Firmware: 1,308,108 bytes (66.5% of 1.92MB OTA partition)
- Filesystem: 512 KB (116 KB web files + 396 KB available for data/logging)
- RAM: 51,532 bytes (15.7%)
- Hardware: XIAO ESP32-S3 @ 240MHz, 320KB RAM, 8MB Flash
- Flash usage: 4.45 MB of 8 MB (55% utilized)
- Backwards compatible: DevKitC-1 partition backup as `partitions_esp32s3_devkit.csv`

## [2026-1.0.14] - 2026-02-06

### Changed
- **Code Cleanup**: Verwijderde overbodige code uit main.cpp en github_updater.cpp
  - Verwijderd uit main.cpp (10 regels):
    - Ongebruikte background update variabelen (`lastBackgroundCheck`, `checksToday`, `dayStartTime`)
    - Dubbele version tracking aliases (`storedFirmwareVersion`, `storedFilesystemVersion`)
  - Verwijderd uit github_updater.cpp (9 regels):
    - `remoteLittlefsVersion` uit UpdateInfo struct (ongebruikt veld)
    - `filesystemUpdateDone` uit UpdateInfo struct (legacy veld)
- **Reinstall messages**: Gebruikt nu dezelfde success messages als normale updates
  - "Reinstall Successful!" → "Firmware update successful!"
  - Groene succespagina consistent tussen install en reinstall

### Technical Details
- Firmware: 1,349,968 bytes (82.4% flash) - 264 bytes kleiner
- RAM: 53,632 bytes (16.4%)
- Totaal verwijderd: ~19 regels overbodige code
- Release binaries: `release/2026-1.0.14/`

## [2026-1.0.13] - 2026-02-06

### Added
- **Code Modularisatie - Fase 2**: GitHub update logica volledig gescheiden
  - **`include/github_updater.h` & `src/github_updater.cpp`**: API handler methods
    - `handleStatusRequest()` - Returns JSON for /api/update/status
    - `handleCheckRequest()` - Returns JSON for /api/update/check
    - `handleInstallRequest()` - Returns JSON for /api/update/install
    - `handleReinstallRequest()` - Returns JSON for /api/update/reinstall

### Changed
- **API endpoints refactored**: Main.cpp bevat nu alleen web server logica
  - `/api/update/status`: 30 → 10 regels (67% reductie)
  - `/api/update/check`: 15 → 7 regels (53% reductie)
  - `/api/update/install`: 100 → 20 regels (80% reductie)
  - `/api/update/reinstall`: 120 → 25 regels (79% reductie)
  - Totaal: ~220 regels verwijderd uit main.cpp

### Technical Details
- Firmware: 1,350,276 bytes (82.4% flash)
- RAM: 53,632 bytes (16.4%)
- Clean separation: Web server (main.cpp) vs Business logic (github_updater.cpp)
- Release binaries: `release/2026-1.0.13/`

## [2026-1.0.12] - 2026-02-05

### Fixed
- **CRITICAL**: GitHub update flow werkt nu perfect
  - Direct redirect naar success pagina na download compleet
  - Geen ERR_CONNECTION_RESET errors meer
  - Geen "Failed to fetch" errors meer

### Changed
- **Update flow optimalisatie**:
  - Redirect gebeurt bij ontvangst van JSON response met `rebootRequired:true`
  - Verwijderd: Reboot detection via connection errors (veroorzaakte 12s delay)
  - JavaScript polling verhoogd naar 500ms (was 2000ms) voor snellere updates
- **UI Styling updates**:
  - Success box: Geel → Groen (#d4edda background, #28a745 border)
  - Success tekst: Zwart → Groen (#2e7d32, bold)
  - Toegevoegd: ✅ emoji voor success message
  - Titel: "Update Successful!" → "Firmware Update" (consistent)

### Technical Details
- Firmware: 1,353,276 bytes (82.6% flash)
- Smooth user experience: red warning → green success → auto home
- Release binaries: `release/2026-1.0.12/`

## [2026-1.0.06] - 2026-02-05

### Added
- **Code Modularisatie - Fase 1**: Fundamentele refactoring voor betere onderhoudbaarheid
  - **`include/config.h`**: Centralized configuration management
    - Alle #defines, constants en configuratie parameters verzameld
    - Hardware configuratie (OLED pins, network settings)
    - Timing configuratie (timeouts, intervals)
    - API endpoints en HTTP parameters
    - Template placeholders
  - **`include/settings.h` & `src/settings.cpp`**: Settings module
    - Centralized NVS (Non-Volatile Storage) management
    - Clean API: `initialize()`, `load()`, `save()`, `syncVersions()`
    - Granular save methods: `saveNetwork()`, `saveFeatures()`
    - Helper methods: `boolToString()`, `stringToBool()`
    - Factory reset support: `reset()`, `clearWiFi()`
    - Debug printing: `print()`

### Changed
- **Main loop refactoring**: Gestructureerd en leesbaarder
  - Gesplitst in logische functies:
    - `handleDisplayTasks()` - OLED display management
    - `handleNetworkTasks()` - DNS, OTA, WiFi
    - `handleSystemTasks()` - Reboots, system operations
  - Verbeterde code organisatie en onderhoudbaarheid
- **Compatibility layer**: Bestaande code blijft werken
  - String references naar settings module
  - Minimale aanpassingen aan bestaande handlers
  - Backwards compatible met oude variabele namen

### Technical Details
- Compile succesvol: 1,333,128 bytes (67.8% flash)
- RAM usage: 53,816 bytes (16.4%)
- Alle functionaliteit behouden, geen breaking changes
- Code voorbereid voor verdere modularisatie (OTA manager, WiFi manager)

## [2026-1.0.05] - 2026-02-02

### Fixed
- **CRITICAL**: DHCP checkbox waarde vergelijking veroorzaakte ongewenste reboot bij het togglen van GitHub Updates
  - Probleem: Vergelijking tussen opgeslagen waarde ("true"/"false") en POST waarde ("on"/"off")
  - Oplossing: Beide waarden genormaliseerd naar "on"/"off" formaat voor vergelijking
  - Locatie: `src/main.cpp` lines 1027-1035

### Changed
- Release binaries voorbereid in `release/2026-1.0.05/`
  - fw-2026-1.0.05.bin (1.3MB)
  - fs-2026-1.0.05.bin (128KB)

## [2026-1.0.04] - 2026-02-02

### Changed
- Message tekst aangepast: "Powercycle ESP32 now!" → "Powercycle device now!"
  - Generiekere tekst, niet hardware-specifiek

## [2026-1.0.03] - 2026-02-02

### Changed
- **CSS Optimalisatie**: Volledige refactoring van `data/style.css`
  - Backup gemaakt: `style.css.backup` (980 lines)
  - Nieuwe versie: 993 lines met CSS custom properties
  - Geïntroduceerd: CSS variabelen voor kleuren, spacing, border radius, font sizes, transitions
  - Geconsolideerd: Button styles, form elements
  - Verwijderd: Duplicate CSS definities
  - **Geen visuele wijzigingen**: Layout blijft identiek
  - Verbeterde onderhoudbaarheid door centralisatie

## [2026-1.0.02] - 2026-02-02

### Changed
- UI tekst verbeteringen voor duidelijkheid:
  - "Enable OTA Upload" → "Enable OTA Update" (settings.html line 115)
  - "Enable Updates" → "Enable GitHub Update" (settings.html line 122)

## [2026-1.0.01] - 2026-02-02

### Added
- **Automatische versie synchronisatie**: `readCurrentVersion()` functie
  - Extraheert versie nummers uit `#define` statements (strips "fw-"/"fs-" prefix)
  - Vergelijkt met opgeslagen NVS versies
  - Update NVS automatisch bij mismatch
  - Compile-time versies zijn altijd leidend
  - Werkt bij boot en na OTA/GitHub updates
  - Locatie: `src/main.cpp` lines 163-202

### Changed
- Default settings voor nieuwe ESP32 installaties:
  - DHCP: enabled (on)
  - Debug: disabled (false)
  - GPIO Viewer: disabled (false)
  - OTA Update: disabled (false)
  - GitHub Update: disabled (false)
  - Locatie: `initializeNVS()` lines 240-277

- Version placeholders in settings.html generiek gemaakt:
  - Hard-coded versie nummers → "YYYY-M.m.p" format
  - Lines 97-98 (firmware version)
  - Lines 102-103 (filesystem version)

### Technical Details
- ESP32-D0WDQ6 revision v1.0
- Platform: Espressif 32 v53.3.11
- Arduino Framework: v3.1.1 with libs v5.3.0
- Firmware size: 1,330,952 bytes (67.7% of 4MB flash)
- RAM usage: 53,856 bytes (16.4%)
- Filesystem: LittleFS 131,072 bytes

## [2026-1.0.00] - 2026-02-02

### Changed
- Basis versie voor deze update cyclus
- Volledige flash erase uitgevoerd voor schone installatie
- Firmware en filesystem geüpload via serial port

---

## Version Naming Convention

- Firmware binaries: `fw-YYYY-M.m.p.bin`
- Filesystem binaries: `fs-YYYY-M.m.p.bin`
- Format: YYYY = jaar, M = major, m = minor, p = patch

## Update Methods

1. **Serial Upload**: Via PlatformIO (`pio run -t upload` / `pio run -t uploadfs`)
2. **OTA Upload**: Via ArduinoOTA (lokaal netwerk)
3. **GitHub Update**: Automatisch via GitHub releases (indien ingeschakeld)
