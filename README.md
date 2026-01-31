# ESP32-S3 WiFi Manager + OTA + OLED

WiFi configuration manager voor ESP32-S3 (Seeed XIAO) met AsyncWebServer, LittleFS storage en ArduinoOTA voor draadloze firmware updates.

## Features

- **Web-based WiFi configuration** met AsyncWebServer
- **Captive portal** voor automatische configuratiepagina
- **WiFi network scanning** met RSSI en encryptie indicatie
- **DHCP of Static IP** configuratie
- **Persistent storage** in LittleFS (config blijft behouden na reboot)
- **Auto-retry** bij verkeerd wachtwoord (herstart in AP mode)
- **ArduinoOTA** voor draadloze firmware updates over WiFi
- **GPIO Viewer** voor debugging via web interface
- **Waacs branding** met logo

## Hardware

- **Board**: Seeed Studio XIAO ESP32S3
- **Flash**: 8MB
- **PSRAM**: 8MB

## Dependencies

Geïnstalleerd via PlatformIO:
- ESPAsyncWebServer @ 3.9.6
- AsyncTCP @ 3.4.10
- LittleFS @ 3.1.1
- DNSServer @ 3.1.1
- WiFi @ 3.1.1
- ArduinoOTA @ 3.1.1 (built-in)
- GPIOViewer @ 1.7.1

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
- Bij succesvolle verbinding: GPIO Viewer beschikbaar op `http://<ip-adres>:5555`
- ArduinoOTA actief op hostname **ESP32-Base** voor draadloze firmware updates

## OTA Updates

Na succesvolle WiFi verbinding kun je firmware updates draadloos uploaden:

### Via PlatformIO CLI
```bash
# Upload via IP adres
pio run -t upload --upload-port 172.17.1.132

# Upload via hostname
pio run -t upload --upload-port ESP32-Base.local
```

### Via Arduino IDE
1. Tools → Port → Network Ports
2. Selecteer **ESP32-Base**
3. Upload zoals normaal

**Voordelen:**
- ✅ Geen USB kabel nodig
- ✅ Remote updates mogelijk
- ✅ Sneller dan seriële upload
- ✅ Ideaal voor ingebouwde devices

## Projectstructuur

```
Base/
├── platformio.ini          # PlatformIO configuratie
├── src/
│   └── main.cpp           # Hoofd applicatie code
├── data/                   # LittleFS filesystem
│   ├── wifimanager.html   # WiFi configuratie pagina
│   ├── index.html         # Hoofdpagina (na verbinding)
│   ├── style.css          # Stylesheet
│   ├── waacs-logo.png     # Bedrijfslogo
│   └── favicon.png        # Browser icoon
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

WiFi credentials en instellingen worden opgeslagen in LittleFS:
- `/ssid.txt` - WiFi network naam
- `/pass.txt` - WiFi wachtwoord
- `/dhcp.txt` - DHCP status ("true"/"false")
- `/ip.txt` - Static IP adres (indien DHCP uit)
- `/gateway.txt` - Gateway adres (indien DHCP uit)

## AP Mode

- **SSID**: ESP-WIFI-MANAGER
- **IP**: 192.168.4.1
- **Beveiliging**: Open (geen wachtwoord)
- **DNS**: Captive portal actief

## GPIO Viewer

Na succesvolle WiFi verbinding is GPIO Viewer beschikbaar op poort 5555 voor hardware debugging.

## Serial Monitor

Baud rate: **115200**

Output toont:
- LittleFS mount status
- Geladen credentials
- WiFi verbindingsstatus
- IP adres (DHCP of Static)
- GPIO Viewer status

## Ontwikkeling

Platform: **PlatformIO**  
Framework: **Arduino ESP32 v3.1.1**  
Board: **seeed_xiao_esp32s3**

## Licentie

Ontwikkeld voor Waacs.
