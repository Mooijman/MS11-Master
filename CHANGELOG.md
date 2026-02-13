# Changelog

All notable changes to this project will be documented in this file.

## [2026.2.13.02] - 2026-02-13

### Added
- **2ms Heartbeat LED Pulse**: Re-enabled visual feedback on MS11-control
  - Every 2 seconds, a brief 2ms LED pulse confirms the heartbeat ping is successful
  - Safe with I2CManager mutex protection (no conflicts with bootloader operations)
  - Provides immediate visual indication that MS11-control is alive and responding

### Changed
- **Startup Sequence Optimization**: MS11-control detection moved to early boot
  - Ping now happens immediately after I2C/SlaveController initialization (before WiFi, before Waacs logo)
  - 500ms detection LED pulse triggered on successful ping during early startup
  - Early detection provides faster feedback and better error visibility
- **Display Improvements**: Cleaner OLED status presentation
  - MS11-control status now shown on OLED screen (not LCD) together with "MS11 Master" text
  - Layout: "MS11 Master" → blank line → "MS11-control" → "Detected"/"Absent" (2s display)
  - LCD remains on "Starting up..." during early boot, then transitions directly to WiFi info

### Removed
- **LCD MS11 Detection Message**: No longer shows "MS11-control Detected/Absent" on LCD during startup
  - Simplified LCD startup flow: "Starting up..." → "WiFi Enabled" + IP → "Ready." + clock
  - MS11-control status only shown on OLED screen

## [2026.2.13.01] - 2026-02-13

### Fixed
- **Twiboot I2C Bootloader Integration**: Fixed bootloader entry/exit command routing
  - `/api/i2c/bootloader` and `/api/i2c/exit-bootloader` now use `I2CManager` instead of direct `Wire1` calls
  - Prevents mutex conflicts with heartbeat polling on slave bus (GPIO5/6)
  - Bootloader entry (register 0x99 + magic 0xB0) now reliably triggers EEPROM write + WDT reset
  - Exit bootloader (0x01 + 0x80) correctly sends CMD_SWITCH_APPLICATION to twiboot at 0x14
- **I2C Bus Mutex Handling**: Resolved race condition between web API commands and background heartbeat
  - All bootloader operations now properly acquire slave bus mutex before Wire1 access
  - Fixed "Arduino not responding" errors caused by concurrent I2C transactions

### Changed
- **Bootloader Protocol Documentation**: Clarified twiboot integration requirements
  - Confirmed EEPROM boot magic: bytes 510-511 = 0xB007 (little-endian: 0x07, 0xB0)
  - Documented 5-second twiboot startup delay before TWI becomes active
  - Verified fuse settings: hfuse=0xD4 (BOOTRST active, 2KB bootloader section @ 0x7800)
- **ISP Flash Procedure**: Documented correct avrdude sequence to preserve twiboot
  - Must use `-D` flag (no erase) when uploading application after initial chip erase
  - Twiboot must be reflashed after any full chip erase (`-e`)
  - Recommended sequence: chip erase → app with `-D` → twiboot with `-D`

### Technical Notes
- **Root Cause**: PlatformIO ISP upload (`pio run -t upload`) performs chip erase, wiping twiboot from boot section
- **Solution**: Either use PlatformIO bootloader target or manual avrdude with `-B10 -D` flags
- **Verification**: Bus scan diagnostics confirmed Arduino transitions 0x30 (app) ↔ 0x14 (bootloader) reliably

## [2026.2.12.03] - 2026-02-12

### Changed
- **Main Loop Optimization**: Eliminated blocking operations for smoother heartbeat and LCD blink
  - LED pulse check moved to top of `loop()` for accurate 2ms pulse timing
  - OLED display only redraws when sensor data actually changes (saves ~10-15ms per cycle)
  - Seesaw button reads throttled to 50ms intervals (was every 1ms loop)
  - NeoPixel AP-mode blink uses change detection instead of unconditional I2C writes
  - Removed excessive Serial debug output from heartbeat path (~6 println per 2s cycle)
