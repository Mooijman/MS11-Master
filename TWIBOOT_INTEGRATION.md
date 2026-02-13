# Twiboot I2C Bootloader Integration

## Overview

MS11-Master integrates with MS11-control's twiboot I2C bootloader (judasgutenberg fork) to enable firmware updates over I2C without requiring physical ISP access. The bootloader runs on the ATmega328P slave at address 0x14, while the application runs at 0x30.

**Status**: ✅ **FULLY WORKING** as of 2026.2.13.01

## Architecture

### Hardware Setup
- **ESP32-S3** (Master): XIAO board, dual I2C buses
  - Bus 0 (Wire): GPIO8/9 @ 100kHz - Display devices (LCD, OLED, Seesaw)
  - Bus 1 (Wire1): GPIO5/6 @ 100kHz - ATmega328P slave (0x30/0x14)
- **ATmega328P** (Slave): 8MHz internal, 3.3V
  - Application address: **0x30** (defined in test_i2c_slave.cpp)
  - Bootloader address: **0x14** (TWAR=0x28 in twiboot)
  - Flash layout: App @ 0x0000-0x77FF, Twiboot @ 0x7800-0x7FFF (2KB boot section)

### Fuse Configuration
```
hfuse = 0xD4  // BOOTRST=0 (jump to bootloader on reset), BOOTSZ=00 (2KB)
lfuse = 0xE2  // 8MHz internal RC oscillator
efuse = 0xFD  // BOD 2.7V
```

**Critical**: `BOOTRST` must be active (bit cleared) so reset jumps to bootloader first.

## Boot Decision Logic

### Twiboot Startup Sequence
1. **Reset** → PC jumps to 0x7800 (bootloader)
2. Watchdog disabled (`wdt_disable()`)
3. Read EEPROM[510-511] (boot magic)
4. **If EEPROM == 0xB007** (little-endian: byte 510=0x07, byte 511=0xB0):
   - Clear EEPROM[510-511] to 0x0000
   - Set `boot_timeout = 0xFFFF` (~36 minutes at 8MHz)
   - **DELAY: `_delay_ms(5000)`** - 5 second startup before TWI enabled
   - Initialize TWI at address 0x14
   - Enter command loop (wait for firmware upload or exit command)
5. **If EEPROM != 0xB007**:
   - Jump to application at 0x0000 immediately

### Application Bootloader Entry
File: `MS11-control/src/test_i2c_slave.cpp`

```cpp
case REG_ENTER_BOOTLOADER:  // 0x99
  if (receivedData == 0xB0) {  // Safety magic
    EEPROM.write(510, 0x07);
    EEPROM.write(511, 0xB0);
    wdt_enable(WDTO_15MS);
    while(1);  // Wait for WDT reset
  }
  break;
```

**Flow**:
1. ESP32 sends `I2C_WRITE(0x30, {0x99, 0xB0})`
2. ATmega ISR receives register 0x99 + data 0xB0
3. Writes boot magic to EEPROM
4. Enables 15ms watchdog
5. Infinite loop → WDT reset
6. Bootloader reads EEPROM magic → stays active at 0x14

## ESP32 API Routes

### 1. Enter Bootloader
**Endpoint**: `POST /api/i2c/bootloader`

**Code** (web_server_routes.cpp):
```cpp
I2CManager& manager = I2CManager::getInstance();

// Check Arduino present
if (!manager.ping(0x30, I2C_BUS_SLAVE)) {
  return {"error": "Arduino not responding at 0x30"};
}

// Send bootloader command via I2CManager (with mutex)
bool success = manager.writeRegister(0x30, 0x99, 0xB0);
```

**Success Response**:
```json
{
  "success": true,
  "message": "Bootloader command sent (Arduino rebooting)",
  "hint": "Bootloader startup takes ~5 seconds. Polling will detect when ready."
}
```

**Timing**:
- T+0ms: Command sent
- T+15ms: WDT reset triggers
- T+20-80ms: Arduino resets (I2C ping fails during this gap)
- T+80ms-5000ms: Twiboot in `_delay_ms(5000)` startup delay (TWI not yet active)
- T+5000ms+: Twiboot responsive at 0x14

### 2. Exit Bootloader
**Endpoint**: `POST /api/i2c/exit-bootloader`

