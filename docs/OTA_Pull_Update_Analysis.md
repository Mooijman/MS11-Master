# Gedistribueerde Pull Firmware Update - Implementatieanalyse

## 1. Huidige Situatie

### Hardware & Software
- **ESP32-S3** (Seeed XIAO ESP32-S3)
- **Flash**: 8MB (custom partition table)
- **Framework**: Arduino ESP32 v3.1.1
- **Board**: seeed_xiao_esp32s3
- **LittleFS**: Actief voor data storage

### Versie Naming Convention
- **Firmware**: `fw-YYYY.M.m.pp` (bijv. fw-2026.1.1.08)
- **Filesystem**: `fs-YYYY.M.m.pp` (bijv. fs-2026.1.1.08)
- **Opslag**: Compile-time defines in `include/config.h` + NVS tracking

### Bestaande Functionaliteit
- ArduinoOTA actief voor lokale OTA updates
- ElegantOTA library aanwezig maar niet gebruikt (versie 3.1.5)
- LittleFS filesystem met HTML/CSS bestanden
- Update pagina (update.html) aanwezig maar zonder backend
- Geen versie tracking in firmware
- Geen HTTPClient of WiFiClientSecure includes

### Bestaande Endpoints
```cpp
GET  /                    -> index.html
GET  /settings            -> settings.html met template
POST /settings            -> Save settings
GET  /files               -> files.html
GET  /update              -> update.html
GET  /scan                -> WiFi scan JSON
```

---

## 2. Benodigde Dependencies

### 2.1 Te Installeren Libraries
```ini
[env:esp32dev]
lib_deps = 
  ayushsharma82/ElegantOTA@^3.1.5     # Al aanwezig
  bblanchon/ArduinoJson@^7.2.1         # Al aanwezig
  + geen extra libraries nodig - HTTPClient en WiFiClientSecure zijn core
```

### 2.2 Benodigde Core Includes
```cpp
#include <HTTPClient.h>
#include <WiFiClientSecure.h>  // Voor HTTPS
#include <Update.h>            // Voor OTA functionaliteit
#include <esp_ota_ops.h>       // Voor partition info
#include <Preferences.h>       // Voor NVS storage
#include <ArduinoJson.h>       // Voor JSON parsing
```

---

## 3. Partition Layout

### 3.1 Huidige Situatie
Standaard ESP32 4MB partition table (geen custom):
```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, 0x140000  (1.25MB)
app1,     app,  ota_1,   0x150000,0x140000  (1.25MB)
spiffs,   data, spiffs,  0x290000,0x160000  (1.375MB)
```

### 3.2 Custom Partition Table Nodig
**JA** - Voor betere ruimte-indeling en LittleFS ondersteuning

**Aanbevolen voor ESP32 (4MB flash):**
```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, 0x1E0000  (1.875MB)
app1,     app,  ota_1,   0x1F0000,0x1E0000  (1.875MB)
littlefs, data, littlefs,0x3D0000,0x20000   (128KB)
```

**Aanbevolen voor ESP32-S3 (8MB flash) - toekomst:**
```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, 0x3D0000  (3.8MB)
app1,     app,  ota_1,   0x3E0000,0x3D0000  (3.8MB)
littlefs, data, littlefs,0x7B0000,0x40000   (256KB)
```

**Implementatie:**
1. Maak bestand: `partitions_esp32.csv` in root
2. Maak bestand: `partitions_esp32s3.csv` in root (voor later)
3. Update `platformio.ini`:
```ini
[env:esp32dev]
board_build.partitions = partitions_esp32.csv
board_build.filesystem = littlefs

[env:esp32s3dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.11/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
board_build.partitions = partitions_esp32s3.csv
board_build.filesystem = littlefs
upload_speed = 460800
monitor_speed = 115200
```

---

## 4. Firmware Versie Tracking

