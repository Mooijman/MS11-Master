#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
// Generic helper functions used across multiple modules.

// Initialize LittleFS with project-specific configuration
void initLittleFS();

// Escape a string for safe embedding in JSON output
String jsonEscape(const String& str);

// Universal blink state helper
// Returns true when "on", false when "off" within a repeating cycle.
// onMs  = how long the visible/on phase lasts
// offMs = how long the hidden/off phase lasts
// Cycle is synced to millis() so all callers with the same parameters blink in unison.
// Example: blinkState(millis(), 600, 400)  →  600ms on, 400ms off (1s cycle)
bool blinkState(unsigned long currentMillis, unsigned long onMs, unsigned long offMs);

#endif // UTILS_H
