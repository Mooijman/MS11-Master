# Release 2026.2.10.03

## Added
- AHT10 Temperature & Humidity Sensor support
  - I2C address 0x38 on Bus 1 (Display bus)
  - Continuous display of temperature (°C) and humidity (%) on OLED
  - Sensor reads every 30 seconds to conserve power
  - Initial reading on startup for immediate valid data
  - Device identification in I2C diagnostics

## Changed
- OLED display now shows temperature and humidity continuously
  - Format: "22.5°C  45%" in small font (ArialMT_Plain_10)
  - Position: Top left (0, 0) - same as IP address display
  - Updates every 1 second
- Removed encoder counter display (temporarily disabled)

## Fixed
- Display updates now blocked during OTA firmware/filesystem updates
  - `otaUpdateInProgress` flag prevents temperature display during updates
  - Flag automatically resets on update errors
  - Ensures clean "Updating FW/FS" messages without interference

## Technical Details
- Library: Adafruit AHTX0 v2.0.5
- AHT10Manager singleton pattern with health checks
- Non-blocking sensor operation
- Error handling with fallback to last valid readings