- **Probe Manager**: New consolidated temperature measurement system
  - Multi-probe support: ADS1110 ADC, AHT10, MS11-control slave temperature
  - Per-probe calibration with NVS persistence and LittleFS sync (probe_cal.txt)
  - Configurable calibration defaults in config.h (PROBE_CAL_*_OFFSET)
- **Partition Table**: Expanded to use full 8MB flash
  - app0/app1: 3MB each (was 1.875MB), littlefs: 1.9MB (was 512KB)

### Fixed
- **Heartbeat LED jitter**: 2ms pulse was stretched to 20-200ms+ due to blocking I2C and Serial operations between pulse-on and pulse-off check
- **LCD blink irregularity**: Variable loop times (5ms-200ms) caused visible stuttering in 600/400ms blink pattern

## [2026.2.12.02] - 2026-02-12

### Changed
- **Startup Blink**: "Starting up..." now properly blinks during setup sequence
  - Added `delayWithBlink()` helper to enable LCD updates during blocking operations
  - Blink works throughout entire 7-second OLED animation phase
  - Uses 600ms on / 400ms off pattern for smooth visual feedback
- **MS11 Reconnect Logic**: Improved heartbeat and connection monitoring
  - Reduced heartbeat interval from 4s to 2s for faster reconnect detection
  - Automatic reconnection attempts every 2 seconds when MS11-control absent
  - Connection status messages now properly sequenced

### Fixed
- **Connection Lost Display**: Enhanced visual feedback for MS11-control status
  - "Connection lost!" now blinks (600ms/400ms) when MS11-control disconnects
  - Time display stops automatically when connection lost
  - "Restored" message shows for full 3 seconds on reconnection (clock blocked)
  - Automatic return to "Ready." + clock after restoration message

## [2026.2.12.01] - 2026-02-12

### Added
- **LCD Display States**: Comprehensive startup and operational display sequences
  - Startup: "MagicSmoker 11" → "Starting up..." (blinking 600ms/400ms)
  - WiFi Enabled: Shows IP address for 3 seconds
  - WiFi Manager: Shows "ESP-WIFI-MANAGER" SSID
  - MS11-control Detection: "Detected" or "Absent" status (2 seconds)
  - Ready State: "Ready." with NTP time display (DD-MM-YYYY HH:MM)
  - Time Display: Blinking colon synchronized to system clock (600ms/400ms)
- **LED Pulse Communication**: Enhanced MS11-control slave signaling
  - Detection Pulse: 500ms LED pulse when MS11-control found at startup
  - Heartbeat Pulse: 2ms LED pulse every 4 seconds during operation
  - Non-blocking pulse state machine for smooth multitasking

### Changed
- **Non-blocking Architecture**: Removed all delay() calls from display updates
  - Universal blinkState() function for asymmetric blink patterns
  - LCD updates now tracked with state variables and timestamps
  - OLED sensor display delayed until startup sequence complete
- **LED Control**: Switched to proper I2C register protocol (0x10/0x11)
  - Uses SLAVE_REG_LED_ONOFF for direct LED control
  - State machine handles pulse timing in main loop
  - Debug serial output for all LED operations

### Fixed
- **Time Display Colon Blink**: Now properly synchronized using modulo arithmetic
- **MS11 Detection Display**: Correctly shows detection status with state tracking
- **WiFi Manager Display**: Shows correct AP SSID instead of configured SSID
- **Startup Blink**: Properly stopped when entering WiFi manager mode

## [2026.2.11.02] - 2026-02-11

### Changed
- **Update UI**: Removed confirmation dialogs for streamlined OTA updates
  - Eliminated confirm() prompts before firmware/filesystem updates
  - Updates now start immediately when buttons clicked
  - Console logging retained for debugging (F12 browser console)

## [2026.2.11.01] - 2026-02-11

