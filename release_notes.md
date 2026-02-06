### Fixed
- **GPIO Viewer Button Alignment**: Fixed button alignment to match other action buttons
  - Wrapped checkbox and button in `form-group-checkbox-row` div
  - "Open" button now right-aligned like "Reset WiFi" and "Update" buttons
  - Consistent layout across all checkbox rows with action buttons

### Technical Details
- HTML structure updated: GPIO Viewer checkbox now uses same pattern as DHCP checkbox
- CSS classes: `form-group-checkbox-row` + `form-group-checkbox-left` for proper flexbox layout
- Firmware: 1,333,968 bytes (67.8% flash)
- RAM: 51,588 bytes (15.7%)

### Installation
1. Download both `fw-2026.1.1.08.bin` and `fs-2026.1.1.08.bin`
2. Use Update page in web interface or serial upload via PlatformIO
3. For serial upload: `pio run -e esp32s3dev -t upload && pio run -e esp32s3dev -t uploadfs`