### 4.1 Versie Definities (VERPLICHT FORMAT)
```cpp
// Toevoegen aan include/config.h
// Format: "type-year-major.minor.patch"
#define FIRMWARE_VERSION "fw-2026.1.1.08"
#define FILESYSTEM_VERSION "fs-2026.1.1.08"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// Globale variabelen
String currentFirmwareVersion = FIRMWARE_VERSION;
String currentFilesystemVersion = FILESYSTEM_VERSION;
```

### 4.2 current.info Bestand
Maak bestand in `data/current.info`:
```
firmware=fw-2026.1.1.08
filesystem=fs-2026.1.1.08
build_date=2026-02-01
```

Dit bestand wordt automatisch mee-geüpload met LittleFS en dient als reference.

**Genereer automatisch via Python script** (optioneel):
```python
# scripts/generate_version_info.py
import datetime

with open('data/current.info', 'w') as f:
    f.write(f'firmware=fw-2026.1.1.08\n')
    f.write(f'filesystem=fs-2026.1.1.08\n')
    f.write(f'build_date={datetime.date.today()}\n')
```

Aanroepen in `platformio.ini`:
```ini
extra_scripts = pre:scripts/generate_version_info.py
```

### 4.3 Compile-time Version Injection (NIET GEBRUIKEN)
❌ **NIET gebruiken** - versies moeten in main.cpp staan, niet als build flags

---

## 5. GitHub Releases Setup

### 5.1 Repository Voorbereiding
1. **Public repository** (of token strategie)
2. **GitHub Actions workflow** voor automated releases
3. **Release assets**:
   - `firmware.bin` - compileer output van `pio run`
   - `littlefs.bin` - genereer met `pio run --target buildfs`

### 5.2 GitHub Actions Workflow Voorbeeld
```yaml
# .github/workflows/release.yml
name: Build and Release

on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.x'
      
      - name: Install PlatformIO
        run: pip install platformio
      
      - name: Build Firmware
        run: pio run
      
      - name: Build LittleFS
        run: pio run --target buildfs
      
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            .pio/build/esp32dev/firmware.bin
            .pio/build/esp32dev/littlefs.bin
```

### 5.3 GitHub API Endpoint
```
GET https://api.github.com/repos/{OWNER}/{REPO}/releases/latest
```

**Response structuur:**
```json
{
  "tag_name": "v1.4.2",
  "assets": [
    {
      "name": "firmware.bin",
      "browser_download_url": "https://github.com/.../firmware.bin"
    },
    {
      "name": "littlefs.bin",
      "browser_download_url": "https://github.com/.../littlefs.bin"
    }
  ]
}
```

---

## 6. Benodigde Code Componenten

### 6.1 Update State Management
```cpp
// Update status tracking
enum UpdateState {
  UPDATE_IDLE,
  UPDATE_CHECKING,
  UPDATE_AVAILABLE,
  UPDATE_DOWNLOADING,
  UPDATE_INSTALLING,
  UPDATE_SUCCESS,
  UPDATE_ERROR
};

struct UpdateInfo {
  UpdateState state;
  String remoteVersion;
  String remoteLittlefsVersion;
  String firmwareUrl;
  String littlefsUrl;
  bool firmwareAvailable;
  bool littlefsAvailable;
  unsigned long lastCheck;
  String lastError;
  int downloadProgress;
};

UpdateInfo updateInfo;
Preferences preferences;  // Voor NVS storage
```

