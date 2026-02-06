# Technical Decisions & Architecture

Dit document beschrijft belangrijke technische beslissingen en de redenering erachter.

## ESP32-WROOM → ESP32-S3 Migration (Feb 2026)

### Hardware Switch Rationale
Geport van ESP32-WROOM-32 naar ESP32-S3-DevKitC-1 om:
- **Performance**: Dual-core @ 240 MHz (vs single @ 160 MHz)
- **Memory**: 8MB Flash + 8MB PSRAM (vs 4MB Flash only)
- **Connectivity**: USB-C native CDC (vs UART + CH340)
- **Future-proof**: More GPIO, advanced peripherals, better support
- **Maintainability**: WROOM EOL, S3 is preferred platform for new designs

### Changes Made
```cpp
// platformio.ini
[platformio]
default_envs = esp32s3dev  # Changed from esp32dev

// include/config.h
#define OLED_SDA_PIN 8   # GPIO5 → GPIO8 (S3 has I2C on different pins)
#define OLED_SCL_PIN 9   # GPIO4 → GPIO9
```

### Backwards Compatibility
- Both environments still supported in platformio.ini (see `[env:esp32dev]`)
- Code is hardware-agnostic (all pins in config.h)
- Easy to revert if needed (see MIGRATION.md)

### Build Procedure Update
```bash
# Old (WROOM)
pio run -e esp32dev -t upload

# New (S3) - Now default
pio run  # or explicitly: pio run -e esp32s3dev
```

See [MIGRATION.md](MIGRATION.md) for complete porting details.

---

## GitHub Release Tagging Convention

### Standard
Bij het maken van een nieuwe GitHub release:
- **Tag name**: `YYYY-M.0.NN` (bijvoorbeeld: `2026-1.0.07`) - **ZONDER `v` prefix**
- **Release title**: Beschrijving van de release
- **Binary files**: `fw-YYYY-M.0.NN.bin` en `fs-YYYY-M.0.NN.bin`

### Procedure
```bash
# Na compilatie en testen (now on S3):
mkdir -p release/YYYY-M.0.NN
cp .pio/build/esp32s3dev/firmware.bin release/YYYY-M.0.NN/fw-YYYY-M.0.NN.bin
cp .pio/build/esp32s3dev/littlefs.bin release/YYYY-M.0.NN/fs-YYYY-M.0.NN.bin

# Git commit en tag
git add -A
git commit -m "v[YYYY-M.0.NN]: Release description"
git tag YYYY-M.0.NN        # WITHOUT v prefix!
git push && git push origin YYYY-M.0.NN

# GitHub Release
# 1. Ga naar https://github.com/Mooijman/ESP32-baseline/releases/new
# 2. Select tag: YYYY-M.0.NN (without v)
# 3. Upload fw-YYYY-M.0.NN.bin en fs-YYYY-M.0.NN.bin
# 4. Publish
```

**Reden**: 
- Consistency met semantic versioning (tag names zonder v prefix)
- Simplifies version comparison in code (no v stripping needed)
- Matches binary file naming convention

---

## Release Binary Naming Convention

### Standard
Bij het maken van een nieuwe release moeten de binary files de volgende naming convention volgen:
- **Firmware**: `fw-YYYY-M.0.NN.bin` (bijvoorbeeld: `fw-2026-1.0.07.bin`)
- **Filesystem**: `fs-YYYY-M.0.NN.bin` (bijvoorbeeld: `fs-2026-1.0.07.bin`)

### Procedure
```bash
# Na compilatie, kopieer binaries naar release folder met correcte namen:
mkdir -p release/YYYY-M.0.NN
cp .pio/build/esp32dev/firmware.bin release/YYYY-M.0.NN/fw-YYYY-M.0.NN.bin
cp .pio/build/esp32dev/littlefs.bin release/YYYY-M.0.NN/fs-YYYY-M.0.NN.bin
```

**Reden**: Consistentie met bestaande releases en compatibility met update systeem dat `fw-` en `fs-` prefixes verwacht.

---

## Automatische Versie Synchronisatie

### Probleem
Versie nummers moesten handmatig worden bijgewerkt op meerdere plaatsen na firmware updates, wat foutgevoelig was en inconsistenties veroorzaakte.

### Oplossing
Geïmplementeerd in `readCurrentVersion()` (lines 163-202 in `src/main.cpp`):

```cpp
#define FIRMWARE_VERSION "fw-2026-1.0.05"
#define FILESYSTEM_VERSION "fs-2026-1.0.05"
```

