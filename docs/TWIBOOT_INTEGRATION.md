# Twiboot I2C Bootloader Integration

**Date:** February 6, 2026  
**Status:** In Progress  
**Repository:** https://github.com/judasgutenberg/twiboot_for_arduino

## Overview

Twiboot is a TWI/I2C bootloader for AVR microcontrollers that allows remote firmware updates over I2C. This enables ESP32S3 to update firmware on Arduino slave devices without requiring serial or ICSP connections.

**Architecture:**
```
ESP32S3 (I2C Master)          Arduino ATmega328P (I2C Slave)
─────────────────             ──────────────────────────────
  GPIO6 (SDA) ───────────────→ A4 (SDA)
  GPIO7 (SCL) ───────────────→ A5 (SCL)
  GND ────────────────────────→ GND
                                
                               Twiboot bootloader @ 0x7C00
                               Firmware/app @ 0x0000
```

## Hardware Requirements

### I2C Connection (Critical)
- **ESP32S3 SDA (GPIO6)** ↔ **Arduino A4 (SDA)**
- **ESP32S3 SCL (GPIO7)** ↔ **Arduino A5 (SCL)**
- **ESP32S3 GND** ↔ **Arduino GND** ✅ **ESSENTIAL**

### Pull-up Resistors (Recommended)
- 4.7kΩ SDA to 3.3V
- 4.7kΩ SCL to 3.3V

Note: Many Arduino development boards have integrated pull-ups. Check your board's schematic.

## Bootloader Installation