### 6.2 GitHub API Client
```cpp
// Check for updates via GitHub API
bool checkGitHubRelease() {
  HTTPClient http;
  WiFiClientSecure client;
  
  client.setInsecure();  // Of gebruik root CA certificate
  
  String url = "https://api.github.com/repos/OWNER/REPO/releases/latest";
  http.begin(client, url);
  http.addHeader("User-Agent", "ESP32-OTA-Client");
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    // Parse JSON met ArduinoJson
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, payload);
    
    String remoteVersion = doc["tag_name"].as<String>();
    JsonArray assets = doc["assets"];
    
    // Extract URLs
    for (JsonObject asset : assets) {
      String name = asset["name"].as<String>();
      if (name == "firmware.bin") {
        updateInfo.firmwareUrl = asset["browser_download_url"].as<String>();
      } else if (name == "littlefs.bin") {
        updateInfo.littlefsUrl = asset["browser_download_url"].as<String>();
      }
    }
    
    // Compare versions
    updateInfo.remoteVersion = remoteVersion;
    updateInfo.firmwareAvailable = compareVersions(remoteVersion, currentFirmwareVersion);
    
    return true;
  }
  
  return false;
}
```

### 6.3 Firmware Download & Install
```cpp
bool downloadAndInstallFirmware(String url) {
  HTTPClient http;
  WiFiClientSecure client;
  
  client.setInsecure();
  http.begin(client, url);
  
  int httpCode = http.GET();
  if (httpCode != 200) return false;
  
  int contentLength = http.getSize();
  if (contentLength <= 0) return false;
  
  // Start OTA update
  if (!Update.begin(contentLength, U_FLASH)) {
    return false;
  }
  
  // Download en schrijf naar flash
  WiFiClient * stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  
  if (written != contentLength) {
    Update.abort();
    return false;
  }
  
  if (!Update.end()) {
    return false;
  }
  
  return Update.isFinished();
}

bool downloadAndInstallLittleFS(String url) {
  // Vergelijkbaar maar met U_LITTLEFS flag
  if (!Update.begin(contentLength, U_LITTLEFS)) {
    return false;
  }
  // ... rest identiek
}
```

### 6.4 Background Check Task
```cpp
// Timer voor periodieke checks
unsigned long lastUpdateCheck = 0;
const unsigned long UPDATE_CHECK_INTERVAL = 6 * 60 * 60 * 1000;  // 6 uur

void loop() {
  // ... bestaande code
  
  // Periodieke update check
  if (millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    if (updatesEnabled == "on") {
      checkForUpdates();
    }
    lastUpdateCheck = millis();
  }
}

// Boot check met random delay
void setup() {
  // ... bestaande setup
  
  // Random delay 0-10 minuten
  if (updatesEnabled == "on") {
    unsigned long delay = random(0, 10 * 60 * 1000);
    Serial.printf("First update check in %lu seconds\n", delay/1000);
    lastUpdateCheck = millis() - UPDATE_CHECK_INTERVAL + delay;
  }
}
```

---

## 7. API Endpoints voor UI

### 7.1 Status Endpoint
```cpp
server.on("/api/update/status", HTTP_GET, [](AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(512);
  
  doc["currentFirmwareVersion"] = currentFirmwareVersion;
  doc["currentLittlefsVersion"] = currentLittlefsVersion;
  doc["remoteVersion"] = updateInfo.remoteVersion;
  doc["state"] = updateInfo.state;
  doc["firmwareAvailable"] = updateInfo.firmwareAvailable;
  doc["littlefsAvailable"] = updateInfo.littlefsAvailable;
  doc["lastCheck"] = updateInfo.lastCheck;
  doc["lastError"] = updateInfo.lastError;
  doc["downloadProgress"] = updateInfo.downloadProgress;
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
});
```

### 7.2 Check Endpoint
```cpp
server.on("/api/update/check", HTTP_POST, [](AsyncWebServerRequest *request) {
  updateInfo.state = UPDATE_CHECKING;
  bool success = checkGitHubRelease();
  
  DynamicJsonDocument doc(256);
  doc["success"] = success;
  doc["message"] = success ? "Check completed" : "Check failed";
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
});
```

