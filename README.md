# XIAO ESP32-S3 Baseline - WiFi Manager + OTA Pull Updates + OLED

XIAO ESP32-S3 baseline project met WiFi configuration manager, GitHub-based OTA pull updates, LittleFS storage, OLED display en web interface.

**Status**: ✅ Geoptimaliseerd voor **XIAO ESP32-S3** (compact board, 8MB flash)  
**Current Version**: `2026.1.1.08` (GPIO Viewer button alignment fix)  
**Firmware Size**: 1.33MB (67.8% of OTA partition)  
**Last Updated**: 6 februari 2026

## Migration Notes (ESP32-WROOM → ESP32-S3)

### Automatisch aangepast
- ✅ **OLED I2C pins**: GPIO5,4 → GPIO8,9 (zie [include/config.h](include/config.h#L46-L48))
- ✅ **Partition table**: Aangepast voor S3 flash layout ([partitions_esp32s3.csv](partitions_esp32s3.csv))
- ✅ **PlatformIO default**: `esp32dev` → `esp32s3dev`
- ✅ **Build environment**: Platform & dependencies

### Handmatig te controleren
- ⚠️ **GPIO assignments**: Check je custom GPIO usage (pins zijn van layout afhankelijk)
- ⚠️ **SPI/PSRAM**: ESP32S3 heeft native SPI4 voor PSRAM - check je SPI configuraties
- ⚠️ **USB CDC**: Seriële monitor via USB-C (niet meer via UART)
- ⚠️ **JTAG debugging**: Pin layout veranderd (zie [ESP32S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf))

## Features

- **Web-based WiFi configuration** met AsyncWebServer
- **Captive portal** voor automatische configuratiepagina
- **WiFi network scanning** met RSSI en encryptie indicatie
- **DHCP of Static IP** configuratie
- **Persistent storage** in LittleFS met atomic writes
- **GitHub OTA pull updates** - firmware en filesystem updates via GitHub Releases
- **Version tracking** - fw-2026.1.1.08 en fs-2026.1.1.08 format
- **Web-based update interface** met status tracking
- **GPIO Viewer** optioneel beschikbaar voor debugging
- **OLED display** support (SSD1306)
- **Settings page** voor configuratie via web interface

## Hardware

**Target Board**: XIAO ESP32-S3 (Seeed Studio)

### Specifications
- **Chip**: ESP32-S3 Dual Core (240 MHz)
- **Flash**: 8MB (expandable via microSD)
- **SRAM**: 320KB
- **Size**: Ultra-compact (21x17.5mm)
- **USB**: USB-C @ 921600 baud
- **Partitions**: Optimized for 8MB flash ([partitions_xiao_s3.csv](partitions_xiao_s3.csv))
   - `nvs`: 0x9000 - 0xDFFF (20KB)
   - `otadata`: 0xE000 - 0xEFFF (4KB)
   - `app0` (OTA_0): 0x10000 - 0x1FFFFF (1.9MB)
   - `app1` (OTA_1): 0x200000 - 0x3EFFFF (1.9MB)
   - `littlefs`: 0x3D0000 - 0x44FFFF (512KB)
- **Display**: SSD1306 OLED I2C (GPIO6=SDA, GPIO7=SCL)

### XIAO S3 Pin Layout
| Function | GPIO | Silk Label |
|----------|------|------------|
| I2C SDA | GPIO6 | D5 |
| I2C SCL | GPIO7 | D6 |
| UART TX | GPIO43 | D7 |
| UART RX | GPIO44 | D8 |
| Power | 5V / GND | — |

## Dependencies

Geïnstalleerd via PlatformIO:
- ESPAsyncWebServer @ 3.9.6
- AsyncTCP @ 3.4.10
- LittleFS @ 3.1.1
- DNSServer @ 3.1.1
- WiFi @ 3.1.1
- HTTPClient @ 3.1.1 (voor GitHub API)
- NetworkClientSecure @ 3.1.1
- ArduinoJson @ 7.4.2
- ArduinoOTA @ 3.1.1
- GPIOViewer @ 1.7.1
- ElegantOTA @ 3.1.7
- ESP8266 and ESP32 OLED driver for SSD1306 displays @ 4.6.2

## Gebruik

### Eerste keer opstarten

1. ESP32 start in AP mode met SSID: **ESP-WIFI-MANAGER**
2. Verbind met dit netwerk (geen wachtwoord)
3. Configuratiepagina opent automatisch (of ga naar http://192.168.4.1)
4. Scan voor WiFi netwerken en selecteer je netwerk
5. Voer wachtwoord in
6. Kies DHCP of Static IP
7. Klik op Submit

### Na configuratie

- ESP32 verbindt automatisch met het geconfigureerde netwerk
- Bij fout wachtwoord: config wordt gewist en AP mode start opnieuw
- Web interface beschikbaar op `http://<ip-adres>`
- Settings pagina voor configuratie: GPIO Viewer, Updates, Reboot
- ArduinoOTA actief op hostname **ESP32-Base** voor lokale firmware updates

## OTA Pull Updates via GitHub

Het systeem kan automatisch firmware en filesystem updates downloaden van GitHub Releases.

### Versie Format

Versies volgen het format: `type-year-major.minor.patch`
- Firmware: `fw-2026.1.1.08`
- Filesystem: `fs-2026.1.1.08`

### GitHub Release Maken

1. Ga naar: https://github.com/Mooijman/MS11-Master/releases/new
2. **Maak één release per versie** (tag zonder `v` prefix):
   - Tag: `2026.1.1.08` (of hogere versie)
3. **Upload binaries** met vaste naming:
   - `fw-2026.1.1.08.bin`
   - `fs-2026.1.1.08.bin`

### Binaries Genereren (XIAO ESP32-S3)

```bash
# Compileer & upload firmware via USB-C (921600 baud)
pio run -e esp32s3dev -t upload

# Upload LittleFS filesystem
pio run -e esp32s3dev -t uploadfs

# Alleen compileren (geen upload)
pio run -e esp32s3dev

# Clean rebuild (indien cache problemen)
pio run -e esp32s3dev --target clean && pio run -e esp32s3dev

# Binaries beschikbaar op:
.pio/build/esp32s3dev/firmware.bin  # → fw-YYYY.M.m.p.bin
.pio/build/esp32s3dev/littlefs.bin  # → fs-YYYY.M.m.p.bin
```

**XIAO S3 Upload Details**:
- Auto-detection via USB-C (CDC)
- Baud rate: 921600 (optimized for XIAO)
- Partition layout: 1.9MB × 2 (dual OTA) + 512KB (LittleFS)
- Flash usage: ~68% per partition (1.33MB firmware)

⚠️ **Let op**: 
- Altijd firmware EN LittleFS samen uploaden na code changes!
- LittleFS update vereist manual power cycle (herstart reboots niet)
- First boot in AP mode voor WiFi configuratie

### Update Proces

1. Open web interface → **Updates** pagina
2. Klik op **Check for Updates**
3. Systeem checkt GitHub API voor nieuwe releases
4. Bij beschikbare update: download en installeer
5. ESP32 reboot automatisch na succesvolle update

**Status tracking:**
- IDLE - geen update actief
- CHECKING - GitHub API wordt geraadpleegd
- AVAILABLE - nieuwe versie gevonden
- DOWNLOADING - update wordt gedownload
- INSTALLING - update wordt geïnstalleerd
- SUCCESS - update succesvol
- ERROR - fout opgetreden

### Update Instellingen

Via Settings pagina:
- **Enable software updates** - schakel OTA updates in/uit
- **Open** knop - open Updates pagina (alleen als updates enabled)

Update URL wordt automatisch ingesteld op:
`https://api.github.com/repos/Mooijman/MS11-Master/releases/latest`

### ArduinoOTA (lokaal)

Voor ontwikkeling kun je ook lokale OTA gebruiken:

```bash
# Upload via IP adres
pio run -t upload --upload-port <ip-adres>

# Upload filesystem via IP adres
pio run -t uploadfs --upload-port <ip-adres>

# Upload via hostname
pio run -t upload --upload-port ESP32-Base.local
```

## Projectstructuur

```
Base+OTA+OLED+settings/
├── platformio.ini          # PlatformIO configuratie (esp32dev + partitions)
├── src/
│   ├── main.cpp           # Hoofd applicatie (1413 regels)
│   └── images.h           # OLED bitmaps
├── data/                   # LittleFS filesystem
│   ├── wifimanager.html   # WiFi configuratie pagina
│   ├── index.html         # Hoofdpagina (device info)
│   ├── settings.html      # Settings (GPIO Viewer, Updates, Reboot)
│   ├── update.html        # OTA update interface
│   ├── files.html         # File browser
│   ├── confirmation.html  # Bevestigingspagina
│   ├── style.css          # Stylesheet
│   ├── waacs-logo.png     # Logo
│   ├── favicon.png        # Browser icoon
│   ├── hex100.png         # Waacs hex logo
│   ├── current.info       # Versie info (fw/fs)
│   └── global.conf        # Configuratie (updates, gpioviewer, ota)
├── partitions_4mb.csv     # Partitie tabel ESP32 (4MB)
├── partitions_8mb.csv     # Partitie tabel ESP32-S3 (8MB - toekomst)
├── include/
├── lib/
└── test/
```

## Upload

### Code uploaden
```bash
pio run --target upload
```

### Filesystem uploaden (HTML/CSS/images)
```bash
pio run --target uploadfs
```

### Config wissen (voor opnieuw testen)
```bash
pio run --target uploadfs
```

## Configuratie opslag

Configuratie wordt opgeslagen in LittleFS met atomic writes:

**global.conf** (hoofdconfiguratie):
- `ssid=` - WiFi network naam
- `ip=` - Static IP adres
- `gateway=` - Gateway adres
- `dhcp=` - DHCP status (on/off)
- `gpioviewer=` - GPIO Viewer status (on/off)
- `ota=` - ArduinoOTA status (on/off)
- `updates=` - Software updates status (on/off)
- `updateUrl=` - GitHub API URL voor releases

**current.info** (versie tracking):
- Regel 1: Firmware versie (fw-2026.1.1.08)
- Regel 2: Filesystem versie (fs-2026.1.1.08)

**NVS storage** (OTA state):
- Update info, download URLs, last check timestamp

## AP Mode

- **SSID**: ESP-WIFI-MANAGER
- **IP**: 192.168.4.1
- **Beveiliging**: Open (geen wachtwoord)
- **DNS**: Captive portal actief

## GPIO Viewer

Optionele debugging tool:
- Standaard **uitgeschakeld** (performance redenen)
- Inschakelen via Settings pagina
- Beschikbaar op `http://<ip-adres>:5555`
- Live GPIO pin monitoring
- Kan GPIO read errors veroorzaken bij debug level 5

## Serial Monitor

Baud rate: **115200**  
Debug Level: **WARN (2)** - voor productie performance

Output toont:
- LittleFS mount status (partition: "littlefs")
- Geladen configuratie (global.conf)
- WiFi verbindingsstatus
- IP adres (DHCP of Static)
- GPIO Viewer status (indien enabled)
- OTA update checks en status
- Versie informatie (fw/fs)

## Ontwikkeling

Platform: **PlatformIO**  
Framework: **Arduino ESP32 v3.1.1**  
Board: **seeed_xiao_esp32s3** (XIAO ESP32-S3)  
Partitions: **partitions_xiao_s3.csv** (1.92MB × 2 OTA + 512KB LittleFS)

### Debug Levels

In platformio.ini:
- **Level 2 (WARN)** - productie (aanbevolen)
- **Level 3 (INFO)** - normale debugging
- **Level 5 (VERBOSE)** - intensive (performance impact!)

### Performance Tips

- Debug level 2 voor normale werking
- GPIO Viewer alleen als debugging nodig
- Debug level 5 + GPIO Viewer = zeer traag systeem
- LittleFS atomic writes voorkomen corruptie

## Recent Updates (February 2026)

### v2026.1.1.08 - GPIO Viewer Button Alignment Fix
- GPIO Viewer "Open" button aligned with other action buttons
- Consistent layout using `form-group-checkbox-row`

### v2026.1.1.07 - Settings UI Polish
- CSS optimization and spacing refinements
- Consistent button hover transitions

### v2026.1.1.05 - Timezone Selection
- Timezone dropdown menu in settings.html
- 15+ timezones (UTC, CET, EST, PST, etc.)
- Only visible when NTP sync is enabled
- Synced with system time when NTP is active

### v2026.1.1.04 - NTP Time Synchronization
- Network time sync with configurable enable/disable
- Fallback to stored date if NTP sync fails
- Wear-limited storage: max 1x per day
- Three NTP servers: pool.ntp.org, time.nist.gov, time.google.com
- Automatic sync on boot and after settings save

## Web Interface

- **/** - Device info en status
- **/settings** - Configuratie (NTP sync, Timezone, GPIO Viewer, Updates, Reboot)
- **/update** - OTA update interface met status
- **/files** - File browser voor LittleFS
- **/api/update/status** - Update status API (JSON)
- **/api/update/start** - Start update download

## Documentation

- **[.github/copilot-instructions.md](.github/copilot-instructions.md)** - AI Agent guidance (architecture, modules, conventions)
- **[SESSION_SUMMARY.md](SESSION_SUMMARY.md)** - Session documentation (February 6, 2026)
- **[DECISIONS.md](DECISIONS.md)** - Technical decisions and workflows
- **[CHANGELOG.md](CHANGELOG.md)** - Complete release history
- **[docs/OTA_Testing_Checklist.md](docs/OTA_Testing_Checklist.md)** - OTA update testing guide
- **[XIAO_S3_SETUP.md](XIAO_S3_SETUP.md)** - XIAO ESP32-S3 specific setup guide

## Licentie

Ontwikkeld voor Waacs.

## Repository


https://github.com/Mooijman/ESP32-baseline
