# MS11 Master - Release 2026.2.14.05

## Features
- **DS18B20 Temperature Sensor Integration**: Complete DS temperature sensor support via MS11-control slave
- **Dual Temperature Display**: OLED shows MST (master AHT10) and SLV (slave DS18B20) temperatures simultaneously
- **Slave Firmware Version Display**: OLED IP/version page now displays MS11-slave firmware version

## Changes
- Added slave firmware version display on OLED version page (reads from SlaveController)
- Format: `sl-YYYY.M.m.p` displayed below filesystem version

## Improvements
- OLED version page layout:
  - Line 0: IP address
  - Line 2: fw-YYYY.M.m.p (master firmware)
  - Line 3: fs-YYYY.M.m.p (master filesystem)
  - Line 4: sl-YYYY.M.m.p (slave firmware version from MS11-control)

## Technical Details
- Uses `SlaveController::getFullVersionString()` to retrieve slave firmware version
- Reads from I2C slave at address 0x30
- Version format: YYYY (year) . M (major) . m (minor) . p (patch)

## Files
- `fw-2026.2.14.05.bin` - Main firmware (1.3M)
- `fs-2026.2.14.05.bin` - LittleFS filesystem (1.9M)

## Installation
1. Flash firmware: `pio run -e esp32s3dev -t upload` (serial) or OTA
2. Upload filesystem: `pio run -e esp32s3dev -t uploadfs` or `./uploadfs_with_backup.sh <ESP_IP>`

## Notes
- This release adds visibility into MS11-slave firmware version on the master device
- All debug Serial output remains removed for clean operation
- Temperature reading and display unchanged from v2026.2.14.04
