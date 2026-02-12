#ifndef NTP_MANAGER_H
#define NTP_MANAGER_H

#include <Arduino.h>

// ============================================================================
// NTP TIME SYNCHRONIZATION
// ============================================================================
// Handles NTP time sync, timezone parsing, and stored-date fallback.

// Synchronize system clock via NTP (if enabled in settings).
// isBootSync = true on first call after boot (saves boot time to NVS).
void syncTimeIfEnabled(bool isBootSync = false);

// Parse timezone offset string (e.g., "UTC+2" -> 2, "UTC-5" -> -5, "UTC" -> 0)
int parseTimezoneOffset(const String& timezone);

#endif // NTP_MANAGER_H