### Fixed
- **LCD Update Messages**: Restored "Updating..." display during OTA updates
  - Line 0: "Updating...", Line 1: Update type ("Firmware"/"Filesystem")
  - Added 50ms delays for LCD stability
  - Debug logging for troubleshooting
- **OLED Update Display**: Consistent small font (ArialMT_Plain_10) throughout updates
  - Improved vertical spacing (y=0, 15, 30)
  - Uniform layout for all update stages

## [2026.2.10.03] - 2026-02-11

### Added
- **AHT10 Temperature & Humidity Sensor**: Continuous environmental monitoring
  - I2C address 0x38 on Display Bus (GPIO8/9)
  - Real-time temperature (°C) and humidity (%) display on OLED
  - Sensor reads every 30 seconds (power optimized)
  - Device identification in I2C diagnostics scanner
  - AHT10Manager singleton with health checks and error handling

### Changed
- **OLED Display**: Now shows temperature/humidity continuously at top-left
  - Format: "22.5°C  45%" in small font (ArialMT_Plain_10)
  - Display updates every 1 second
  - Encoder counter temporarily disabled

### Fixed
- **OTA Display Blocking**: Temperature/humidity display now paused during firmware updates
  - `otaUpdateInProgress` flag prevents display interference
  - Clean "Updating FW/FS" messages without sensor data overlay
  - Flag auto-resets on errors to restore normal operation

## [2026.2.10.02] - 2026-02-10

### Added
- **LCD Real-Time Clock**: Date and time display on LCD line 1 (format: DD-MM-YYYY HH:MM)
- **Universal Blink Helper**: Reusable `getBlinkState()` function for all blink functionality
- **Blinking Clock Colon**: 2 Hz blink on time separator for visual feedback

### Removed
- **GPIO Viewer**: Fully removed from firmware, settings, UI, and build dependencies

### Changed
- **Date/Time Format**: LCD shows full 4-digit year with zero-padded day
- **Code Optimization**: Stateless blink helper replaces individual toggle variables

## [2026.2.10.01] - 2026-02-10

  ### Added
  - **OLED Encoder Counter**: Counter range 90-350 with speed-based step sizes (1/10/50).

  ### Changed
  - **Seesaw Encoder I2C**: Fully migrated to Adafruit seesaw API (no raw register fallback).
  - **Encoder Responsiveness**: Faster polling and display refresh throttling for smoother input.

  ### Fixed
  - **NeoPixel Status**: Restores status color immediately after button blink.

  ## [2026.1.1.19] - 2026-02-10

  ### Fixed
  - **Software Update Display**: OLED and LCD now display update status during firmware/filesystem downloads
    - OLED shows: "Updating FW/FS", "Please Wait...", "DO NOT POWER OFF", then "Update done" / "Rebooting..."
    - LCD shows: "Updating..." during any software update
    - Previously display remained blank during updates (all display calls were commented out)

## [2026.1.1.18] - 2026-02-10

### Added
- **GitHub Updater**: Complete firmware/filesystem update system integrated
- **LCD Display Support**: 16x2 I2C LCD manager for system status
- **Display Manager**: Unified OLED display control via SSD1306

## [2026.1.1.16] - 2026-02-09

### Fixed
- **I2C Bus Mapping**: Corrected reversed bus assignments in I2CManager
  - Bus 0 (Display): Now correctly uses Wire (GPIO8/9) for LCD/OLED/Seesaw
  - Bus 1 (Slave): Now correctly uses Wire1 (GPIO5/6) for ATmega328P controller
  - Updated I2C scanner API endpoints to reflect correct bus topology
- **I2C Diagnostics UI**: Fixed device list rendering and removed redundant bus labels

## [2026.1.1.15] - 2026-02-09

