# TODO List - MS11-Master

## Pending Tasks

### 1. Add 'Open' button for HTML files in file manager
**Status**: Not started  
**Priority**: Medium

**Description**:
- In `data/files.html`, add an 'Open' button next to the 'View' button
- Only show for `.html` files (index.html, settings.html, etc.)
- Button should open the HTML file in a new window/tab
- Similar behavior to GPIO Viewer 'Open' button
- Use target="_blank" for new tab/window

**Files to modify**:
- `data/files.html` - Add button in file list item actions

---

### 2. Rename 'Arduino' to 'MS11-control' throughout project
**Status**: Not started  
**Priority**: Medium

**Description**:
- Replace all user-facing references of "Arduino" with "MS11-control"
- Keep technical I2C protocol references unchanged
- Update UI text, messages, and documentation

**Files to modify**:
- `data/i2cdemo.html` - UI labels and button text
- `data/twiboot.html` - Page title and descriptions
- `src/main.cpp` - API endpoint messages (success/error messages)
- Any other user-facing text

**Note**: Do NOT change technical terms like "Arduino bootloader protocol", "Arduino firmware", etc. in code comments or technical documentation.

---

### 3. Add real-time clock display on LCD
**Status**: ✅ Completed  
**Priority**: Medium

**Description**:
- Add a running clock display beneath the "Ready..." status text on the LCD (16x2)
- Format: `HH:MM  DD-MM-YY` (24-hour format)
- Only display after successful NTP time synchronization
- Clock should update in real-time (every minute or second)
- Use timezone setting from NVS for correct local time

**Implementation**: Real-time clock now displays on LCD line 1 after IP display, updates every second with local time based on timezone setting.

**Files to modify**:
- `src/main.cpp` - Update LCD display logic in ready state
- Check `ntpEnabled` setting and time sync status
- Use system time functions with timezone offset

**Requirements**:
- Hide clock display if NTP is disabled or sync failed
- Show only when NTP time is valid
- Display on second line of LCD (line 1)

---

## Notes

- This file tracks project enhancements and feature requests
- Mark completed tasks by moving them to "Completed Tasks" section
- Add new tasks as needed