**Code**:
```cpp
I2CManager& manager = I2CManager::getInstance();

uint8_t exitCmd[2] = {0x01, 0x80};  // CMD_SWITCH_APPLICATION + BOOTTYPE_APPLICATION
bool success = manager.write(0x14, exitCmd, 2);
```

**Success Response**:
```json
{
  "success": true,
  "message": "Bootloader exit command sent",
  "hint": "Arduino will return to application mode"
}
```

**Twiboot Exit Protocol**:
- Byte 1: `0x01` = CMD_SWITCH_APPLICATION
- Byte 2: `0x80` = BOOTTYPE_APPLICATION
- Bootloader clears EEPROM magic, resets → app starts at 0x0000

### 3. Status Check
**Endpoint**: `GET /api/twi/status`

**Code**:
```cpp
I2CManager& manager = I2CManager::getInstance();

if (manager.ping(0x14, I2C_BUS_SLAVE)) {
  return {"connected": true, "signature": "1E 95 0F", "version": "twiboot"};
} else if (manager.ping(0x30, I2C_BUS_SLAVE)) {
  return {"appConnected": true, "hint": "Arduino in normal mode"};
} else {
  return {"connected": false, "appConnected": false};
}
```

## Critical Implementation Details

### Mutex Handling (FIXED in 2026.2.13.01)
**Problem**: Direct `Wire1.beginTransmission()` calls bypassed I2CManager mutex, causing conflicts with 2-second heartbeat polling.

**Solution**: All bootloader routes now use `I2CManager` methods:
- `manager.ping(address, I2C_BUS_SLAVE)` - Bus availability check
- `manager.writeRegister(0x30, 0x99, 0xB0)` - Enter bootloader
- `manager.write(0x14, exitCmd, 2)` - Exit bootloader

**Why It Matters**: The slave bus (Wire1) is shared between:
1. Web API commands (bootloader entry/exit)
2. Background heartbeat polling (every 2 seconds in main.cpp)
3. LED control commands from web UI

Without mutex protection, concurrent transactions corrupt I2C state → Arduino appears unresponsive.

### Twiboot 5-Second Delay
The `_delay_ms(5000)` in twiboot startup means:
- **First 5 seconds after reset**: TWI not initialized, pings to 0x14 will fail
- **After 5 seconds**: Bootloader fully active and responsive

**Web UI Polling**: The i2cdemo.html page polls `/api/twi/status` every 500ms for up to 15 seconds, which covers the startup delay.

## ISP Flash Procedure

### Problem: Twiboot Gets Wiped
When using `pio run -t upload` (or `avrdude -e`), chip erase wipes the entire flash including bootloader section.

### Solution: Preserve Bootloader
**Option 1 - PlatformIO (Recommended)**:
```ini
[env:test_i2c_slave]
board_bootloader.file = bootloader/twiboot.hex
board_bootloader.size = 2048
upload_protocol = usbtiny
upload_flags = -B5
```
Then: `pio run -e test_i2c_slave -t bootloader` after ISP upload.

**Option 2 - Manual avrdude**:
```bash
# Initial flash (chip erase required)
avrdude -c usbtiny -p m328p -B10 -e

# Upload application (NO erase)
avrdude -c usbtiny -p m328p -B10 -D -U flash:w:firmware.hex

# Upload twiboot (NO erase)
avrdude -c usbtiny -p m328p -B10 -D -U flash:w:twiboot.hex

# Verify
avrdude -c usbtiny -p m328p -B10 -U flash:v:firmware.hex
avrdude -c usbtiny -p m328p -B10 -U flash:v:twiboot.hex
```

**Critical Flags**:
- `-B10`: Slow SPI clock (fixes verification errors with cheap USBtiny clones)
- `-D`: Disable chip erase (preserves other flash regions)
- `-e`: Chip erase (only needed once initially)

### Verification
**Check EEPROM boot magic**:
```bash
avrdude -c usbtiny -p m328p -U eeprom:r:-:h | grep "01f0"
# Should show: 00 00 (cleared by twiboot after reading)
```

**Check flash contents**:
```bash
# Application starts at 0x0000
avrdude -c usbtiny -p m328p -U flash:r:flash_dump.hex:i

# Twiboot should be at 0x7800-0x7FFF
# Look for "TWIBOOT v3.3 NR" string in disassembly
```

