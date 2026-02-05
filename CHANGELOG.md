# Changelog

All notable changes to this project will be documented in this file.

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