### Changed
- **LCD Display Content**: Updated display layout and formatting
  - Boot phase: `*MagicSmoker 11*` / `**Starting... **` (two asterisks, space-space format)
  - WiFi connected: Shows `[SSID]` on line 0, `[IP address]` on line 1
  - WiFi Manager mode: Shows `WiFi manager` on line 0, `ESP-WIFI-MANAGER` on line 1
  - Ready state: Shows `Ready...` on line 0, blank on line 1

## [2026.1.1.14] - 2026-02-09

### Changed
- **LCD Display Messages**: Updated text formatting for better readability
  - Boot phase: `*MagicSmoker 11*` / `.**Starting...**` (with ending asterisks)
  - WiFi connected: IP address on line 1, `Wifi OK!` on line 2
  - WiFi Manager mode: `WiFi manager` on line 1, `ESP-WIFI-MGR` on line 2

## [2026.1.1.13] - 2026-02-09

### Fixed
- **OLED Display Boot Sequence**: Fixed Waacs logo rendering on startup
  - Corrected XBM bitmap dimensions from 200x52 to 105x21 pixels
  - Repositioned logo to proper coordinates (x=11, y=16) for centered display
  - Added proper I2C_TWO bus selection for SSD1306 driver initialization
  - Boot sequence now displays: Waacs logo → MS11 Master text → WiFi logo → IP address
  - Verified with git history to match working implementation from previous releases

### Technical Details
- Fixed `DisplayManager` constructor to use correct I2C_TWO parameter for Wire1
- Updated images.h with PROGMEM attribute for optimal memory usage
- Boot display sequence timing: 3s (logo) + 2s (text) + 1s (wifi) + continuous (info)

## [2026.1.1.12] - 2026-02-09

### Changed
- **I2C Bus 0 to Standard Pins**: Migrated Slave I2C bus to XIAO ESP32-S3 standard I2C pins
  - Bus 0 (Slave): Changed from GPIO6/7 to **GPIO5 (D4) SDA + GPIO6 (D5) SCL** @ 100kHz
  - Bus 1 (Display): Remains GPIO8 (D9) SDA + GPIO9 (D10) SCL @ 100kHz
  - Both buses now run at 100kHz for optimal reliability
  - Benefits: Better hardware support, Arduino Wire library compatibility, easier debugging
  - GPIO7 (D8) now available for other purposes
  - Bus assignment preserved: Slave on Bus 0 (critical), Displays on Bus 1 (non-critical)

- **I2C Scanner Improvements**: `/api/i2c/scan` endpoint now scans both buses separately
  - Separate sections for Bus 0 (Slave) and Bus 1 (Display)
  - Shows bus info: pins, speed, device count per bus
  - Device details include bus identification
  - Register dumps work correctly for both buses

### Technical Details
- Updated: `i2c_manager.cpp`, `config.h`, all wiring documentation
- Standard I2C pins provide dedicated peripheral routing and improved reliability
- No functional changes to application logic, only pin assignments

## [2026.1.1.11] - 2026-02-07

### Fixed
- **CRITICAL: Twiboot Exit Bootloader Protocol**: Fixed exit bootloader and reset commands to use correct Twiboot protocol
  - Changed from incorrect single-byte command (0x00) to proper two-byte sequence
  - Exit bootloader: CMD_SWITCH_APPLICATION (0x01) + BOOTTYPE_APPLICATION (0x80)
  - Reset from bootloader: Same command sequence (0x01 + 0x80)
  - Both commands now work perfectly without powercycle
  - Discovered correct protocol through exhaustive GitHub repo search

### Technical Details
- Protocol source: `slaveupdate.cpp` functions `runSlaveSketch()` and `leaveSlaveBootloader()`
- Both `/api/i2c/exit-bootloader` and `/api/i2c/reset` endpoints updated
- Complete I2C bootloader workflow now functional: Enter → Exit → Reset all work

## [2026.1.1.08] - 2026-02-06

### Fixed
- **GPIO Viewer Button Alignment**: Fixed button alignment to match other action buttons
  - Wrapped checkbox and button in `form-group-checkbox-row` div
  - "Open" button now right-aligned like "Reset WiFi" and "Update" buttons
  - Consistent layout across all checkbox rows with action buttons