**Mechanisme:**
1. Compile-time versies in `#define` statements zijn de single source of truth
2. Bij boot: Extract versie uit `#define` (verwijder "fw-"/"fs-" prefix)
3. Vergelijk met opgeslagen NVS versies
4. Bij mismatch: Update NVS automatisch met compile-time versie
5. Log: "Firmware version updated: X -> Y"

**Voordelen:**
- Eén plek om versie te wijzigen (de `#define` statements)
- Automatische sync na OTA updates, GitHub updates, en serial uploads
- Consistentie tussen firmware code en opgeslagen settings
- Geen handmatige interventie nodig

**Impact:**
- Werkt bij elke boot en na elke update methode
- Betrouwbaar en onderhoudsarm

---

## DHCP Checkbox Bug Fix

### Probleem
Bij het togglen van "Enable GitHub Update" rebootte de ESP32 onverwacht, ook al was alleen die setting veranderd.

### Root Cause
In settings form handler (lines 1027-1035):
- Opgeslagen DHCP waarde in NVS: `"true"` of `"false"` (string)
- POST waarde van checkbox: `"on"` of leeg (standard HTML form behavior)
- Vergelijking faalde altijd → `shouldReboot = true`

### Oplossing
Normalisatie van beide waarden naar "on"/"off" formaat:

```cpp
String currentDHCP = currentConfig.dhcp_enabled;
if (currentDHCP == "true") currentDHCP = "on";
if (currentDHCP == "false") currentDHCP = "off";

String newDHCP = (server.hasArg("dhcp") && server.arg("dhcp") == "on") ? "on" : "off";

if (currentDHCP != newDHCP) {
    shouldReboot = true;
}
```

**Voordelen:**
- Accurate detectie van DHCP wijzigingen
- Geen onnodige reboots meer
- GitHub Update checkbox werkt nu correct zonder side-effects

---

## CSS Optimalisatie met Custom Properties

### Doelstelling
Code optimaliseren zonder visuele wijzigingen in layout.

### Aanpak
Geïmplementeerd CSS custom properties (variabelen) voor:

**1. Color Palette**
```css
--color-primary: #034078;
--color-secondary: #1282A2;
--color-accent: #FEFCFB;
--color-success: #28a745;
--color-danger: #d9534f;
```

**2. Spacing Scale**
```css
--spacing-xs: 4px;
--spacing-sm: 8px;
--spacing-md: 10px;
--spacing-lg: 15px;
--spacing-xl: 16px;
--spacing-2xl: 18px;
--spacing-3xl: 20px;
```

**3. Border Radius**
```css
--border-radius-sm: 4px;
--border-radius-md: 5px;
--border-radius-lg: 8px;
--border-radius-xl: 10px;
```

**4. Typography**
```css
--font-size-sm: 12px;
--font-size-base: 14px;
--font-size-md: 16px;
--font-size-lg: 18px;
--font-size-xl: 1.4rem;
```

**5. Transitions**
```css
--transition-base: 0.2s ease;
--transition-medium: 0.3s ease;
--transition-slow: 0.4s ease-in-out;
```

### Resultaat
- Voor: 980 lines met duplicates
- Na: 993 lines met variabelen (beter georganiseerd)
- Backup: `style.css.backup`
- **Gegarandeerd: Geen visuele wijzigingen**

### Voordelen
- Centraal beheer van design tokens
- Consistentie door hele codebase
- Makkelijker theming in de toekomst
- Betere leesbaarheid en onderhoudbaarheid

---

## Default Settings voor Nieuwe Installaties

### Beslissing
Bij eerste boot (clean flash) worden deze defaults gebruikt:

```cpp
preferences.putString("debug", "false");
preferences.putString("gpioviewer", "false");
preferences.putString("ota", "false");
preferences.putString("updates", "false");
preferences.putString("dhcp", "true");
```

### Redenering
- **Debug OFF**: Production-ready default, voorkomt verbose logging
- **GPIO Viewer OFF**: Niet standaard nodig, kan performance impact hebben
- **OTA OFF**: Veiligheid - expliciet inschakelen vereist
- **GitHub Updates OFF**: Voorkomt automatische updates zonder consent
- **DHCP ON**: Meest gebruikelijke netwerk configuratie

### Impact
- Veilige defaults voor productie gebruik
- Gebruiker moet bewust features inschakelen
- Voorkomt onverwacht gedrag bij nieuwe installaties

---

## UI Text Consistency

