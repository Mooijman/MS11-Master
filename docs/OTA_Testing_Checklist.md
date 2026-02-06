# OTA Pull Update - Testing Checklist

## Voorbereiding

### Git Status
- [x] Commit gemaakt: `450bb22`
- [x] Tag gemaakt: `v1.0.0-pre-ota`
- [x] Feature branch: `feature/ota-pull-updates`
- [ ] Alle wijzigingen gecommit

### Hardware Vereisten
- [ ] ESP32 verbonden via USB
- [ ] Werkende WiFi verbinding beschikbaar
- [ ] Serial monitor gereed (115200 baud)

## Fase 1: Compilatie

### Build Test ESP32
```bash
cd "/Users/jeroen/Documents/PlatformIO/Projects/Base+OTA+OLED+settings"
pio run -e esp32dev
```
- [ ] Compilatie succesvol (0 errors)
- [ ] Firmware size < 1.875MB (partition constraint)
- [ ] Memory usage acceptable

### Build Test ESP32-S3  
```bash
pio run -e esp32s3dev
```
- [ ] Compilatie succesvol voor ESP32-S3
- [ ] Firmware size < 3.8MB (partition constraint)

## Fase 2: First Upload

### Flash Firmware + Filesystem
```bash
# Upload firmware
pio run -e esp32dev -t upload

# Upload filesystem (includes current.info)
pio run -e esp32dev -t uploadfs
```

### Verificatie
- [ ] Device boot succesvol
- [ ] OLED display toont IP adres
- [ ] Serial output toont:
  - `OTA Update System Initialized`
  - `Firmware Version: fw-2026.1.1.08`
  - `Filesystem Version: fs-2026.1.1.08`
- [ ] Web interface bereikbaar op IP

## Fase 3: UI Testing

### Update Pagina
Navigeer naar: `http://<IP>/update.html`

- [ ] OTA sectie zichtbaar
- [ ] Current firmware toont: `fw-2026.1.1.08`
- [ ] Current filesystem toont: `fs-2026.1.1.08`
- [ ] Remote version toont: `-` (nog geen check)
- [ ] Status toont: `Idle`
- [ ] Check Nu knop zichtbaar
- [ ] Install knoppen NIET zichtbaar (geen update beschikbaar)
- [ ] Manual upload sectie nog aanwezig

### Status Polling
Open browser DevTools → Network tab

- [ ] GET `/api/update/status` elke 60 seconden
- [ ] JSON response bevat alle velden
- [ ] `state: 0` (UPDATE_IDLE)

## Fase 4: GitHub API Integration

### Settings Aanpassen
Navigeer naar: `http://<IP>/settings`

Vul in Settings → Updates:
- Updates Enabled: `ON`
- GitHub Update URL: `https://api.github.com/repos/OWNER/REPO/releases/latest`
  (Vervang OWNER/REPO met jouw repository)

- [ ] Settings opgeslagen
- [ ] Device herstart automatisch

### Check for Updates
Terug naar `/update.html`, klik **Check Nu**

Serial Monitor:
- [ ] `Checking for updates at: https://api.github.com/repos/...`
- [ ] `HTTP response code: 200` (of 404 als geen releases)
- [ ] JSON parsing succesvol

Als GitHub release bestaat:
- [ ] Remote version update zichtbaar
- [ ] Install knoppen verschijnen
- [ ] `firmwareAvailable` of `littlefsAvailable` = true

Als GEEN release:
- [ ] Alert: "Geen updates beschikbaar"
- [ ] Status blijft Idle

### Error Scenarios
Test met ongeldige URLs:

1. **Invalide URL**: `http://invalid-url`
   - [ ] Error message in UI
   - [ ] `lastError` bevat foutmelding

2. **Timeout**: Disconnect WiFi, klik Check Nu
   - [ ] Timeout na ~15 seconden
   - [ ] Error in Serial Monitor

## Fase 5: Download & Install

⚠️ **Vereist:** GitHub release met assets:
- `firmware-esp32.bin`
- `littlefs-esp32.bin`

### Firmware Update Test
1. Klik **Installeer Firmware**
   - [ ] Bevestigingsvenster: "Device zal herstarten..."
   - [ ] Progress bar verschijnt
   - [ ] Status toont: `Downloaden...` → `Installeren...`
   - [ ] Progress percentage update (~2 seconden polling)

Serial Monitor:
- [ ] `Starting firmware download...`
- [ ] `Firmware download progress: X%`
- [ ] `Firmware Update Success`
- [ ] `Rebooting in 5 seconds...`
- [ ] Device herstart

Na herstart:
- [ ] OLED toont nieuwe firmware versie (als gewijzigd in nieuwe build)
- [ ] Serial toont nieuwe versie
- [ ] Web interface werkt normaal

### LittleFS Update Test
1. Klik **Installeer Filesystem**
   - [ ] Progress bar verschijnt
   - [ ] Download en install succesvol

Serial Monitor:
- [ ] `Starting LittleFS download...`
- [ ] `LittleFS Update Success`
- [ ] Device herstart

⚠️ **LET OP:** LittleFS update wist alle bestanden!
- [ ] Web interface files nog aanwezig (waren in nieuwe LittleFS image)
- [ ] Settings mogelijk gereset (waren in NVS, niet LittleFS)

### Both Update Test
1. Klik **Installeer Alles**
   - [ ] Firmware download → install
   - [ ] LittleFS download → install
   - [ ] Single reboot