### 7.3 Install Endpoint
```cpp
server.on("/api/update/install", HTTP_POST, [](AsyncWebServerRequest *request) {
  String type = "both";  // firmware, littlefs, of both
  
  if (request->hasParam("type", true)) {
    type = request->getParam("type", true)->value();
  }
  
  bool success = false;
  updateInfo.state = UPDATE_DOWNLOADING;
  
  if (type == "firmware" || type == "both") {
    success = downloadAndInstallFirmware(updateInfo.firmwareUrl);
  }
  
  if (type == "littlefs" || type == "both") {
    success = downloadAndInstallLittleFS(updateInfo.littlefsUrl);
  }
  
  DynamicJsonDocument doc(256);
  doc["success"] = success;
  doc["rebootRequired"] = success;
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
  
  if (success) {
    delay(2000);
    ESP.restart();
  }
});
```

---

## 8. Update HTML UI

### 8.1 Huidige Status Display
```html
<div class="update-status">
  <h3>Huidige Versie</h3>
  <p>Firmware: <span id="current-firmware">-</span></p>
  <p>LittleFS: <span id="current-littlefs">-</span></p>
  
  <h3>Beschikbare Update</h3>
  <p>Versie: <span id="remote-version">-</span></p>
  <p>Status: <span id="update-status">Idle</span></p>
  <p>Laatst gecontroleerd: <span id="last-check">-</span></p>
</div>

<div id="update-actions" style="display:none;">
  <button onclick="installUpdate('firmware')">Installeer Firmware</button>
  <button onclick="installUpdate('littlefs')">Installeer LittleFS</button>
  <button onclick="installUpdate('both')">Installeer Alles</button>
</div>

<button onclick="checkForUpdate()">Check Nu</button>

<div id="progress" style="display:none;">
  <div class="progress-bar">
    <div id="progress-fill" style="width:0%"></div>
  </div>
  <p id="progress-text">0%</p>
</div>
```

### 8.2 JavaScript Polling
```javascript
let pollInterval;

function updateStatus() {
  fetch('/api/update/status')
    .then(response => response.json())
    .then(data => {
      document.getElementById('current-firmware').textContent = data.currentFirmwareVersion;
      document.getElementById('remote-version').textContent = data.remoteVersion;
      document.getElementById('update-status').textContent = data.state;
      
      if (data.firmwareAvailable || data.littlefsAvailable) {
        document.getElementById('update-actions').style.display = 'block';
      }
      
      if (data.state === 'DOWNLOADING' || data.state === 'INSTALLING') {
        document.getElementById('progress').style.display = 'block';
        document.getElementById('progress-fill').style.width = data.downloadProgress + '%';
        startPolling();
      } else {
        stopPolling();
      }
    });
}

function startPolling() {
  if (!pollInterval) {
    pollInterval = setInterval(updateStatus, 2000);  // Poll elke 2 seconden
  }
}

function stopPolling() {
  if (pollInterval) {
    clearInterval(pollInterval);
    pollInterval = null;
  }
}

function checkForUpdate() {
  fetch('/api/update/check', { method: 'POST' })
    .then(() => setTimeout(updateStatus, 1000));
}

function installUpdate(type) {
  if (!confirm('Device zal herstarten na update. Doorgaan?')) return;
  
  fetch('/api/update/install', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'type=' + type
  })
  .then(response => response.json())
  .then(data => {
    if (data.rebootRequired) {
      alert('Update succesvol! Device herstart...');
      startPolling();
    }
  });
}

// Initial load en refresh elke 60 seconden
updateStatus();
setInterval(updateStatus, 60000);
```

---

## 9. NVS Storage voor Persistentie

