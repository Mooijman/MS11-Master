# Release v2026.2.13.01 - Twiboot I2C Bootloader Integration Fixed

## 🎯 Major Fix

### Twiboot Bootloader Entry/Exit Now Works Reliably
- Fixed mutex conflicts between web API commands and background heartbeat polling
- `/api/i2c/bootloader` (enter bootloader) and `/api/i2c/exit-bootloader` now use `I2CManager` instead of direct `Wire1` calls
- Arduino reliably transitions between application mode (0x30) and bootloader mode (0x14)

## 🔧 Technical Changes

### I2C Bus Mutex Handling
**Problem**: Direct `Wire1` calls in bootloader routes bypassed I2CManager mutex, causing race conditions with the 2-second heartbeat polling on the same slave bus (GPIO5/6).

**Solution**: 
- Enter bootloader: `manager.writeRegister(0x30, 0x99, 0xB0)` with proper mutex acquisition
- Exit bootloader: `manager.write(0x14, {0x01, 0x80}, 2)` with proper mutex acquisition
- Status check: `manager.ping(address, I2C_BUS_SLAVE)` already correct

### Bootloader Protocol Verified
- **EEPROM boot magic**: bytes 510-511 = 0xB007 (little-endian: 0x07, 0xB0)
- **Twiboot startup delay**: 5 seconds after reset before TWI becomes active at 0x14
- **Exit command**: CMD_SWITCH_APPLICATION (0x01) + BOOTTYPE_APPLICATION (0x80)
- **Fuses**: hfuse=0xD4 (BOOTRST active, 2KB bootloader @ 0x7800)

### ISP Flash Procedure Documented
- **Critical**: Twiboot gets wiped by chip erase during ISP upload
- Must reflash twiboot after any `pio run -t upload` or `avrdude -e`
- Documented correct avrdude sequence: chip erase → app with `-D` → twiboot with `-D`
- See [TWIBOOT_INTEGRATION.md](../../TWIBOOT_INTEGRATION.md) for full details

## 📝 Documentation

### New Files
- **TWIBOOT_INTEGRATION.md**: Complete guide to I2C bootloader integration
  - Architecture and hardware setup
  - Boot decision logic and EEPROM protocol
  - API routes with code examples
  - ISP flash procedure
  - Troubleshooting guide
  - Testing checklist

### Updated Files
- **CHANGELOG.md**: Added detailed technical notes on the fix
- **.github/copilot-instructions.md**: Added critical mutex warning
- **include/config.h**: Version bumped to 2026.2.13.01

## ✅ Verification

Tested full bootloader cycle multiple times:
1. Arduino starts at 0x30 (application mode)
2. POST `/api/i2c/bootloader` → Arduino resets → appears at 0x14 within 6 seconds
3. GET `/api/twi/status` → `{"connected": true, "version": "twiboot"}`
4. POST `/api/i2c/exit-bootloader` → Arduino returns to 0x30 within 2 seconds
5. Cycle repeats reliably without bus conflicts

## 🔄 Upgrade Notes

**From any previous version:**
1. Flash this firmware via OTA or USB
2. No LittleFS changes required (but included for completeness)
3. No settings migration needed

**For MS11-control slave:**
- If twiboot was wiped during ISP programming, reflash it following the procedure in TWIBOOT_INTEGRATION.md
- Verify fuses: `avrdude -c usbtiny -p m328p -U hfuse:r:-:h` should return `0xd4`

## 📦 Files in This Release

- `fw-2026.2.13.01.bin` - Main firmware (1.3 MB)
- `fs-2026.2.13.01.bin` - LittleFS filesystem (1.9 MB)

## 🐛 Known Issues

None. Bootloader integration is fully functional.

## 🔗 Links

- [Full Change History](../../CHANGELOG.md)
- [Twiboot Integration Guide](../../TWIBOOT_INTEGRATION.md)
- [Project Documentation](../../README.md)
