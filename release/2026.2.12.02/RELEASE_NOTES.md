# Release v2026.2.12.02 - LCD Display Improvements

## Changed
- **Startup Blink**: "Starting up..." now properly blinks during setup sequence
  - Added `delayWithBlink()` helper to enable LCD updates during blocking operations
  - Blink works throughout entire 7-second OLED animation phase
  - Uses 600ms on / 400ms off pattern for smooth visual feedback

- **MS11 Reconnect Logic**: Improved heartbeat and connection monitoring
  - Reduced heartbeat interval from 4s to 2s for faster reconnect detection
  - Automatic reconnection attempts every 2 seconds when MS11-control absent
  - Connection status messages now properly sequenced

## Fixed
- **Connection Lost Display**: Enhanced visual feedback for MS11-control status
  - "Connection lost!" now blinks (600ms/400ms) when MS11-control disconnects
  - Time display stops automatically when connection lost
  - "Restored" message shows for full 3 seconds on reconnection (clock blocked)
  - Automatic return to "Ready." + clock after restoration message

## Technical Details
- State machine variables: `ms11ConnectionLost`, `ms11Restored`, `ms11RestoredTime`
- Clock display now checks `!ms11Restored` flag to prevent premature updates
- Reconnection includes 500ms LED pulse for visual confirmation

---

**Installation**: Flash both firmware and filesystem binaries
**Compatibility**: XIAO ESP32-S3, dual I2C bus configuration
