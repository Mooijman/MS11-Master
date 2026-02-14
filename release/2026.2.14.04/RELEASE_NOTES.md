# MS11 Master - Release 2026.2.14.04

## Features
- **DS18B20 Temperature Sensor Integration**: Full support for reading system temperature from MS11-control slave via DS temperature sensor
- **Dual Temperature Display**: OLED now displays both MST (master AHT10) and SLV (slave DS18B20) temperatures simultaneously
- **Temperature Format**: Fixed-point Q8.8 conversion (divide by 256) for correct Celsius display

## Changes
- Implemented `readMS11ControlTemp()` in ProbeManager for DS18B20 reading
- Updated OLED display to show:
  - **MST**: AHT10 temperature + humidity
  - **SLV**: DS temperature from MS11-control slave
- Removed all debug Serial output for cleaner operation
- Temperature data flows: SlaveController I2C registers → ProbeManager → OLED display

## Fixes
- Fixed temperature conversion from raw sensor value (e.g., 7168 → 28.0°C)
- Corrected DS18B20 fixed-point format handling

## Technical Details
- **Firmware**: 1.3M
- **Filesystem**: 1.9M
- **I2C Buses**: Bus 0 (Display devices), Bus 1 (Slave 0x30)
- **Temperature Registers**: REG_SYS_TEMP_H (0x02) / REG_SYS_TEMP_L (0x03)

## Files
- `fw-2026.2.14.04.bin` - Main firmware
- `fs-2026.2.14.04.bin` - LittleFS filesystem

## Installation
1. Flash firmware: `pio run -e esp32s3dev -t upload` (serial) or OTA
2. Upload filesystem: `pio run -e esp32s3dev -t uploadfs`
3. Or use: `./uploadfs_with_backup.sh <ESP_IP>`

## Notes
- Temperature updates every ~30 seconds (AHT10 sampling interval)
- DS sensor accessible via SlaveController probe system
- Calibration support via probe_cal.txt (offset/scale factors)