### Source Repository
The recommended version is [twiboot_for_arduino](https://github.com/judasgutenberg/twiboot_for_arduino) by Gus Mueller, which includes:
- Support for chunked I2C transfers (16-byte chunks, compatible with Arduino Wire library)
- Pre-compiled hex files for common MCUs
- Virtual bootloader section support (automatic vector table patching)

### Configuration

**File:** `main.c` (bootloader source)

Key compile-time settings:
```c
#ifndef TWI_ADDRESS
#define TWI_ADDRESS             0x29    // Default, can be changed
#endif

#define F_CPU                   8000000ULL
#define TIMEOUT_MS              1000    // 1 second before boot to app
#define LED_SUPPORT             1       // Status LEDs (optional)
#define EEPROM_SUPPORT          1       // EEPROM read/write
#define USE_CLOCKSTRETCH        0       // Polling-based write completion
#define VIRTUAL_BOOT_SECTION    0       // Device-dependent
```

### Build Instructions

1. **Clone repository:**
```bash
git clone https://github.com/judasgutenberg/twiboot_for_arduino.git
cd twiboot_for_arduino
```

2. **Edit Makefile for target MCU:**
```makefile
MCU = atmega328p
F_CPU = 8000000
AVRDUDE_PROGRAMMER = usbtiny  # or your programmer
```

3. **Customize I2C address (optional):**
Edit `main.c`:
```c
#define TWI_ADDRESS 0x10  // Choose address 0x10-0x20 range
```

4. **Build:**
```bash
make clean
make
```

5. **Flash to Arduino:**
```bash
make install
make fuses
```

**Or use pre-compiled:**
- Download `pre-compiled_atmega328p_twiboot.hex` from releases
- Flash with avrdude:
```bash
avrdude -p atmega328p -P /dev/ttyUSB0 -c usbtiny -U flash:w:pre-compiled_atmega328p_twiboot.hex:i
```

## I2C Protocol

### Bootloader Operation

Twiboot waits ~1 second for I2C commands. If none received, it boots to application.

**Timeline after Arduino reset:**
- t=0ms: Bootloader active, listening on I2C address
- t=1000ms: Timeout, jump to application firmware

### Protocol Commands

Master sends SLA+W (Start, Address, Write), command byte(s), then SLA+R (Read) or STO (Stop).

| Operation | Command Sequence | Response |
|-----------|-----------------|----------|
| **Abort timeout** | SLA+W, 0x00, STO | (keeps bootloader active) |
| **Query version** | SLA+W, 0x01, SLA+R, {n}, STO | 16-byte ASCII version string |
| **Boot application** | SLA+W, 0x01, 0x80, STO | (jumps to app) |
| **Query chip info** | SLA+W, 0x02, 0x00, 0x00, 0x00, SLA+R, {8}, STO | sig(3), page(1), flash(2), eeprom(2) |
| **Read flash bytes** | SLA+W, 0x02, 0x01, addrH, addrL, SLA+R, {n}, STO | flash data |
| **Read EEPROM bytes** | SLA+W, 0x02, 0x02, addrH, addrL, SLA+R, {n}, STO | EEPROM data |
| **Write flash page** | SLA+W, 0x02, 0x01, addrH, addrL, {128 bytes}, STO | (page write) |
| **Write EEPROM** | SLA+W, 0x02, 0x02, addrH, addrL, {n}, STO | (EEPROM write) |

### Chunk-Based Updates

The Gus Mueller version supports chunked transfers (recommended for Arduino compatibility):
- **Chunk size:** 16 bytes maximum (compatible with Arduino Wire library)
- **Page size:** 128 bytes (ATmega328P)
- **Write sequence:** 
  1. Master sends 0x02, 0x01, addrH, addrL (4 bytes) + chunk 1 (16 bytes)
  2. Master sends chunk 2 (16 bytes)
  3. ... continue until 128 bytes accumulated
  4. Master sends STO → bootloader writes flash page
  5. During write: bootloader won't acknowledge (polling required)

## ESP32S3 Master Implementation

### I2C Scanner Verification

**Location:** Settings page → I2C Diagnostics → I2C Scanner

Used to verify bootloader is active:
```cpp
// src/main.cpp - existing endpoint
server.on("/api/i2c/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
  // Scans addresses 0x03-0x77
  // Should show 0x29 (bootloader) + 0x30 (LED controller)
});
```

### TwiBootUpdater Class

**File:** `include/twi_boot_updater.h` (to be created)

```cpp
class TwiBootUpdater {
public:
  TwiBootUpdater(uint8_t address = 0x29);
  
  // Device info
  struct ChipInfo {
    uint8_t signature[3];
    uint8_t pageSize;
    uint16_t flashSize;
    uint16_t eepromSize;
    bool valid;
  };
  
  // Operations
  bool queryChipInfo(ChipInfo& info);
  bool readBootloaderVersion(String& version);
  bool uploadHexFile(const String& hexContent);
  bool bootApplication();
  
  // Error handling
  String getLastError();
  
private:
  uint8_t slaveAddress;
  String lastError;
  
  // Hex file parsing
  bool parseIntelHex(const String& hexContent, 
                     std::map<uint16_t, uint8_t>& addressData);
  
  // Flash writing
  bool writePage(uint16_t address, const uint8_t* pageData, uint8_t size);
  
  // I2C primitives
  bool i2cWrite(const uint8_t* data, size_t len);
  bool i2cRead(uint8_t* buffer, size_t len);
};
```

### Web API Endpoints

**File:** `src/main.cpp`

Planned endpoints:

```cpp
// GET chip info + bootloader version
server.on("/api/twi/info", HTTP_GET, [](AsyncWebServerRequest *request) {
  // Returns: { signature, pageSize, flashSize, eepromSize, version }
});

// POST hex file upload
server.on("/api/twi/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
  // Expects: multipart/form-data with file "firmware.hex"
  // Returns: { success, progress, message }
});

// GET upload progress
server.on("/api/twi/progress", HTTP_GET, [](AsyncWebServerRequest *request) {
  // Returns: { currentPage, totalPages, percentage }
});

// POST boot application
server.on("/api/twi/boot", HTTP_POST, [](AsyncWebServerRequest *request) {
  // Returns: { success }
});
```

### Web UI

**File:** `data/twiboot.html` (to be created)

Features:
- Hex file upload (drag-drop or file picker)
- Bootloader version query
- Chip info display
- Progress bar during upload
- Boot application button
- Error messages

## Implementation Checklist

- [ ] Clone/fork twiboot_for_arduino repo
- [ ] Build bootloader with desired I2C address
- [ ] Flash bootloader to Arduino ATmega328P
- [ ] Set Arduino fuses (BOOTRST enabled)
- [ ] Verify I2C hardware connection (GND critical!)
- [ ] Test I2C Scanner shows 0x29 (after Arduino reset)
- [ ] Create `include/twi_boot_updater.h`
- [ ] Create `src/twi_boot_updater.cpp`
- [ ] Add Intel HEX parser
- [ ] Implement chunked flash writer (16-byte chunks)
- [ ] Add `/api/twi/*` endpoints to main.cpp
- [ ] Create `data/twiboot.html` web UI
- [ ] Test with sample firmware hex file
- [ ] Version bump and release

## Testing Strategy

### Phase 1: Bootloader Activation
1. Reset Arduino (press button)
2. Open `/i2c` page
3. Click "Scan I2C Bus"
4. Verify address **0x29** appears (within 1 second of reset)

### Phase 2: Chip Info Query
1. Implement `TwiBootUpdater::queryChipInfo()`
2. Add `/api/twi/info` endpoint
3. Test response: should show ATmega328P signature (0x1E 0x95 0x0F)

### Phase 3: Hex File Upload
1. Create minimal test hex file (e.g., blink sketch)
2. Implement Intel HEX parser
3. Test single-page write (128 bytes)
4. Expand to multi-page writes

### Phase 4: Web UI & Integration
1. Create upload form
2. Show progress bar
3. Handle errors gracefully
4. Add success/failure messages

## Troubleshooting

### Issue: I2C Scanner shows 0x30 but not 0x29
**Possible causes:**
1. GND not connected → **Fix:** Connect GND between ESP32S3 and Arduino
2. Bootloader not installed → **Fix:** Flash correct hex file and set fuses
3. Bootloader address mismatch → **Fix:** Check I2C_ADDRESS in main.c matches compilation
4. Arduino booted to app too fast → **Fix:** Reset Arduino, immediately scan (within 1 second)

### Issue: Bootloader doesn't respond to I2C commands
**Possible causes:**
1. Pull-up resistors missing → **Fix:** Add 4.7kΩ pull-ups or verify board has them
2. I2C bus speed incompatible → **Fix:** Try slower speed (100 kHz standard)
3. I2C clock stretching issue → **Fix:** Ensure `USE_CLOCKSTRETCH=0` in bootloader

### Issue: Flash write fails
**Possible causes:**
1. Hex file address above 0x7C00 → **Fix:** Validate bootloader section not overwritten
2. Chunk size too large → **Fix:** Use 16-byte maximum chunks
3. Power supply weak during write → **Fix:** Ensure stable 5V/3.3V

## References

- **Twiboot Arduino fork:** https://github.com/judasgutenberg/twiboot_for_arduino
- **Original Twiboot:** https://github.com/orempel/twiboot
- **Intel HEX format:** https://en.wikipedia.org/wiki/Intel_HEX
- **ATmega328P datasheet:** https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DataSheet-DS40002112A.pdf

## Related Files

- **Copilot Instructions:** [.github/copilot-instructions.md](../.github/copilot-instructions.md)
- **I2C LED Demo:** [data/i2cdemo.html](../data/i2cdemo.html)
- **Main firmware:** [src/main.cpp](../src/main.cpp)
- **Architecture:** [ARCHITECTURE.md](./ARCHITECTURE.md)

## Session History

| Date | Action |
|------|--------|
| 2026-02-06 | Initial analysis: extracted I2C address 0x29 from twiboot main.c |
| 2026-02-06 | Identified bootloader repo: judasgutenberg/twiboot_for_arduino |
| 2026-02-06 | Documented protocol, hardware requirements, implementation plan |