```cpp
void saveUpdateInfo() {
  preferences.begin("update", false);
  preferences.putString("remoteVer", updateInfo.remoteVersion);
  preferences.putString("fwUrl", updateInfo.firmwareUrl);
  preferences.putString("fsUrl", updateInfo.littlefsUrl);
  preferences.putULong("lastCheck", updateInfo.lastCheck);
  preferences.putString("lastError", updateInfo.lastError);
  preferences.end();
}

void loadUpdateInfo() {
  preferences.begin("update", true);
  updateInfo.remoteVersion = preferences.getString("remoteVer", "");
  updateInfo.firmwareUrl = preferences.getString("fwUrl", "");
  updateInfo.littlefsUrl = preferences.getString("fsUrl", "");
  updateInfo.lastCheck = preferences.getULong("lastCheck", 0);
  updateInfo.lastError = preferences.getString("lastError", "");
  preferences.end();
}
```

---

## 10. Beveiliging & Validatie

### 10.1 HTTPS Certificate Pinning
```cpp
// GitHub's root CA certificate
const char* github_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
// ... rest van certificate
"-----END CERTIFICATE-----\n";

// In code:
client.setCACert(github_root_ca);
```

### 10.2 Firmware Hash Verification (optioneel)
```cpp
// Include SHA256 hash in release notes of GitHub
// Download firmware
// Calculate SHA256
// Compare met expected hash
// Alleen installeren als match
```

### 10.3 Rate Limiting
```cpp
const int MAX_CHECKS_PER_DAY = 4;
const unsigned long MIN_CHECK_INTERVAL = 6 * 60 * 60 * 1000;  // 6 uur

int checksToday = 0;
unsigned long lastCheckDay = 0;

bool canCheckUpdate() {
  unsigned long currentDay = millis() / (24 * 60 * 60 * 1000);
  
  if (currentDay != lastCheckDay) {
    checksToday = 0;
    lastCheckDay = currentDay;
  }
  
  if (checksToday >= MAX_CHECKS_PER_DAY) {
    return false;
  }
  
  if (millis() - lastUpdateCheck < MIN_CHECK_INTERVAL) {
    return false;
  }
  
  return true;
}
```

---

## 11. Error Handling

### 11.1 Download Failures
```cpp
if (!downloadAndInstallFirmware(url)) {
  updateInfo.state = UPDATE_ERROR;
  updateInfo.lastError = Update.errorString();
  saveUpdateInfo();
  
  // Huidige firmware blijft actief
  Serial.println("Update failed, staying on current firmware");
  return;
}
```

### 11.2 Rollback Mechanisme
```cpp
// Na reboot - check if new firmware works
void setup() {
  // ... existing setup
  
  const esp_partition_t* partition = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  
  if (esp_ota_get_state_partition(partition, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      // Nieuwe firmware draait - mark als valid
      if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        Serial.println("New firmware validated");
      }
    }
  }
}

// Bij kritieke fout - rollback
void criticalError() {
  esp_ota_mark_app_invalid_rollback_and_reboot();
}
```

---

## 12. Implementatie Checklist

### Phase 1: Foundation
- [ ] Custom partition table aanmaken en testen
- [ ] Firmware versie constanten toevoegen
- [ ] HTTPClient en WiFiClientSecure includes
- [ ] UpdateInfo struct en globale variabelen
- [ ] Preferences voor NVS storage

### Phase 2: GitHub Integration
- [ ] GitHub API client functie
- [ ] JSON parsing met ArduinoJson
- [ ] Version comparison logic
- [ ] URL extraction van assets

### Phase 3: Update Mechanisme
- [ ] Download firmware functie
- [ ] Download LittleFS functie
- [ ] Progress tracking
- [ ] Error handling
- [ ] Rollback mechanisme

### Phase 4: API Endpoints
- [ ] GET /api/update/status
- [ ] POST /api/update/check
- [ ] POST /api/update/install

### Phase 5: UI Integration
- [ ] update.html uitbreiden
- [ ] JavaScript voor polling
- [ ] Progress bar
- [ ] Status display

### Phase 6: Background Tasks
- [ ] Periodieke check implementeren
- [ ] Boot check met random delay
- [ ] Rate limiting
- [ ] NVS persistentie

