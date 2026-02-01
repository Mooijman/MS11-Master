# ESP32 Baseline - WiFi Manager + OTA Pull Updates + OLED

ESP32 baseline project met WiFi configuration manager, GitHub-based OTA pull updates, LittleFS storage, OLED display en web interface.

## Features

- **Web-based WiFi configuration** met AsyncWebServer
- **Captive portal** voor automatische configuratiepagina
- **WiFi network scanning** met RSSI en encryptie indicatie
- **DHCP of Static IP** configuratie
- **Persistent storage** in LittleFS met atomic writes
- **GitHub OTA pull updates** - firmware en filesystem updates via GitHub Releases
- **Version tracking** - fw-2026-1.0.00 en fs-2026-1.0.00 format
- **Web-based update interface** met status tracking
- **GPIO Viewer** optioneel beschikbaar voor debugging
- **OLED display** support (SSD1306)
- **Settings page** voor configuratie via web interface

## Hardware

- **Board**: ESP32-D0WDQ6 (rev 1.0)
- **Flash**: 4MB
- **Partitions**: Custom OTA-enabled (app0, app1, littlefs)
- **Display**: SSD1306 OLED (optional)

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
- Firmware: `fw-2026-1.0.00`
- Filesystem: `fs-2026-1.0.00`

### GitHub Release Maken

1. Ga naar: https://github.com/Mooijman/ESP32-baseline/releases/new
2. **Voor firmware update:**
   - Tag: `fw-2026-1.0.01` (of hogere versie)
   - Upload binary als: `fw.bin`
3. **Voor filesystem update:**
   - Tag: `fs-2026-1.0.01`
   - Upload binary als: `fs.bin`

### Binaries Genereren

```bash
# Compileer project
pio run -e esp32dev

# Binaries zijn beschikbaar op:
.pio/build/esp32dev/firmware.bin  # → fw.bin
.pio/build/esp32dev/littlefs.bin  # → fs.bin
```

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
`https://api.github.com/repos/Mooijman/ESP32-baseline/releases/latest`

### ArduinoOTA (lokaal)

Voor ontwikkeling kun je ook lokale OTA gebruiken:

```bash
# Upload via IP adres
pio run -t upload --upload-port <ip-adres>

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
- Regel 1: Firmware versie (fw-2026-1.0.00)
- Regel 2: Filesystem versie (fs-2026-1.0.00)

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
Board: **esp32dev** (ESP32-D0WDQ6)  
Partitions: **custom OTA (4MB)**

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

## Web Interface

- **/** - Device info en status
- **/settings** - Configuratie (GPIO Viewer, Updates, Reboot)
- **/update** - OTA update interface met status
- **/files** - File browser voor LittleFS
- **/api/update/status** - Update status API (JSON)
- **/api/update/start** - Start update download

## Licentie

Ontwikkeld voor Waacs.

## Repository

https://github.com/Mooijman/ESP32-baseline