### Technical Details
- HTML structure updated: GPIO Viewer checkbox now uses same pattern as DHCP checkbox
- CSS classes: `form-group-checkbox-row` + `form-group-checkbox-left` for proper flexbox layout
- No visual changes to other UI elements

## [2026.1.1.07] - 2026-02-06

### Changed
- **Settings UI Polish**: Final visual refinements for production
  - Removed HR divider above "Enable Debug options" for cleaner layout
  - Optimized style.css: consolidated button styles, removed redundant properties
  - Improved CSS maintainability with better organization and comments
  - Smooth 0.5s color transitions on button hover

### Technical Details
- Firmware: 1,333,968 bytes (67.8% of 1.92MB OTA partition)
- RAM: 51,588 bytes (15.7%)
- Filesystem: 512KB
- CSS cleanup: removed duplicate border-color properties, consolidated spacing rules

## [2026.1.1.06] - 2026-02-06

### Changed
- **Settings UI Spacing Improvements**: Complete visual refinement of settings page layout
  - Reduced spacing between "Enable NTP sync" checkbox and time fields (better visual grouping)
  - Reduced spacing between "Enable GitHub Update" checkbox and URL/Token fields (tighter integration)
  - Increased HR divider spacing from 15px to 20px for better section separation
  - Added targeted CSS IDs: `#ntp-checkbox-row`, `#github-update-checkbox-row` for precise control
  - Created logical visual hierarchy: related fields grouped closely, sections clearly separated

### Technical Details
- CSS: Refined spacing using CSS custom properties (--spacing-md, --spacing-3xl)
- HTML: Added semantic IDs for targeted styling without affecting other checkboxes
- Improved maintainability: Removed conflicting CSS rules that were overriding intended spacing

## [2026.1.1.05] - 2026-02-06

### Added
- **Timezone Selection**: Dropdown menu for timezone configuration
  - 15+ timezones (UTC, CET, EST, PST, etc.)
  - Only visible when NTP sync is enabled
  - Default: UTC
  - Synced with system time when NTP is active

### Changed
- **Settings UI**: Improved layout with timezone field aligned right of NTP checkbox
  - Responsive design: timezone field appears/disappears based on NTP toggle
  - Clean form grouping with consistent styling

### Technical Details
- Firmware: 1,330,848 bytes (67.7% of 1.92MB OTA partition)
- RAM: 51,588 bytes (15.7%)
- Filesystem: 512KB
- NVS key: `timezone` persists across reboots

## [2026.1.1.04] - 2026-02-06

### Added
- **NTP Time Sync**: Network time synchronization with configurable enable/disable
  - Checkbox in settings.html (default: enabled)
  - Syncs on boot and after settings save (if enabled)
  - UTC-based with configurable NTP servers (pool.ntp.org, time.nist.gov, time.google.com)
- **Date Fallback System**: Wear-limited date storage for NTP failures
  - Stores year/month/day in NVS (max 1x per day to reduce flash wear)
  - Falls back to stored date if NTP sync fails
  - Automatic fallback clock setting with stored date at midnight

### Changed
- **Settings Module**: Extended with `ntpEnabled` field and date storage helpers
  - `getStoredDate()` - retrieves stored date from NVS
  - `saveStoredDateIfNeeded()` - wear-limited date save (once per day maximum)
- **Config**: Added NTP configuration constants (servers, timeout, GMT offset)

### Technical Details
- Firmware: 1,330,292 bytes (67.7% of 1.92MB OTA partition)
- RAM: 51,572 bytes (15.7%)
- Filesystem: 512KB
- NVS keys: `dateY`, `dateM`, `dateD`, `dateSaved` (wear tracking)

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
    _(Note: Changed to standard I2C pins GPIO5/6 in 2026.1.1.12 - see Unreleased)_
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
