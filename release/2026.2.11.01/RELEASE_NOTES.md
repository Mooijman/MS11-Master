# Release 2026.2.11.01

## Fixed
- **LCD Update Messages**: Restored "Updating..." display during OTA updates
  - Line 0: "Updating..."
  - Line 1: Shows update type ("Firmware" or "Filesystem")
  - Added 50ms delays after clear() and printLine() for LCD stability
  - Debug logging added for troubleshooting

- **OLED Update Display**: Improved font consistency during OTA updates
  - All messages now use small font (ArialMT_Plain_10) only
  - Better vertical spacing (y=0, 15, 30 instead of mixed positions)
  - Consistent layout across all update stages:
    - "Updating FW/FS"
    - "Please Wait..."
    - "DO NOT POWER OFF"
    - "Update done" / "Rebooting..."

## Technical Details
- LCD updates properly cleared before writing new text
- OLED uses consistent 10pt font throughout update process
- Added Serial debug output for LCD initialization status