## Web UI Integration

### i2cdemo.html UI Flow
1. User clicks **"Enter Bootloader"** button
2. JavaScript POSTs to `/api/i2c/bootloader`
3. Polls `/api/twi/status` every 500ms (30 attempts = 15s timeout)
4. Once `connected: true` detected, shows firmware upload form
5. User selects `.hex` file and clicks **"Upload Firmware"**
6. JavaScript POSTs hex data to `/api/twi/upload`
7. ESP32 sends Intel HEX commands over I2C to twiboot at 0x14
8. After upload complete, POST to `/api/i2c/exit-bootloader`
9. Poll until `appConnected: true` (back to 0x30)

### Status Display States
- **Normal**: Arduino detected at 0x30 → "Arduino in normal mode"
- **Bootloader**: Arduino detected at 0x14 → "Bootloader active (twiboot)"
- **Absent**: Neither address responds → "Arduino not detected"
- **Transitioning**: Brief gap during WDT reset (20-80ms)

## Troubleshooting

### Arduino Not Entering Bootloader
**Symptom**: `/api/i2c/bootloader` returns success but Arduino stays at 0x30

**Causes**:
1. Twiboot not in flash (wiped by ISP chip erase)
2. Fuses incorrect (BOOTRST not active)
3. EEPROM write failed

**Diagnostics**:
```bash
# Check fuses
avrdude -c usbtiny -p m328p -U hfuse:r:-:h  # Should be 0xD4

# Check EEPROM after bootloader command
avrdude -c usbtiny -p m328p -U eeprom:r:-:h | grep "01f0"
# Should be 07 B0 if write succeeded, then 00 00 after twiboot reads it
```

### Arduino Stuck in Bootloader
**Symptom**: Arduino at 0x14, won't exit to 0x30

**Cause**: Exit command not sent correctly or EEPROM magic still set

**Solution**:
```bash
# Via web UI
curl -X POST http://172.17.1.132/api/i2c/exit-bootloader

# Or clear EEPROM via ISP
avrdude -c usbtiny -p m328p -U eeprom:w:0x00:m -P 510
avrdude -c usbtiny -p m328p -U eeprom:w:0x00:m -P 511
# Then power cycle Arduino
```

### Bus Conflicts / Arduino Intermittently Unreachable
**Symptom**: Ping works sometimes, fails other times; "Arduino not responding" errors

**Cause**: Mutex conflicts between web commands and heartbeat polling

**Solution**: Ensure all I2C operations use `I2CManager` methods (FIXED in 2026.2.13.01)

### Very Long Bootloader Timeout
**Symptom**: Arduino in bootloader for 30+ minutes

**Cause**: Twiboot read boot magic before it was cleared, set `boot_timeout = 0xFFFF` (~36 min)

**Solution**:
- Send exit command via `/api/i2c/exit-bootloader`
- Or power cycle (EEPROM will be 0x0000 after twiboot clears it)

## Testing Checklist

- [ ] Arduino detected at 0x30 after normal power-on
- [ ] Enter bootloader command → Arduino transitions to 0x14 within 6 seconds
- [ ] Status endpoint shows `"version": "twiboot"` when at 0x14
- [ ] Exit bootloader command → Arduino returns to 0x30 within 2 seconds
- [ ] Full cycle (0x30 → 0x14 → 0x30) works 5 times in a row
- [ ] ISP flash preserves twiboot (verify with bus scan after upload)
- [ ] Web UI polling detects state transitions correctly
- [ ] No mutex conflicts during repeated enter/exit cycles

## References

- **Twiboot Repository**: https://github.com/judasgutenberg/twiboot (fork with 5s delay)
- **ATmega328P Fuses Calculator**: https://www.engbedded.com/fusecalc/
- **Intel HEX Format**: https://en.wikipedia.org/wiki/Intel_HEX
- **MS11-control test_i2c_slave.cpp**: `../MS11-control/src/test_i2c_slave.cpp`
- **Copilot Instructions**: `.github/copilot-instructions.md` (project guidelines)

## Version History

- **2026.2.13.01**: Fixed mutex conflicts, documented full integration
- **2026.2.12.x**: Initial twiboot investigation and diagnostics
- **Earlier**: Direct Wire1 implementation (had race conditions)