### Phase 7: Testing & Security
- [ ] HTTPS certificate pinning
- [ ] Hash verification (optioneel)
- [ ] Timeout configuratie
- [ ] Error scenarios testen
- [ ] Rollback testen

---

## 13. GitHub Repository Vereisten

### 13.1 Repository Structure
```
Mooijman/ESP32-SettingsPage/
├── .github/
│   └── workflows/
│       └── release.yml          # Automated build & release
├── src/
│   └── main.cpp
├── data/
│   └── *.html
├── partitions.csv               # Custom partition table
├── platformio.ini
└── README.md
```

### 13.2 Release Workflow
1. Developer maakt changes
2. Commit & push naar main
3. Tag maken: `git tag v1.0.1 && git push origin v1.0.1`
4. GitHub Action triggered:
   - Build firmware.bin
   - Build littlefs.bin  
   - Create release met assets
5. ESP32 devices detecteren nieuwe versie
6. User kan installeren via UI

---

## 14. Geschatte Tijdsinvestering

- **Phase 1**: 2-3 uur (partition table, basis structuur)
- **Phase 2**: 3-4 uur (GitHub API integratie)
- **Phase 3**: 4-6 uur (update mechanisme, testing)
- **Phase 4**: 2-3 uur (API endpoints)
- **Phase 5**: 3-4 uur (UI updates)
- **Phase 6**: 2-3 uur (background tasks)
- **Phase 7**: 4-6 uur (security, testing, debugging)

**Totaal: 20-29 uur** (spreiding over meerdere dagen aanbevolen)

---

## 15. Risico's & Mitigaties

### 15.1 Risico: Brick tijdens update
**Mitigatie**: 
- OTA mechanisme zorgt voor fallback naar vorige versie
- Test altijd eerst met lokale updates
- Implementeer health check na boot

### 15.2 Risico: Filesystem corruptie
**Mitigatie**:
- LittleFS update apart van firmware
- Backup mechanisme voor kritieke config
- Validatie voor en na update

### 15.3 Risico: GitHub rate limiting
**Mitigatie**:
- Max 4 checks per dag per device
- Exponential backoff bij errors
- Cache laatste check resultaat

### 15.4 Risico: Netwerk failures tijdens download
**Mitigatie**:
- Timeout configuratie (30s connect, 60s read)
- Retry mechanisme met max 3 pogingen
- Progress tracking voor resume (optioneel)

---

## 16. Alternatieven & Optimalisaties

### 16.1 Alternatief: ElegantOTA Library
De ElegantOTA library is al aanwezig en biedt:
- Web-based upload interface
- Geen GitHub integratie out-of-box
- Eenvoudiger maar minder automatisch

**Voordeel custom implementatie:**
- Volledige controle over update proces
- GitHub Releases integratie
- Batch updates naar meerdere devices
- Scheduled releases

### 16.2 Optimalisatie: Delta Updates
Voor grote firmware:
- Alleen differences downloaden
- Complexer maar bespaart bandwidth
- Library: `esp-delta-ota` (experimenteel)

### 16.3 Optimalisatie: Compressed Assets
```cpp
// Download .gz bestanden
// Update.writeStream() ondersteunt gzip automatisch
```

---

## 17. Rollback Strategie - BELANGRIJK!

### 17.1 Pre-Implementation Safety

**VOOR JE BEGINT - Maak een veilig herstelpunt:**

```bash
# 1. Commit alle huidige changes
cd /Users/jeroen/Documents/PlatformIO/Projects/Base+OTA+OLED+settings
git add .
git commit -m "Pre-OTA implementation snapshot"

# 2. Maak een safety tag
git tag -a v1.0.0-pre-ota -m "Stable version before OTA pull updates implementation"

# 3. Push naar GitHub
git push origin main
git push origin v1.0.0-pre-ota

# 4. Maak een development branch (optioneel maar aanbevolen)
git checkout -b feature/ota-pull-updates
```