## Fase 6: Background Checks

### Boot Check
1. Herstart device (physical reset of software reboot)

Serial Monitor:
- [ ] `Boot update check scheduled in X seconds`
- [ ] X is tussen 0-600 seconden (0-10 minuten)

Wacht X seconden:
- [ ] `Background update check starting...`
- [ ] Check uitgevoerd
- [ ] `Background check completed successfully`

### Periodic Checks
Laat device 6+ uur draaien (of wijzig `CHECK_INTERVAL` voor snellere test)

- [ ] Check elke 6 uur
- [ ] Maximum 4 checks per 24 uur
- [ ] Counter reset na 24 uur

Test rate limiting (wijzig CHECK_INTERVAL naar 5 minuten):
```cpp
const unsigned long CHECK_INTERVAL = 5UL * 60UL * 1000UL; // 5 minutes for testing
```
- [ ] Na 4 checks binnen 24 uur, geen verdere checks
- [ ] Serial toont dat check wordt geskipt

## Fase 7: Error Handling

### Network Errors
1. **WiFi disconnect tijdens download**
   - [ ] Download failed
   - [ ] Error message in UI
   - [ ] Device blijft werkend (geen brick)

2. **Incomplete download**
   - Simuleer door server te stoppen mid-download
   - [ ] Size verification failed
   - [ ] Update.end() returns false
   - [ ] Rollback naar oude versie
   - [ ] Device blijft werkend

### Corrupt Firmware
⚠️ **GEVAARLIJK** - alleen als je serieuze rollback wilt testen

1. Upload bewust corrupte .bin file (manueel)
   - [ ] Device herstart naar oude partition
   - [ ] OTA rollback mechanisme werkt

### Invalid JSON Response
Wijzig GitHub URL naar endpoint die ongeldige JSON geeft:
- [ ] `JSON parsing failed` in Serial
- [ ] Error in UI
- [ ] State = UPDATE_ERROR

## Fase 8: ESP32-S3 Test

Als ESP32-S3 hardware beschikbaar:

```bash
pio run -e esp32s3dev -t upload
pio run -e esp32s3dev -t uploadfs
```

- [ ] Compilatie en upload ESP32-S3
- [ ] Boot succesvol
- [ ] Versies correct
- [ ] GitHub API check detecteert ESP32-S3
- [ ] Download `firmware-esp32s3.bin`
- [ ] OTA update succesvol

## Fase 9: Security

### HTTPS Certificate
Vervang `client.setInsecure()` met GitHub root CA:

```cpp
const char* github_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
// ... rest of certificate
"-----END CERTIFICATE-----\n";

client.setCACert(github_root_ca);
```

- [ ] Certificaat toegevoegd
- [ ] HTTPS verbinding succesvol
- [ ] Download werkt met certificate validation

### Hash Verification (Optioneel)
Als release sha256 hashes bevat in assets:

```cpp
// Verify download integrity
if (calculateSHA256(downloadedFile) != expectedHash) {
  Serial.println("Hash verification failed!");
  return false;
}
```

- [ ] Hash check geïmplementeerd
- [ ] Corrupt download gedetecteerd

## Fase 10: Rollback Test

Als iets fout gaat:

### Git Rollback
```bash
git checkout v1.0.0-pre-ota
pio run -e esp32dev -t upload
```
- [ ] Oude firmware restored
- [ ] Device werkt normaal
- [ ] Geen OTA functionaliteit meer

### OTA Rollback
ESP32 bootloader automatisch:
- [ ] Bij 3 mislukte boots → rollback naar vorige partition
- [ ] Test door bewust slechte firmware te uploaden

## Resultaat

### Success Criteria
- [ ] Alle basisfeatures werken
- [ ] GitHub API integration functioneel
- [ ] Firmware updates succesvol
- [ ] LittleFS updates succesvol
- [ ] Background checks werken
- [ ] Rate limiting actief
- [ ] Error handling robuust
- [ ] Device blijft altijd werkend (geen brick)

### Known Issues
Document hier eventuele problemen:
- 
- 

### Next Steps
- [ ] Merge naar main branch
- [ ] Create GitHub Actions workflow voor automated releases
- [ ] Documentatie updaten
- [ ] HTTPS certificate pinning in productie
- [ ] Version bump naar fw-2026.1.1.09 voor eerste test release

## GitHub Release Setup

Voor testing, maak een release aan:

### Release Workflow
1. Merge feature branch
2. Update version in code
3. Build firmware + LittleFS:
   ```bash
   pio run -e esp32dev
   pio run -e esp32s3dev
   ```
4. Create GitHub release:
   - Tag: `v1.0.01`
   - Assets:
     - `.pio/build/esp32dev/firmware.bin` → `firmware-esp32.bin`
     - `.pio/build/esp32dev/littlefs.bin` → `littlefs-esp32.bin`
     - `.pio/build/esp32s3dev/firmware.bin` → `firmware-esp32s3.bin`
     - `.pio/build/esp32s3dev/littlefs.bin` → `littlefs-esp32s3.bin`

### Automated Build (GitHub Actions)
Create `.github/workflows/release.yml`:

```yaml
name: Build and Release

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.9'
      - name: Install PlatformIO
        run: pip install platformio
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

- [ ] GitHub Actions workflow gemaakt
- [ ] Automated build test succesvol
- [ ] Release assets correct benoemd
