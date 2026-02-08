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
- Replace all user-facing references of "Arduino" with "MS11-control3"
- Keep technical I2C protocol references unchanged
- Update UI text, messages, and documentation

**Files to modify**:
- `data/i2cdemo.html` - UI labels and button text
- `data/twiboot.html` - Page title and descriptions
- `src/main.cpp` - API endpoint messages (success/error messages)
- Any other user-facing text

**Note**: Do NOT change technical terms like "Arduino bootloader protocol", "Arduino firmware", etc. in code comments or technical documentation.

---

## Notes

- This file tracks project enhancements and feature requests
- Mark completed tasks by moving them to "Completed Tasks" section
- Add new tasks as needed