### 17.2 Rollback Procedures

#### Option A: Complete Rollback (alles terugdraaien)
```bash
# Terug naar de tagged versie
git checkout v1.0.0-pre-ota

# Maak een nieuwe branch vanaf deze tag
git checkout -b rollback-to-stable

# Upload oude firmware en filesystem
pio run --target upload
pio run --target uploadfs
```

#### Option B: Selective Rollback (enkele bestanden)
```bash
# Herstel specifieke bestanden
git checkout v1.0.0-pre-ota -- src/main.cpp
git checkout v1.0.0-pre-ota -- data/update.html
git checkout v1.0.0-pre-ota -- platformio.ini

# Commit de rollback
git commit -m "Rollback specific files to pre-OTA state"
```

#### Option C: Hard Reset (VOORZICHTIG!)
```bash
# Hard reset naar tag (verliest uncommitted changes!)
git reset --hard v1.0.0-pre-ota
git push origin main --force  # Alleen als zeker!
```

### 17.3 Testing Strategy

**Test ALTIJD eerst op development branch:**

```bash
# Ontwikkel op feature branch
git checkout -b feature/ota-pull-updates

# Maak changes, test
# ... implementatie ...

# Als alles werkt: merge naar main
git checkout main
git merge feature/ota-pull-updates

# Als het NIET werkt: verwijder branch
git checkout main
git branch -D feature/ota-pull-updates
```

### 17.4 Emergency Recovery

**Als ESP32 niet meer boot na update:**

```bash
# 1. Erase complete flash
esptool.py --port /dev/cu.usbserial-0001 erase_flash

# 2. Upload partition table
pio run --target uploadfs

# 3. Upload firmware
pio run --target upload

# 4. Als dat niet werkt - upload bootloader + partitions handmatig
esptool.py --port /dev/cu.usbserial-0001 --chip esp32 \
  write_flash 0x1000 .pio/build/esp32dev/bootloader.bin \
  0x8000 .pio/build/esp32dev/partitions.bin \
  0x10000 .pio/build/esp32dev/firmware.bin
```

### 17.5 Backup Bestanden

**Maak backups van kritieke bestanden:**

```bash
# Maak backup directory
mkdir -p backups/pre-ota-$(date +%Y%m%d)

# Copy kritieke bestanden
cp src/main.cpp backups/pre-ota-$(date +%Y%m%d)/
cp platformio.ini backups/pre-ota-$(date +%Y%m%d)/
cp -r data/ backups/pre-ota-$(date +%Y%m%d)/

# Maak een complete tar backup
tar -czf backups/complete-backup-$(date +%Y%m%d).tar.gz \
  src/ data/ include/ platformio.ini
```

### 17.6 Git Tag Overview

**Aanbevolen tag strategie:**

```bash
v1.0.0-pre-ota       # Voor OTA implementatie
v1.1.0-ota-alpha     # Eerste OTA versie (test)
v1.1.0-ota-beta      # Beta versie (uitgebreid getest)
v1.1.0               # Stable release met OTA
```

**Check alle tags:**
```bash
git tag -l -n1  # Lijst alle tags met messages
```

### 17.7 Current State Protection

**HUIDIGE STATUS (1 feb 2026):**
```bash
Laatste commit: 7b869dc "Add software updates feature..."
Uncommitted files:
  - data/confirmation.html (M)
  - data/files.html (M)
  - data/index.html (M)
  - data/style.css (M)
  - data/wifimanager.html (M)
  - data/update.html (??)
  - docs/ (??)
```

**ACTIE VEREIST VOOR START:**
```bash
# Commit huidige wijzigingen
git add data/update.html docs/
git commit -m "Add update.html and OTA analysis documentation"

# Check of er nog modified files zijn
git status

# Als ja: commit ook die
git add data/confirmation.html data/files.html data/index.html \
        data/style.css data/wifimanager.html
git commit -m "Update HTML pages with final styling"

# Maak safety tag
git tag -a v1.0.0-pre-ota -m "Stable: Before OTA pull updates"
git push origin main --tags
```