### Wijzigingen
1. "Enable OTA Upload" → "Enable OTA Update"
2. "Enable Updates" → "Enable GitHub Update"
3. "Powercycle ESP32 now!" → "Powercycle device now!"

### Redenering
- **OTA Update** is technisch correctere term (niet alleen upload)
- **GitHub Update** is duidelijker over update bron
- **Device** is generiek, maakt code hardware-onafhankelijk

---

## Version Placeholder Generalisatie

### Beslissing
Hard-coded versie nummers in HTML vervangen door generiek "YYYY-M.m.p" format.

### Redenering
- Versie wordt runtime ingevuld door server
- Geen handmatige updates in HTML nodig
- Voorkomt inconsistenties tussen HTML en firmware

---

## Release Binary Naming Convention

### Format
- Firmware: `fw-YYYY-M.m.p.bin`
- Filesystem: `fs-YYYY-M.m.p.bin`

### Redenering
- Prefix maakt type direct duidelijk
- Jaar prefix helpt bij chronologische sortering
- Consistent met interne `#define` naming
- Compatible met OTA update systeem

### Implementatie
```bash
# In release directory
mv firmware.bin fw-2026-1.0.05.bin
mv littlefs.bin fs-2026-1.0.05.bin
```

---

## Flash Erase voor Clean Install

### Wanneer
Bij grote updates of probleem troubleshooting.

### Commando
```bash
esptool.py --port /dev/cu.usbserial-0001 erase_flash
```

### Impact
- Verwijdert ALL data (firmware, filesystem, NVS settings)
- Duurt ~4.3 seconden
- Na erase: Full upload required (firmware + filesystem)
- NVS wordt opnieuw geïnitialiseerd met defaults

---

## Geheugen Management

### Huidige Status (versie 2026-1.0.05)
- **Firmware**: 1,330,952 bytes (67.7% van 4MB flash)
- **RAM**: 53,856 bytes (16.4%)
- **Filesystem**: 131,072 bytes (LittleFS)

### Overwegingen
- Flash gebruik is hoog maar acceptabel
- Ruimte voor toekomstige features beperkt (~1.3MB vrij)
- Bij toekomstige uitbreidingen: Monitor flash gebruik
- Overweeg code optimalisatie als >80% flash gebruikt

---

## Development Workflow

### Version Update Proces
1. Update `#define FIRMWARE_VERSION` en `FILESYSTEM_VERSION` in `src/main.cpp`
2. Full flash erase: `esptool.py erase_flash`
3. Upload firmware: `pio run -t upload`
4. Upload filesystem: `pio run -t uploadfs`
5. Test functionaliteit
6. Prepare release:
   - `mkdir -p release/YYYY-M.m.p`
   - Copy binaries from `.pio/build/esp32dev/`
   - Rename to `fw-YYYY-M.m.p.bin` en `fs-YYYY-M.m.p.bin`
7. Publish GitHub release met binaries

### Testing Checklist
- [ ] Automatic version sync werkt
- [ ] Settings behouden na reboot
- [ ] DHCP checkbox werkt zonder spurious reboot
- [ ] OTA update via serial/OTA/GitHub
- [ ] Web interface laadt correct
- [ ] Alle checkboxes functioneel

---

## Hardware Specificaties

- **Chip**: ESP32-D0WDQ6 revision v1.0
- **MAC Address**: 30:ae:a4:0f:2c:3c
- **Flash**: 4MB
- **Upload Port**: /dev/cu.usbserial-0001
- **Baud Rate**: 460800
- **OTA IP**: 172.17.1.159 (lokaal netwerk)

---

## Platform & Framework

- **Platform**: Espressif 32 v53.3.11 (latest stable)
- **Framework**: Arduino v3.1.1
- **Libraries**: v5.3.0
- **Build Tool**: PlatformIO

---

## Future Considerations

### Potentiële Verbeteringen
1. **OTA Rollback**: Implementeer vorige versie restore bij falende update
2. **Version History**: Log van alle versie updates in NVS
3. **Flash Usage Warning**: Alert bij >80% flash gebruik
4. **Settings Export/Import**: Backup/restore functionaliteit
5. **Update Verification**: Checksum validatie voor binaries

### Code Maintenance
- Houd CSS custom properties up-to-date bij nieuwe design elementen
- Monitor memory usage bij nieuwe features
- Test automatic version sync na elke update methode
- Documenteer breaking changes in CHANGELOG.md

---

**Laatste update**: 2 februari 2026
**Huidige versie**: 2026-1.0.05
