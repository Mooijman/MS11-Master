# TODO - Feature Requests & Improvements

## High Priority

### [ ] Progress Bar / Status Melding bij Firmware Update
**Doel**: Gebruiker informeren tijdens OTA update proces

**Requirements**:
- Visual progress bar (0-100%) tijdens firmware download en schrijven
- Alternatief: Duidelijke status melding "Firmware update in progress..."
- **KRITISCH**: Waarschuwing "DO NOT DISCONNECT POWER"
- Blijft zichtbaar gedurende hele update proces
- Bij succes: "Update complete - Rebooting..."
- Bij falen: "Update failed - Device will reboot"

**Technisch**:
- Update via OTA: `ArduinoOTA.onProgress()`
- Update via GitHub: Progress feedback tijdens download chunks
- Overweeg OLED display update voor standalone feedback
- HTML page met auto-refresh voor web-based updates

**Impact**: Gebruiksveiligheid - voorkomt brick door power loss tijdens update

---

### [ ] Filesystem Update "Update Complete" Melding
**Doel**: Bevestiging na succesvolle filesystem update

**Requirements**:
- Na filesystem upload: Duidelijke "Filesystem update complete" melding
- Zowel via serial console als web interface
- Geen automatische reboot - gebruiker bepaalt wanneer
- Status indicator (LED/OLED) optioneel

**Technisch**:
- Extend `handleUpdate()` met filesystem detection
- Separate success messages voor firmware vs filesystem
- Log naar Serial: "Filesystem update completed successfully"

**Impact**: User experience - duidelijkheid over update status

---

### [ ] Automatische Flash Erase bij Serial Upload
**Doel**: Clean install garantie bij elke serial firmware update

**Requirements**:
- Voor elke serial upload: Automatisch flash erase
- Optie om te skippen indien gewenst (advanced users)
- Default: Erase enabled
- Warning in console: "Full flash erase will be performed"

**Technisch**:
**Optie 1: PlatformIO extra_scripts**
```python
# pre_upload.py
Import("env")
env.AddPreAction("upload", "esptool.py --port $UPLOAD_PORT erase_flash")
```

**Optie 2: platformio.ini**
```ini
[env:esp32dev]
upload_flags = 
    --before=default_reset
    --after=hard_reset
    --erase-all
```

**Optie 3: Custom upload target**
```ini
extra_scripts = pre:scripts/upload_with_erase.py
```

**Overwegingen**:
- Upload tijd neemt toe (~4 seconden voor erase)
- Alle NVS settings worden gereset naar defaults
- Documenteer in README.md voor gebruikers

**Impact**: Consistentie - voorkomt oude config problemen

---

## Medium Priority

### [ ] Main Loop Opschonen
**Doel**: Betere code organisatie en leesbaarheid

**Huidige situatie** (src/main.cpp):
- `loop()` bevat te veel directe functionaliteit
- Mixing van concerns (OTA, debug, updates, LED)
- Moeilijk te onderhouden

**Voorgestelde structuur**:
```cpp
void loop() {
    handleNetworkTasks();      // ArduinoOTA, WiFi status
    handleUpdateTasks();       // GitHub updates check
    handleDebugTasks();        // Serial debug output
    handleDisplayTasks();      // OLED updates
    handleLEDTasks();          // Status LED
    
    yield(); // Allow background tasks
}
```

**Voordelen**:
- Clear separation of concerns
- Makkelijker debuggen
- Modulaire aanpak (zie volgende item)

**Impact**: Code kwaliteit en onderhoudbaarheid

---

### [ ] Modulair Maken van Broncode
**Doel**: Code opsplitsen in logische modules voor betere organisatie

**Voorgestelde structuur**:
```
src/
├── main.cpp                 (setup, loop, core logic)
├── config.h                 (defines, constants)
├── modules/
│   ├── wifi_manager.cpp/h   (WiFi, AP mode, connection)
│   ├── web_server.cpp/h     (HTTP handlers, routes)
│   ├── ota_manager.cpp/h    (OTA updates, ArduinoOTA)
│   ├── github_updater.cpp/h (GitHub release checks)
│   ├── settings.cpp/h       (NVS read/write, config)
│   ├── display.cpp/h        (OLED interface)
│   ├── led_control.cpp/h    (Status LED)
│   └── debug.cpp/h          (Debug output, GPIO viewer)
```

**Voordelen**:
- **Separation of concerns**: Elke module heeft één verantwoordelijkheid
- **Testbaarheid**: Modules kunnen individueel getest worden
- **Herbruikbaarheid**: Modules kunnen in andere projecten gebruikt worden
- **Onderhoudbaarheid**: Wijzigingen zijn gelokaliseerd
- **Compileertijd**: Alleen gewijzigde modules worden hercompiled

**Migratie strategie**:
1. Start met config.h - extract alle defines en constants
2. WiFi manager - isoleer WiFi logica
3. Settings module - NVS operations
4. Web server - HTTP handlers
5. OTA/Updates - update logica
6. Display/LED - hardware interfaces
7. Refactor main.cpp - gebruik modules

**Overwegingen**:
- Flash/RAM impact: Header guards en inline functies
- Dependencies tussen modules minimaliseren
- Gebruik forward declarations waar mogelijk
- Overweeg namespaces voor grotere modules

**Breaking changes**:
- Compile process aangepast
- Mogelijk extra includes nodig in bestaande code

**Impact**: Grote verbetering in code organisatie, lang termijn onderhoudbaarheid

---

## Low Priority / Nice to Have

### [ ] LED Status Patterns
- Verschillende blink patronen voor verschillende states
- Fast blink: Update in progress
- Slow blink: Normal operation
- Solid: Error state

### [ ] Web-Based File Manager Improvements
- Drag & drop file upload
- Bulk delete
- File preview voor text bestanden

### [ ] Settings Validation
- Input validation op client-side (JavaScript)
- Server-side validation met error messages
- IP address format checking
- Port range validation

### [ ] Backup & Restore
- Export alle settings naar JSON file
- Import settings from file
- Includeert network config

### [ ] Update Scheduling
- Configure update check interval
- Maintenance window voor automatische updates
- "Check for updates" button

---

## Technical Debt

### [ ] Error Handling Improvements
- Consistent error reporting
- Error codes voor different failure scenarios
- User-friendly error messages
- Recovery procedures

### [ ] Memory Optimization
- Flash usage is 67.7% - monitor bij nieuwe features
- Overweeg String vs char* optimalisaties
- Check voor memory leaks in long-running operations

### [ ] Security Hardening
- HTTPS voor OTA updates (optioneel)
- Authentication voor web interface
- Secure storage van credentials
- Rate limiting voor failed login attempts

---

## Completed ✓

- ✓ Automatische versie synchronisatie
- ✓ DHCP checkbox bug fix
- ✓ CSS optimalisatie met custom properties
- ✓ UI text consistency improvements
- ✓ Default settings voor nieuwe installaties
- ✓ Release binary naming convention
- ✓ Documentation (CHANGELOG.md, DECISIONS.md)

---

**Laatste update**: 2 februari 2026
**Project versie**: 2026-1.0.05