---

## 18. ESP32 vs ESP32-S3 Compatibility

### 18.1 Code Compatibility

**Identiek voor beide:**
- Arduino framework code
- WiFi libraries
- LittleFS
- Update library
- HTTPClient
- ArduinoJSON

**Verschillen:**
- Partition sizes (zie sectie 3.2)
- GPIO pinout (OLED display pins kunnen anders zijn)
- PSRAM gebruik (S3 heeft meer)

### 18.2 Migration Checklist

**Bij overstap naar ESP32-S3:**

```ini
# platformio.ini aanpassen
[env:esp32s3dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.11/platform-espressif32.zip
board = esp32-s3-devkitc-1  # Of jouw specifieke S3 board
framework = arduino
board_build.partitions = partitions_esp32s3.csv  # Grotere partities
board_build.filesystem = littlefs
```

**Check GPIO pins:**
```cpp
// ESP32: SDA=5, SCL=4
// ESP32-S3: Controleer jouw board pinout!
// Mogelijk: SDA=8, SCL=9 of andere
SSD1306 display(0x3c, SDA_PIN, SCL_PIN);
```

**Test procedure:**
1. Upload firmware naar S3
2. Controleer serial output
3. Test WiFi connectie
4. Test OLED display
5. Test LittleFS
6. Test OTA functionaliteit

### 18.3 GitHub Releases voor Multiple Boards

**Release strategie:**
```yaml
# .github/workflows/release.yml
- name: Build ESP32
  run: pio run -e esp32dev
  
- name: Build ESP32-S3
  run: pio run -e esp32s3dev
  
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    files: |
      .pio/build/esp32dev/firmware.bin
      .pio/build/esp32dev/littlefs.bin
      .pio/build/esp32s3dev/firmware.bin
      .pio/build/esp32s3dev/littlefs.bin
```

**Naamgeving:**
- `firmware-esp32.bin`
- `littlefs-esp32.bin`
- `firmware-esp32s3.bin`
- `littlefs-esp32s3.bin`

**Device moet weten welke te downloaden:**
```cpp
// In main.cpp
#ifdef CONFIG_IDF_TARGET_ESP32
  String boardType = "esp32";
#elif CONFIG_IDF_TARGET_ESP32S3
  String boardType = "esp32s3";
#endif

// Bij download:
String firmwareUrl = baseUrl + "/firmware-" + boardType + ".bin";
String littlefsUrl = baseUrl + "/littlefs-" + boardType + ".bin";
```

---

## Conclusie

Het implementeren van gedistribueerde pull updates is **technisch haalbaar** met de huidige hardware en software stack. De belangrijkste vereisten zijn:

1. ✅ **Custom partition table** voor OTA support (ESP32 + ESP32-S3)
2. ✅ **HTTPClient integratie** met GitHub API
3. ✅ **Update UI** met real-time status
4. ✅ **Background task** voor periodieke checks
5. ✅ **NVS storage** voor persistentie
6. ✅ **Error handling** en rollback
7. ✅ **Versie format**: fw-YYYY-M.m.pp en fs-YYYY-M.m.pp
8. ✅ **current.info** file in filesystem
9. ✅ **ESP32-S3 migratie** voorbereid

**KRITIEK - VOOR JE BEGINT:**
```bash
git add .
git commit -m "Pre-OTA stable snapshot"
git tag -a v1.0.0-pre-ota -m "Rollback point"
git push origin main --tags
```

**Aanbeveling:** 
1. Maak eerst de rollback tag
2. Werk op feature branch
3. Start met Phase 1-3 voor basis functionaliteit
4. Test grondig op development board
5. Merge pas naar main als volledig werkend
